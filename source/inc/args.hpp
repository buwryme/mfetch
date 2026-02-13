#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace args_ns
{
    struct args_t
    {
            bool show_help = false;
            bool show_version = false;
            std::string config_path;
            bool error = false;
            std::string error_msg;
    };

    inline void print_help_fn()
    {
        std::cout << "usage: mfetch [OPTIONS]\n"
                  << "a minimal fetch tool!\n\n"
                  << "options:\n"
                  << "  -c, --config <PATH>   use a specific configuration file\n"
                  << "  -h, --help            display this and exit\n"
                  << "  -v, --version         output version information and exit\n\n"
                  << "default config location: $HOME/.config/mfetch.conf\n";
    }

    inline void print_version_fn()
    {
        std::cout << "mfetch v1.0.0\n";
    }

    inline args_t parse_args_fn(int argc, char **argv)
    {
        args_t args;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                args.show_help = true;
                return args;
            }
            else if (arg == "--version" || arg == "-v")
            {
                args.show_version = true;
                return args;
            }
            else if (arg == "--config" || arg == "-c")
            {
                if (i + 1 < argc)
                {
                    args.config_path = argv[++i];
                }
                else
                {
                    args.error = true;
                    args.error_msg = "Option '--config' requires an argument.";
                    return args;
                }
            }
            else if (arg[0] == '-')
            {
                // Handle GNU-style grouped short options? e.g. -vc path?
                // For now, let's keep it simple but strict on unknown flags to act like coreutils
                args.error = true;
                args.error_msg = "Unrecognized option '" + arg + "'";
                return args;
            }
            else
            {
                // Positional arguments? We don't really have any yet.
                // Treat as error or ignore?
                // Coreutils usually error on unexpected positionals if they don't take files.
                args.error = true;
                args.error_msg = "Unexpected argument '" + arg + "'";
                return args;
            }
        }

        return args;
    }
} // namespace args_ns
