/*
 * scanner_config.h:
 *
 * class to hold name=value configurations for all of the scanners.
 */

/* Config is a set of name=value pairs from the command line and the list of all scanners that
 * are enabled or disabled.
 * All of the scanners get the same config, so the names that the scanners want need to be unique.
 * We could have adopted a system where each scanner had its own configuraiton space, but we didn't.
 * Scanner histograms are added to 'histograms' by machinery.
 */

#ifndef _SCANNER_CONFIG_H_
#define _SCANNER_CONFIG_H_

#include <cinttypes>
#include <map>
#include <string>
#include <sstream>
#include <vector>

struct  scanner_config {
    typedef std::map<std::string,std::string>  config_t ; // configuration for scanner passed in
    config_t  namevals {};             //  (input) name=val map
    std::string help_str {};          // help string that is built
    //std::stringstream helpstream{};
    //static std::string MAX_DEPTH;
    //static unsigned int default_max_depth;
    virtual ~scanner_config(){};
    scanner_config(){};
    scanner_config( const scanner_config &) = default;
    std::string input_fname {};         // where input comes from
    std::string outdir {};              // where output goes
    std::string hash_alg {};            // which hash algorithm are se using
    std::string help() { return help_str; };
    static const std::string   NO_INPUT;  // 'filename' indicator that the FRS has no input file
    static const std::string   NO_OUTDIR; // 'dirname' indicator that the FRS produces no file output

    // These methods are implemented in the plugin system for the scanner to get config information.
    // which is why they need to be virtual functions.
    // The get_config methods should be called on the si object during PHASE_STARTUP
    //virtual void get_config(const scanner_info::config_t &c, const std::string &name,std::string *val,const std::string &help);
    virtual void set_config( const std::string &name, const std::string &val);
    virtual void get_config( const std::string &name, std::string *val, const std::string &help);
    virtual void get_config( const std::string &name, uint64_t *val,    const std::string &help);
    virtual void get_config( const std::string &name, int32_t *val,     const std::string &help);
    virtual void get_config( const std::string &name, uint32_t *val,    const std::string &help);
    virtual void get_config( const std::string &name, uint16_t *val,    const std::string &help);
    virtual void get_config( const std::string &name, uint8_t *val,     const std::string &help);
#ifdef __clang__
    virtual void get_config( const std::string &name, size_t *val,      const std::string &help);
#define HAVE_GET_CONFIG_SIZE_T
#endif
    virtual void get_config( const std::string &name, bool *val,        const std::string &help);
    //virtual int max_depth() const;

    /**
     * Commands whether to enable or disable a scanner.
     * Typically created from parsing command-line arguments
     */
    struct scanner_command {
        static const std::string ALL_SCANNERS;
        enum command_t {DISABLE,ENABLE};
        scanner_command(const scanner_command &sc):scannerName(sc.scannerName),command(sc.command){};
        scanner_command(const std::string &scannerName_,scanner_command::command_t c):scannerName(scannerName_),command(c){};
        const std::string scannerName  {};
        const command_t command {};
    };

    // The commands for those scanners (enable, disable, options, etc.
    typedef std::vector<struct scanner_command> scanner_commands_t;
    scanner_commands_t scanner_commands {};

    /* Control which scanners are enabled */
    void    push_scanner_command(const std::string &scannerName, scanner_command::command_t c); // enable/disable a specific scanner
};

#endif
