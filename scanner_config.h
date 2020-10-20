/*
 * scanner_config.h:
 *
 * class to hold name=value configurations for all of the scanners.
 */


/* Config is a set of name=value pairs from the command line.
 * All of the scanners get the same config, so the names that the scanners want need to be unique.
 * We could have adopted a system where each scanner had its own configuraiton space, but we didn't.
 * Scanner histograms are added to 'histograms' by machinery.
 */

#ifndef _SCANNER_CONFIG_H_
#define _SCANNER_CONFIG_H_

#include <cinttypes>
#include <map>
#include <string>

struct scanner_config {
    typedef std::map<std::string,std::string>  config_t ; // configuration for scanner passed in
    virtual ~scanner_config(){};
    scanner_config(){};
    config_t  namevals{};             //  (input) name=val map
    int       debug{};                //  (input) current debug level

    // These methods are implemented in the plugin system for the scanner to get config information.
    // The get_config methods should be called on the si object during PHASE_STARTUP
    //virtual void get_config(const scanner_info::config_t &c, const std::string &name,std::string *val,const std::string &help);
    virtual void get_config(const std::string &name, std::string *val, const std::string &help);
    virtual void get_config(const std::string &name, uint64_t *val,    const std::string &help);
    virtual void get_config(const std::string &name, int32_t *val,     const std::string &help);
    virtual void get_config(const std::string &name, uint32_t *val,    const std::string &help);
    virtual void get_config(const std::string &name, uint16_t *val,    const std::string &help);
    virtual void get_config(const std::string &name, uint8_t *val,     const std::string &help);
#ifdef __APPLE__
    virtual void get_config(const std::string &name, size_t *val,      const std::string &help);
#define HAVE_GET_CONFIG_SIZE_T
#endif
    virtual void get_config(const std::string &name, bool *val,        const std::string &help);
};

#endif
