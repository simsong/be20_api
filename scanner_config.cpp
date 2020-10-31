#include "scanner_config.h"


/************************************
 *** HELP and  option processing  ***
 ************************************/

void scanner_config::set_config(const std::string &name, const std::string &val)
{
    namevals[name] = val;
}

/* Get the config and build the help strings at the same time! */


/* All routines to get config information call this class. It builds the help usage as part of
 * the configuration being requested (by each scanner). Thus, after each of the scanners
 * query for their arguments, it's possible to dump the help stream and find eveyrthing that they were looking for.
 */
void scanner_config::get_config(const std::string &name, std::string *val,const std::string &help)
{
    std::stringstream ss;
    ss << "   -S " << name << "=" << *val << "    " << help << " (" << name << ")\n";
    help_str += ss.str();

    auto it = namevals.find(name);
    if ( it != namevals.end() && val){
        *val = it->second;
    }
}

/* Should this be redone with templates? */
#define GET_CONFIG(T) void scanner_config::get_config(const std::string &n,T *val,const std::string &help) { \
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
void scanner_config::get_config(const std::string &n,uint8_t *val_,const std::string &help)
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
void scanner_config::get_config(const std::string &n,bool *val,const std::string &help)
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
