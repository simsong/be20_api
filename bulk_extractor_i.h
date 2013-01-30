/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef BULK_EXTRACTOR_I_H
#define BULK_EXTRACTOR_I_H

/* We need netinet/in.h or windowsx.h */
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef WIN32
#  include <winsock2.h>
#  include <windows.h>
#  include <windowsx.h>
#endif

/* If byte_order hasn't been defined, assume its intel */

#if defined(WIN32) || !defined(__BYTE_ORDER)
#  define __LITTLE_ENDIAN 1234
#  define __BIG_ENDIAN    4321
#  define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#if (__BYTE_ORDER == __LITTLE_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#  error Invalid __BYTE_ORDER
#endif

/**
 * \addtogroup plugin_module
 * @{
 */

/**
 * \file
 * bulk_extractor scanner plug_in architecture.
 *
 * Scanners are called with two parameters:
 * A reference to a scanner_params (SP) object.
 * A reference to a recursion_control_block (RCB) object.
 * 
 * On startup, each scanner is called with a special SP and RCB.
 * The scanners respond by setting fields in the SP and returning.
 * 
 * When executing, once again each scanner is called with the SP and RCB.
 * By design, this file can be read without reading config.h
 * This is the only file that needs to be included for a scanner.
 *
 * \li \c phase_startup - scanners are loaded and register the names of the feature files they want.
 * \li \c phase_scan - each scanner is called to analyze 1 or more sbufs.
 * \li \c phase_shutdown - scanners are given a chance to shutdown
 */

#ifndef __cplusplus
# error bulk_extractor_i.h requires C++
#endif

#include "sbuf.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "xml.h"
#include "utf8.h"

#include <vector>
#include <set>
#include <map>

/* bulkd extractor configuration */

typedef std::map<std::string,std::string>  be_config_t;
extern be_config_t be_config;           // system configuration


/* Network includes */

/****************************************************************
 *** pcap.h --- If we don't have it, fake it. ---
 ***/
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
#ifdef HAVE_NETINET_TCP_H
// the BSD flavor of tcphdr is the one used elsewhere in the code
# define __FAVOR_BSD
# include <netinet/tcp.h>
# undef __FAVOR_BSD
#endif
#ifdef HAVE_NETINET_IF_ETHER_H
# include <netinet/if_ether.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NET_ETHERNET_H
# include <net/ethernet.h>		// for freebsd
#endif


#if defined(HAVE_LIBPCAP)
#  ifdef GNUC_HAS_DIAGNOSTIC_PRAGMA
#    pragma GCC diagnostic ignored "-Wredundant-decls"
#  endif
#  ifdef HAVE_PCAP_PCAP_H
#    include <pcap/pcap.h>
#  else
#    include <pcap.h>
#  endif
#  ifdef GNUC_HAS_DIAGNOSTIC_PRAGMA
#    pragma GCC diagnostic warning "-Wredundant-decls"
#  endif
#else
#  include "pcap_fake.h"
#endif

/**
 * \class scanner_params
 * The scanner params class is the primary way that the bulk_extractor framework
 * communicates with the scanners. 
 * @param sbuf - the buffer to be scanned
 * @param feature_names - if fs==0, add to feature_names the feature file types that this
 *                        scanner records.. The names can have a /c appended to indicate
 *                        that the feature files should have context enabled. Do not scan.
 * @param fs   - where the features should be saved. Must be provided if feature_names==0.
 **/

class histogram_def {
 public:
    /**
     * @param feature- the feature file to histogram (no .txt)
     * @param re     - the regular expression to extract
     * @param require- require this string on the line (usually in context)
     * @param suffix - the suffix to add to the histogram file after feature name before .txt
     * @param flags  - any flags (see above)
     */

    histogram_def(string feature_,string re_,string suffix_,uint32_t flags_=0):
        feature(feature_),pattern(re_),require(),suffix(suffix_),flags(flags_){}
    histogram_def(string feature_,string re_,string require_,string suffix_,uint32_t flags_=0):
        feature(feature_),pattern(re_),require(require_),suffix(suffix_),flags(flags_){}
    string feature;                     /* feature file */
    string pattern;                     /* extract pattern; "" means use entire feature */
    string require;
    string suffix;                      /* suffix to append; "" means "histogram" */
    uint32_t flags;                     // defined in histogram.h
};

typedef  set<histogram_def> histograms_t;

inline bool operator <(class histogram_def h1,class histogram_def h2)  {
    if (h1.feature<h2.feature) return true;
    if (h1.feature>h2.feature) return false;
    if (h1.pattern<h2.pattern) return true;
    if (h1.pattern>h2.pattern) return false;
    if (h1.suffix<h2.suffix) return true;
    if (h1.suffix>h2.suffix) return false;
    return false;                       /* equal */
};

inline bool operator !=(class histogram_def h1,class histogram_def h2)  {
    return h1.feature!=h2.feature || h1.pattern!=h2.pattern || h1.suffix!=h2.suffix;
};



/*****************************************************************
 *** bulk_extractor has a private implementation of IPv4 and IPv6,
 *** UDP and TCP. 
 ***
 *** We did this becuase we found slightly different versions on
 *** MacOS, Ubuntu Linux, Fedora Linux, Centos, Mingw, and Cygwin.
 *** TCP/IP isn't changing anytime soon, and when it changes (as it
 *** did with IPv6), these different systems all implemented it slightly
 *** differently, and that caused a lot of problems for us.
 *** So the BE13 API has a single implementation and it's good enough
 *** for our uses.
 ***/

namespace be13 {

#ifndef ETH_ALEN
#  define ETH_ALEN 6			// ethernet address len
#endif

#ifndef IPPROTO_TCP
#  define IPPROTO_TCP     6               /* tcp */
#endif

    struct ether_addr {
        uint8_t ether_addr_octet[ETH_ALEN];
    } __attribute__ ((__packed__));

    /* 10Mb/s ethernet header */
    struct ether_header {
        uint8_t  ether_dhost[ETH_ALEN];	/* destination eth addr	*/
        uint8_t  ether_shost[ETH_ALEN];	/* source ether addr	*/
        uint16_t ether_type;		        /* packet type ID field	*/
    } __attribute__ ((__packed__));

    // The mess below is becuase these items are typedefs and
    // structs on some systems and #defines on other systems
    // So in the interest of portability we need to define *new*
    // structures that are only used here
    typedef uint32_t ip4_addr_t;         // historical
    // on windows we use the definition that's in winsock
    struct ip4_addr {   
	ip4_addr_t addr;
    };

  /*
   * Structure of an internet header, naked of options.
   */
    struct ip4 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        uint8_t ip_hl:4;		/* header length */
        uint8_t ip_v:4;		/* version */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
        uint8_t ip_v:4;		/* version */
        uint8_t ip_hl:4;		/* header length */
#endif
        uint8_t ip_tos;			/* type of service */
        uint16_t ip_len;			/* total length */
        uint16_t ip_id;			/* identification */
        uint16_t ip_off;			/* fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
        uint8_t ip_ttl;			/* time to live */
        uint8_t ip_p;			/* protocol */
        uint16_t ip_sum;			/* checksum */
        struct ip4_addr ip_src, ip_dst;	/* source and dest address */
    } __attribute__ ((__packed__));

    struct ip4_dgram {
        const struct ip4 *header;
        const uint8_t *payload;
        uint16_t payload_len;
    };

    /*
     * IPv6 header structure
     */
    struct ip6_addr {		// our own private ipv6 definition
        union {
            uint8_t   __u6_addr8[16];
            uint16_t  __u6_addr16[8];
            uint32_t  __u6_addr32[4];
        } __u6_addr;                    /* 128-bit IP6 address */
    };
    struct ip6_hdr {
        union {
            struct ip6_hdrctl {
                uint32_t ip6_un1_flow;	/* 20 bits of flow-ID */
                uint16_t ip6_un1_plen;	/* payload length */
                uint8_t  ip6_un1_nxt;	/* next header */
                uint8_t  ip6_un1_hlim;	/* hop limit */
            } ip6_un1;
            uint8_t ip6_un2_vfc;	/* 4 bits version, top 4 bits class */
        } ip6_ctlun;
        struct ip6_addr ip6_src;	/* source address */
        struct ip6_addr ip6_dst;	/* destination address */
    } __attribute__((__packed__));

    struct ip6_dgram {
        const struct ip6_hdr *header;
        const uint8_t *payload;
        uint16_t payload_len;
    };

    /*
     * TCP header.
     * Per RFC 793, September, 1981.
     */
    typedef	uint32_t tcp_seq;
    struct tcphdr {
        uint16_t th_sport;		/* source port */
        uint16_t th_dport;		/* destination port */
        tcp_seq th_seq;		/* sequence number */
        tcp_seq th_ack;		/* acknowledgement number */
#  if __BYTE_ORDER == __LITTLE_ENDIAN
        uint8_t th_x2:4;		/* (unused) */
        uint8_t th_off:4;		/* data offset */
#  endif
#  if __BYTE_ORDER == __BIG_ENDIAN
        uint8_t th_off:4;		/* data offset */
        uint8_t th_x2:4;		/* (unused) */
#  endif
        uint8_t th_flags;
#  define TH_FIN	0x01
#  define TH_SYN	0x02
#  define TH_RST	0x04
#  define TH_PUSH	0x08
#  define TH_ACK	0x10
#  define TH_URG	0x20
    uint16_t th_win;		/* window */
    uint16_t th_sum;		/* checksum */
    uint16_t th_urp;		/* urgent pointer */
};
#endif
/*
 * The packet_info structure records packets after they are read from the pcap library.
 * It preserves the original pcap information and information decoded from the MAC and
 * VLAN (IEEE 802.1Q) layers.
 *
 * @param ts   - the actual packet time to use (adjusted)
 * @param pcap_data - Original data offset point from pcap
 * @param data - the actual packet data, minus the MAC layer
 * @param datalen - How much data is available at the datalen pointer
 * 
 */
class packet_info {
public:
    // IPv4 header offsets
    static const size_t ip4_proto_off = 9;
    static const size_t ip4_src_off = 12;
    static const size_t ip4_dst_off = 16;
    // IPv6 header offsets
    static const size_t ip6_nxt_hdr_off = 6;
    static const size_t ip6_plen_off = 4;
    static const size_t ip6_src_off = 8;
    static const size_t ip6_dst_off = 24;
    // TCP header offsets
    static const size_t tcp_sport_off = 0;
    static const size_t tcp_dport_off = 2;

    class frame_too_short : public std::logic_error {
    public:
        frame_too_short() :
            std::logic_error("frame too short to contain requisite network structures") {}
    };

    enum vlan_t {NO_VLAN=-1};
    packet_info(const int dlt,const struct pcap_pkthdr *h,const u_char *d,
                const struct timeval &ts_,const uint8_t *d2,size_t dl2):
        pcap_dlt(dlt),pcap_hdr(h),pcap_data(d),ts(ts_),ip_data(d2),ip_datalen(dl2){}
    packet_info(const int dlt,const struct pcap_pkthdr *h,const u_char *d):
        pcap_dlt(dlt),pcap_hdr(h),pcap_data(d),ts(h->ts),ip_data(d),ip_datalen(h->caplen){}

    const int    pcap_dlt;              // data link type; needed by libpcap, not provided
    const struct pcap_pkthdr *pcap_hdr; // provided by libpcap
    const u_char *pcap_data;            // provided by libpcap
    const struct timeval &ts;           // possibly modified before packet_info created
    const uint8_t * const ip_data;             // pointer to where ip data begins
    const size_t ip_datalen;            // length of ip data

    int     ip_version() const;                 // returns 4, 6 or 0
    u_short ether_type() const;               // returns 0 if not IEEE802, otherwise returns ether_type
    int     vlan() const; // returns NO_VLAN if not IEEE802 or not VLAN, othererwise VID
    // packet typing
    bool    is_ip4() const;
    bool    is_ip6() const;
    bool    is_ip4_tcp() const;
    bool    is_ip6_tcp() const;
    // packet extraction
    // IPv4
    const struct in_addr *get_ip4_src() const;
    const struct in_addr *get_ip4_dst() const;
    uint8_t get_ip4_proto() const;
    // IPv6
    uint8_t get_ip6_nxt_hdr() const;
    uint16_t get_ip6_plen() const;
    const struct private_in6_addr *get_ip6_src() const;
    const struct private_in6_addr *get_ip6_dst() const;
    // TCP
    uint16_t get_ip4_tcp_sport() const;
    uint16_t get_ip4_tcp_dport() const;
    uint16_t get_ip6_tcp_sport() const;
    uint16_t get_ip6_tcp_dport() const;
};

    
#ifdef DLT_IEEE802
    inline u_short packet_info::ether_type() const
    {
        if(pcap_dlt==DLT_IEEE802 || pcap_dlt==DLT_EN10MB){
            const struct ether_header *eth_header = (struct ether_header *) pcap_data;
            return ntohs(eth_header->ether_type);
        }
        return 0;
    }
#endif
    
#ifndef ETHERTYPE_PUP
#define	ETHERTYPE_PUP		0x0200          /* Xerox PUP */
#define ETHERTYPE_SPRITE	0x0500		/* Sprite */
#define	ETHERTYPE_IP		0x0800		/* IP */
#define	ETHERTYPE_ARP		0x0806		/* Address resolution */
#define	ETHERTYPE_REVARP	0x8035		/* Reverse ARP */
#define ETHERTYPE_AT		0x809B		/* AppleTalk protocol */
#define ETHERTYPE_AARP		0x80F3		/* AppleTalk ARP */
#define	ETHERTYPE_VLAN		0x8100		/* IEEE 802.1Q VLAN tagging */
#define ETHERTYPE_IPX		0x8137		/* IPX */
#define	ETHERTYPE_IPV6		0x86dd		/* IP protocol version 6 */
#define ETHERTYPE_LOOPBACK	0x9000		/* used to test interfaces */
#endif
    
    inline int packet_info::vlan() const
    {
        if(ether_type()==ETHERTYPE_VLAN){
            return ntohs(*(u_short *)(pcap_data + sizeof(struct ether_header)));
        }
        return -1;
    }
    
    inline int packet_info::ip_version() const
    {
        /* This takes advantage of the fact that ip4 and ip6 put the version number in the same place */
        if (ip_datalen >= sizeof(struct ip4)) {
            const struct ip4 *ip_header = (struct ip4 *) ip_data;
            switch(ip_header->ip_v){
            case 4: return 4;
            case 6: return 6;
            }
        }
        return 0;
    }

    // packet typing

    inline bool packet_info::is_ip4() const
    {
        return ip_version() == 4;
    }

    inline bool packet_info::is_ip6() const
    {
        return ip_version() == 6;
    }

    inline bool packet_info::is_ip4_tcp() const
    {
        if(ip_datalen < sizeof(struct ip) + sizeof(struct tcphdr)) {
            return false;
        }
        return *((uint8_t*) (ip_data + ip4_proto_off)) == IPPROTO_TCP;
        return false;
    }

    inline bool packet_info::is_ip6_tcp() const
    {
        if(ip_datalen < sizeof(struct private_ip6_hdr) + sizeof(struct tcphdr)) {
            return false;
        }
        return *((uint8_t*) (ip_data + ip6_nxt_hdr_off)) == IPPROTO_TCP;
    }

    // packet extraction
    // precondition: the apropriate packet type function must return true before using these functions.
    //     example: is_ip4_tcp() must return true before calling get_ip4_tcp_sport()

    // IPv4
    inline const struct in_addr *packet_info::get_ip4_src() const
    {
        if(ip_datalen < sizeof(struct ip)) {
            throw new frame_too_short;
        }
        return (const struct in_addr *) ip_data + ip4_src_off;
    }
    inline const struct in_addr *packet_info::get_ip4_dst() const
    {
        if(ip_datalen < sizeof(struct ip)) {
            throw new frame_too_short;
        }
        return (const struct in_addr *) ip_data + ip4_dst_off;
    }
    inline uint8_t packet_info::get_ip4_proto() const
    {
        if(ip_datalen < sizeof(struct ip)) {
            throw new frame_too_short;
        }
        return *((uint8_t *) (ip_data + ip4_proto_off));
    }
    // IPv6
    inline uint8_t packet_info::get_ip6_nxt_hdr() const
    {
        if(ip_datalen < sizeof(struct private_ip6_hdr)) {
            throw new frame_too_short;
        }
        return *((uint8_t *) (ip_data + ip6_nxt_hdr_off));
    }
    inline uint16_t packet_info::get_ip6_plen() const
    {
        if(ip_datalen < sizeof(struct private_ip6_hdr)) {
            throw new frame_too_short;
        }
        return ntohs(*((uint16_t *) (ip_data + ip6_plen_off)));
    }
    inline const struct private_in6_addr *packet_info::get_ip6_src() const
    {
        if(ip_datalen < sizeof(struct private_ip6_hdr)) {
            throw new frame_too_short;
        }
        return (const struct private_in6_addr *) ip_data + ip6_src_off;
    }
    inline const struct private_in6_addr *packet_info::get_ip6_dst() const
    {
        if(ip_datalen < sizeof(struct private_ip6_hdr)) {
            throw new frame_too_short;
        }
        return (const struct private_in6_addr *) ip_data + ip6_dst_off;
    }
    // TCP
    inline uint16_t packet_info::get_ip4_tcp_sport() const
    {
        if(ip_datalen < sizeof(struct tcphdr) + sizeof(struct ip)) {
            throw new frame_too_short;
        }
        return ntohs(*((uint16_t *) (ip_data + sizeof(struct ip) + tcp_sport_off)));
    }
    inline uint16_t packet_info::get_ip4_tcp_dport() const
    {
        if(ip_datalen < sizeof(struct tcphdr) + sizeof(struct ip)) {
            throw new frame_too_short;
        }
        return ntohs(*((uint16_t *) (ip_data + sizeof(struct ip) + tcp_dport_off)));
    }
    inline uint16_t packet_info::get_ip6_tcp_sport() const
    {
        if(ip_datalen < sizeof(struct tcphdr) + sizeof(struct private_ip6_hdr)) {
            throw new frame_too_short;
        }
        return ntohs(*((uint16_t *) (ip_data + sizeof(struct private_ip6_hdr) + tcp_sport_off)));
    }
    inline uint16_t packet_info::get_ip6_tcp_dport() const
    {
        if(ip_datalen < sizeof(struct tcphdr) + sizeof(struct private_ip6_hdr)) {
            throw new frame_too_short;
        }
        return ntohs(*((uint16_t *) (ip_data + sizeof(struct private_ip6_hdr) + tcp_dport_off)));
    }
};


typedef void scanner_t(const class scanner_params &sp,const class recursion_control_block &rcb);
typedef void process_t(const class scanner_params &sp); 
typedef void packet_callback_t(void *user,const be13::packet_info &pi);
    
/** scanner_info gets filled in by the scanner to tell the caller about the scanner.
 */
class scanner_info {
private:
    class not_impl: public exception {
        virtual const char *what() const throw() {
            return "copying feature_recorder objects is not implemented.";
        }
    };
    scanner_info(const scanner_info &i) __attribute__((__noreturn__))
    :si_version(),name(),author(),description(),url(),
                                        scanner_version(),flags(),feature_names(),histogram_defs(),
                                        packet_user(),packet_cb(){
        throw new not_impl();}
    ;
    const scanner_info &operator=(const scanner_info &i){ throw new not_impl();}
 public:
    static const int SCANNER_DISABLED=0x01;             /* v1: enabled by default */
    static const int SCANNER_NO_USAGE=0x02;             /* v1: do not show scanner in usage */
    static const int SCANNER_NO_ALL  =0x04;             // v2: do not enable with -eALL
    static const int CURRENT_SI_VERSION=2;

    scanner_info():si_version(CURRENT_SI_VERSION),
                   name(),author(),description(),url(),scanner_version(),flags(0),feature_names(),
                   histogram_defs(),packet_user(),packet_cb(){}
    int         si_version;             // version number for this structure
    string      name;                   // v1: scanner name
    string      author;                 // v1: who wrote me?
    string      description;            // v1: what do I do?
    string      url;                    // v1: where I come from
    string      scanner_version;        // v1: version for the scanner
    uint64_t    flags;                  // v1: flags
    set<string> feature_names;          // v1: features I need
    histograms_t histogram_defs;        // v1: histogram definition info
    void        *packet_user;           // v2: user data provided to packet_cb
    packet_callback_t *packet_cb;       // v2: packet handler, or NULL if not present.
};

#include <map>
class scanner_params {
 public:
    /** Construct a scanner_params from a sbuf and other sensible defaults.
     *
     */
    enum print_mode_t {MODE_NONE=0,MODE_HEX,MODE_RAW,MODE_HTTP};
    static const int CURRENT_SP_VERSION=3;

    typedef std::map<string,string> PrintOptions;
    static print_mode_t getPrintMode(const PrintOptions &po){
        PrintOptions::const_iterator p = po.find("print_mode_t");
        if(p != po.end()){
            if(p->second=="MODE_NONE") return MODE_NONE;
            if(p->second=="MODE_HEX") return MODE_HEX;
            if(p->second=="MODE_RAW") return MODE_RAW;
            if(p->second=="MODE_HTTP") return MODE_HTTP;
        }
        return MODE_NONE;
    }
    static void setPrintMode(PrintOptions &po,int mode){
        switch(mode){
        default:
        case MODE_NONE:po["print_mode_t"]="MODE_NONE";return;
        case MODE_HEX:po["print_mode_t"]="MODE_HEX";return;
        case MODE_RAW:po["print_mode_t"]="MODE_RAW";return;
        case MODE_HTTP:po["print_mode_t"]="MODE_HTTP";return;
        }
    }

    typedef enum {none=-1,startup=0,scan=1,shutdown=2} phase_t ;
    static PrintOptions no_options;     // in common.cpp

    /********************
     *** CONSTRUCTORS ***
     ********************/

    /* A scanner params with all of the instance variables */
    scanner_params(phase_t phase_,const sbuf_t &sbuf_,class feature_recorder_set &fs_,
                   PrintOptions &print_options_):
        sp_version(CURRENT_SP_VERSION),
        phase(phase_),sbuf(sbuf_),fs(fs_),depth(0),print_options(print_options_),info(0),sbufxml(0){
    }

    /* A scanner params with no print options*/
    scanner_params(phase_t phase_,const sbuf_t &sbuf_,class feature_recorder_set &fs_):
        sp_version(CURRENT_SP_VERSION),
        phase(phase_),sbuf(sbuf_),fs(fs_),depth(0),print_options(no_options),info(0),sbufxml(0){
    }

    /* A scanner params with no print options but xmlstream */
    scanner_params(phase_t phase_,const sbuf_t &sbuf_,class feature_recorder_set &fs_,std::stringstream *xmladd):
        sp_version(CURRENT_SP_VERSION),
        phase(phase_),sbuf(sbuf_),fs(fs_),depth(0),print_options(no_options),info(0),sbufxml(xmladd){
    }

    /** Construct a scanner_params for recursion from an existing sp and a new sbuf.
     * Defaults to phase1
     */
    scanner_params(const scanner_params &sp_existing,const sbuf_t &sbuf_new):
        sp_version(CURRENT_SP_VERSION),phase(sp_existing.phase),
        sbuf(sbuf_new),fs(sp_existing.fs),depth(sp_existing.depth+1),
        print_options(sp_existing.print_options),info(0),sbufxml(0){
        assert(sp_existing.sp_version==CURRENT_SP_VERSION);
    };

    /**************************
     *** INSTANCE VARIABLES ***
     **************************/

    int      sp_version;                /* version number of this structure */
    phase_t  phase;                     /* v1: 0=startup, 1=normal, 2=shutdown (changed to phase_t in v1.3) */
    const sbuf_t &sbuf;                 /* v1: what to scan */
    class feature_recorder_set &fs;     /* v1: where to put the results*/
    uint32_t   depth;                   /* v1: how far down are we? */

    PrintOptions  &print_options;       /* v1: how to print */
    scanner_info  *info;                /* v2: get parameters on startup; info's are stored in a the scanner_def vector */
    std::stringstream *sbufxml;         /* v3: tags added to the sbuf's XML stream */
};


inline std::ostream & operator <<(std::ostream &os,const class scanner_params &sp){
    os << "scanner_params(" << sp.sbuf << ")";
    return os;
};

class recursion_control_block {
 public:
/**
 * @param callback_ - the function to call back
 * @param partName_ - the part of the forensic path processed by this scanner.
 * @param raf       - return after free -- don't call *callback_
 */
 recursion_control_block(process_t *callback_,string partName_,bool raf):
    callback(callback_),partName(partName_),returnAfterFound(raf){}
    process_t *callback;
    string partName;            /* eg "ZIP", "GZIP" */
    bool returnAfterFound;      /* only run once */
};
    
/* plugin.cpp. This will become a class...  */
class scanner_def {
public:;
    static uint32_t max_depth;
    scanner_def():scanner(0),enabled(false),info(),pathPrefix(){};
    scanner_t  *scanner;                // pointer to the primary entry point
    bool        enabled;                // is enabled?
    scanner_info info;
    string      pathPrefix;             /* path prefix for recursive scanners */
};
void load_scanner(scanner_t scanner);
void load_scanners(scanner_t * const *scanners);                // load the scan_ plugins
void load_scanner_directory(const string &dirname);             // load the scan_ plugins
typedef vector<scanner_def *> scanner_vector;
extern scanner_vector current_scanners;                         // current scanners
void enable_alert_recorder(feature_file_names_t &feature_file_names);
void enable_feature_recorders(feature_file_names_t &feature_file_names);
// print info about the scanners:
void info_scanners(bool detailed,scanner_t * const *scanners_builtin,const char enable_opt,const char disable_opt);
void scanners_enable(const std::string &name); // saves a command to enable this scanner
void scanners_enable_all();                    // enable all of them
void scanners_disable(const std::string &name); // saves a command to disable this scanner
void scanners_disable_all();                    // saves a command to disable all
void scanners_process_commands();               // process the saved commands

/* plugin.cpp */
void phase_shutdown(feature_recorder_set &fs, xml &xreport);
void phase_histogram(feature_recorder_set &fs, xml &xreport);
void process_sbuf(const class scanner_params &sp);                              /* process for feature extraction */
void process_packet_info(const be13::packet_info &pi);


inline std::string itos(int i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string dtos(double d){ std::stringstream ss; ss << d;return ss.str();}
inline std::string utos(unsigned int i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string utos(uint64_t i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string utos(uint16_t i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string safe_utf16to8(std::wstring s){ // needs to be cleaned up
    std::string utf8_line;
    try {
        utf8::utf16to8(s.begin(),s.end(),back_inserter(utf8_line));
    } catch(utf8::invalid_utf16){
        /* Exception thrown: bad UTF16 encoding */
        utf8_line = "";
    }
    return utf8_line;
}


#ifndef HAVE_ISXDIGIT
inline int isxdigit(int c)
{
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}
#endif

/* Useful functions for scanners */
#define ONE_HUNDRED_NANO_SEC_TO_SECONDS 10000000
#define SECONDS_BETWEEN_WIN32_EPOCH_AND_UNIX_EPOCH 11644473600LL
/*
 * 11644473600 is the number of seconds between the Win32 epoch
 * and the Unix epoch.
 *
 * http://arstechnica.com/civis/viewtopic.php?f=20&t=111992
 * gmtime_r() is Linux-specific. You'll find a copy in util.cpp for Windows.
 */

#ifndef HAVE_GMTIME_R
void gmtime_r(time_t *t,struct tm *tm);
#endif

inline std::string microsoftDateToISODate(const uint64_t &time)
{
    time_t tmp = (time / ONE_HUNDRED_NANO_SEC_TO_SECONDS) - SECONDS_BETWEEN_WIN32_EPOCH_AND_UNIX_EPOCH;
    
    struct tm time_tm;
    gmtime_r(&tmp, &time_tm);
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &time_tm); // Zulu time
    return string(buf);
}

#endif
