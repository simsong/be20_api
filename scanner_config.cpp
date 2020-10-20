#include "scanner_config.h"


/************************************
 *** HELP and  option processing  ***
 ************************************/

/* Get the config and build the help strings at the same time! */

std::stringstream scanner_info::helpstream;
void scanner_info::get_config(const scanner_info::config_t &c,
                              const std::string &n,std::string *val,const std::string &help)
{
    /* Check to see if we are being called as part of a help operation */
    helpstream << "   -S " << n << "=" << *val << "    " << help << " (" << name << ")\n";
    scanner_info::config_t::const_iterator it = c.find(n);
    if(it!=c.end() && val){
        *val = it->second;
    }
}

void scanner_info::get_config(const std::string &n,
                                    std::string *val,const std::string &help)
{
    scanner_info::get_config(config->namevals,n,val,help);
}

/* Should this be redone with templates? */
#define GET_CONFIG(T) void scanner_info::get_config(const std::string &n,T *val,const std::string &help) { \
        std::stringstream ss;\
        ss << *val;\
        std::string v(ss.str());\
        get_config(n,&v,help);\
        ss.str(v);\
        ss >> *val;\
    }

GET_CONFIG(uint64_t)
GET_CONFIG(int32_t)                     // both int32_t and uint32_t
GET_CONFIG(uint32_t)
GET_CONFIG(uint16_t)
#ifdef HAVE_GET_CONFIG_SIZE_T
GET_CONFIG(size_t)
#endif


/* uint8_t needs cast to uint32_t for <<
 * Otherwise it is interpreted as a character.
 */
void scanner_info::get_config(const std::string &n,uint8_t *val_,const std::string &help)
{
    uint32_t val = *val_;
    std::stringstream ss;
    ss << val;
    std::string v(ss.str());
    get_config(n,&v,help);
    ss.str(v);
    ss >> val;
    *val_ = (uint8_t)val;
}

/* bool needs special processing for YES/NO/TRUE/FALSE */
void scanner_info::get_config(const std::string &n,bool *val,const std::string &help)
{
    std::stringstream ss;
    ss << ((*val) ? "YES" : "NO");
    std::string v(ss.str());
    get_config(n,&v,help);
    switch(v.at(0)){
    case 'Y':case 'y':case 'T':case 't':case '1':
        *val = true;
        break;
    default:
        *val = false;
    }
}
