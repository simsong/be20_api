#include "scanner_config.h"


/************************************
 *** HELP and  option processing  ***
 ************************************/

const std::string scanner_config::NO_INPUT  {"<NO-INPUT>"};
const std::string scanner_config::NO_OUTDIR {"<NO-OUTDIR>"};
const std::string scanner_config::scanner_command::ALL_SCANNERS {"<ALL>"};

void scanner_config::set_config(const std::string &name, const std::string &val)
{
    namevals[name] = val;
}

/* Get the config and build the help strings at the same time! */


/* All routines to get config information call this class. It builds the help usage as part of
 * the configuration being requested (by each scanner). Thus, after each of the scanners
 * query for their arguments, it's possible to dump the help stream and find eveyrthing that they were looking for.
 */
template<>
void scanner_config::get_config(const std::string &name, std::string *val,const std::string &help)
{
    std::stringstream ss;
    ss << "   -S " << name << "=" << *val << "    " << help << " (" << name << ")\n";
    help_str += ss.str();               // add the help in

    auto it = namevals.find(name);
    if ( it != namevals.end() && val){
        *val = it->second;
    }
}

/* signed/unsigned char need cast to larger type for <<
 * Otherwise it is interpreted as a character.
 */
template<>
void scanner_config::get_config(const std::string &n,unsigned char *val_,const std::string &help)
{
    unsigned int val = *val_;
    get_config(n, &val, help);
    *val_ = (unsigned char)val;
}
template<>
void scanner_config::get_config(const std::string &n,signed char *val_,const std::string &help)
{
    int val = *val_;
    get_config(n, &val, help);
    *val_ = (signed char)val;
}

/* bool needs special processing for YES/NO/TRUE/FALSE */
template<>
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

void scanner_config::push_scanner_command(const std::string &scannerName, scanner_command::command_t c)
{
    scanner_commands.push_back(scanner_command(scannerName,c));
}
