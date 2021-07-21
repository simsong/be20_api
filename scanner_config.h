/*
 * scanner_config.h:
 *
 * class to hold the full configuration of the scanner_set and the feature recorders.
 *
 * Includes a set of name=value pairs from the command line and the list of all scanners that
 * are enabled or disabled.
 *
 * This class is also used to build the help string.
 *
 * All of the scanners get the same config, so the names that the scanners want need to be unique.
 * We could have adopted a system where each scanner had its own configuraiton space, but we didn't.
 * Scanner histograms are added to 'histograms' by machinery.
 */

#ifndef _SCANNER_CONFIG_H_
#define _SCANNER_CONFIG_H_

#include <cinttypes>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct scanner_config {

    typedef std::map<std::string, std::string> config_t; // configuration for scanner passed in
    config_t namevals{};                                 //  (input) name=val map
    std::string help_str{};                              // help string that is built

    size_t context_window_default{16}; // global option
    uint64_t offset_add{0}; // add this number to the first offset in every feature file (used for parallelism)
    std::filesystem::path banner_file{}; // add the contents of this file to the top of every feature file

    static inline const u_int DEFAULT_MAX_DEPTH {12};
    virtual ~scanner_config(){};
    scanner_config(){};
    scanner_config(const scanner_config&) = default;
    std::filesystem::path input_fname{NO_INPUT}; // where input comes from
    std::filesystem::path outdir{NO_OUTDIR};     // where output goes
    std::string hash_algorithm{"sha1"};          // which hash algorithm are using; default to SHA1
    std::string help() { return help_str; };
    inline static const std::string NO_INPUT = "<NO-INPUT>"; // 'filename' indicator that the FRS has no input file
    inline static const std::string NO_OUTDIR =
        "<NO-OUTDIR>"; // 'dirname' indicator that the FRS produces no file output

    // These methods are implemented in the plugin system for the scanner to get config information.
    // which is why they need to be virtual functions.
    // The get_config methods should be called on the si object during PHASE_STARTUP
    // void get_config(const scanner_info::config_t &c, const std::string &name,std::string *val,const std::string
    // &help);
    void set_config(const std::string& name, const std::string& val);
    template <typename T> void get_config(const std::string& name, T* val, const std::string& help);
    u_int max_depth {DEFAULT_MAX_DEPTH};

    /**
     * Commands whether to enable or disable a scanner.
     * Typically created from parsing command-line arguments
     */
    struct scanner_command {
        static inline const std::string ALL_SCANNERS = "<ALL-SCANNERS>";
        enum command_t { DISABLE, ENABLE };
        scanner_command(const scanner_command& sc) : scannerName(sc.scannerName), command(sc.command){};
        scanner_command(const std::string& scannerName_, scanner_command::command_t c)
            : scannerName(scannerName_), command(c){};
        std::string scannerName{};
        command_t command{};
        /* default copy construction and assignment */
        scanner_command& operator=(const scanner_command& a) {
            this->scannerName = a.scannerName;
            this->command = a.command;
            return *this;
        }
    };

    // The commands for those scanners (enable, disable, options, etc.
    typedef std::vector<struct scanner_command> scanner_commands_t;
    scanner_commands_t scanner_commands{};

    /* Control which scanners are enabled */
    void push_scanner_command(const std::string& scannerName,
                              scanner_command::command_t c); // enable/disable a specific scanner
};

template <> void scanner_config::get_config(const std::string& name, std::string* val, const std::string& help);
template <> void scanner_config::get_config(const std::string& name, signed char* val, const std::string& help);
template <> void scanner_config::get_config(const std::string& name, unsigned char* val, const std::string& help);
template <> void scanner_config::get_config(const std::string& name, bool* val, const std::string& help);

template <typename T> void scanner_config::get_config(const std::string& n, T* val, const std::string& help) {
    std::stringstream ss;
    ss << *val;
    std::string v(ss.str());
    get_config(n, &v, help);
    ss.str(v);
    ss >> *val;
}

#endif
