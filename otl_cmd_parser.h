//
// Created by hsyuan on 2019-03-21.
//

#ifndef FACEDEMOEXAMPLE_OTL_CMD_PARSER_H
#define FACEDEMOEXAMPLE_OTL_CMD_PARSER_H
#include <vector>
#include <map>
#include "otl_string.h"

class CommandLineParser {
    int argc_;
    char** argv_;
    // The vector of passed command line arguments.
    std::vector<std::string> args_;
    // The map of the flag names/values.
    std::map<std::string, std::string> flags_;
    // The usage message.
    std::map<std::string, std::string> usage_messages_;
    std::map<std::string, int> simple_flags_;

    // Returns whether the passed flag is standalone or not. By standalone we
    // understand e.g. --standalone (in contrast to --non_standalone=1).
    bool IsStandaloneFlag(std::string flag){
        return flag.find('=') == std::string::npos;
    }

    // Checks whether the flag is in the format --flag_name=flag_value.
    // or just --flag_name.
    bool IsFlagWellFormed(std::string flag){
        size_t dash_pos = flag.find("--");
        size_t equal_pos = flag.find('=');
        if (dash_pos != 0) {
            std::cout << "Wrong switch format:" << flag << std::endl;
            std::cout << "Flag doesn't start with --" << std::endl;
            return false;
        }

        size_t flag_length=flag.length()-1;
        // We use 3 here because we assume that the flags are in the format
        // --flag_name=flag_value, thus -- are at positions 0 and 1 and we should have
        // at least one symbol for the flag name.
        if (equal_pos > 0 && (equal_pos < 3 || equal_pos == flag_length)) {
            fprintf(stderr, "Wrong switch format: %s\n", flag.c_str());
            fprintf(stderr, "Wrong placement of =\n");
            return false;
        }
        return true;
    }

    // Extracts the flag name from the flag, i.e. return foo for --foo=bar.
    std::string GetCommandLineFlagName(std::string flag)
    {
        size_t dash_pos = flag.find("--");
        size_t equal_pos = flag.find('=');
        if (equal_pos == std::string::npos) {
            return flag.substr(dash_pos + 2);
        } else {
            return flag.substr(dash_pos + 2, equal_pos - 2);
        }
    }

    // Extracts the flag value from the flag, i.e. return bar for --foo=bar.
    // If the flag has no value (i.e. no equals sign) an empty string is returned.
    std::string GetCommandLineFlagValue(std::string flag)
    {
        size_t equal_pos = flag.find('=');
        if (equal_pos == std::string::npos) {
            return "";
        } else {
            return flag.substr(equal_pos + 1);
        }
    }

    void ParseArgs(int argc, char** argv){
        for(int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (String::start_with(arg, "-") && !String::start_with(arg, "--")) {
                auto key = arg.substr(1, arg.size());
                auto flags_itor = flags_.find(key);
                if (flags_itor == flags_.end()) {
                    if (key == "help") {
                        PrintUsageMessage();
                        exit(1);
                    }
                    // Ignore unknown flags.
                    fprintf(stdout, "Flag '%s' is not recognized\n", key.c_str());
                    continue;
                }
                simple_flags_[key] = 1;
                if (i+1 == argc) {
                    //it's the last argument
                    flags_[key] = "true";
                }else {
                    std::string param1 = argv[i+1];
                    if (String::start_with(param1, "-")) {
                        // the next token is another flags
                        flags_[key] = "true";
                    }else{
                        flags_[key] = param1;
                        i++;
                    }
                }
            }else{
                args_.push_back(arg);
            }
        }
    }

public:
    CommandLineParser(int argc, char** argv):argc_(argc), argv_(argv){
        std::cout << "CommandLineParser() ctor" << std::endl;
    }

    ~CommandLineParser() {
        std::cout << "~CommandLineParser() dtor" << std::endl;
    }
    // Disallow copy and assign
    CommandLineParser(const CommandLineParser& ) = delete;
    CommandLineParser& operator=(const CommandLineParser&) = delete;

    // Prints the entered flags and their values (without --help).
    void PrintEnteredFlags()
    {
        std::map<std::string, std::string>::iterator flag_iter;
        fprintf(stdout, "You have entered:\n");
        for (flag_iter = flags_.begin(); flag_iter != flags_.end(); ++flag_iter) {
            if (flag_iter->first != "help") {
                fprintf(stdout, "%s=%s, ", flag_iter->first.c_str(),
                        flag_iter->second.c_str());
            }
        }
        fprintf(stdout, "\n");
    }

    // Processes the vector of command line arguments and puts the value of each
    // flag in the corresponding map entry for this flag's name. We don't process
    // flags which haven't been defined in the map.
    void ProcessFlags()
    {
        ParseArgs(argc_, argv_);

        std::map<std::string, std::string>::iterator flag_iter;
        std::vector<std::string>::iterator iter;
        for (iter = args_.begin(); iter != args_.end(); ++iter) {
            if (!IsFlagWellFormed(*iter)) {
                // Ignore badly formated flags.
                continue;
            }
            std::string flag_name = GetCommandLineFlagName(*iter);
            flag_iter = flags_.find(flag_name);
            if (flag_iter == flags_.end()) {
                //HACK:for help command
                if (flag_name == "help") {
                    PrintUsageMessage();
                    exit(1);
                }
                // Ignore unknown flags.
                fprintf(stdout, "Flag '%s' is not recognized\n", flag_name.c_str());
                continue;
            }
            if (IsStandaloneFlag(*iter)) {
                flags_[flag_name] = "true";
            } else {
                flags_[flag_name] = GetCommandLineFlagValue(*iter);
            }
        }
    }

    // prints the usage message.
    void PrintUsageMessage()
    {
        fprintf(stdout, "Usage:\n");
        for(auto s: usage_messages_) {
            if (simple_flags_.find(s.first) != simple_flags_.end()) {
                fprintf(stdout, "\t-%s:%s\n", s.first.c_str(), s.second.c_str());
            }else {
                fprintf(stdout, "\t--%s:%s\n", s.first.c_str(), s.second.c_str());
            }
        }

    }

    // Set a flag into the map of flag names/values.
    // To set a boolean flag, use "false" as the default flag value.
    // The flag_name should not include the -- prefix.
    void SetFlag(std::string flag_name, std::string default_flag_value, const std::string &&desc = "")
    {
        flags_[flag_name] = default_flag_value;
        usage_messages_[flag_name] = desc;
        if (flag_name.length() == 1){
            simple_flags_[flag_name] = 1;
        }
    }

    // Gets a flag when provided a flag name (name is without the -- prefix).
    // Returns "" if the flag is unknown and "true"/"false" if the flag is a
    // boolean flag.
    std::string GetFlag(std::string flag_name)
    {
        std::map<std::string, std::string>::iterator flag_iter;
        flag_iter = flags_.find(flag_name);
        // If no such flag.
        if (flag_iter == flags_.end()) {
            return "";
        }
        return flag_iter->second;
    }
};



#endif //FACEDEMOEXAMPLE_OTL_CMD_PARSER_H
