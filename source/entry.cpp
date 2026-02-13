#include "inc/ascii.hpp"
#include "inc/config.hpp"
#include <iostream>
#include <exception>
#include <filesystem>

#include "inc/args.hpp"

using namespace renderer_ns;
using namespace config_ns;
using namespace args_ns;

int main_fn(int argc, char **argv)
{
    try
    {
        // 0. Parse Arguments
        args_t args = parse_args_fn(argc, argv);

        if (args.show_help)
        {
            print_help_fn();
            return 0;
        }
        if (args.show_version)
        {
            print_version_fn();
            return 0;
        }
        if (args.error)
        {
            std::cerr << "mfetch: " << args.error_msg << "\n";
            std::cerr << "Try 'mfetch --help' for more information.\n";
            return 1;
        }

        // default: $HOME/.config/mfetch.conf
        std::string config_path = std::getenv("HOME") + std::string("/.config/mfetch.conf");

        // Override
        if (!args.config_path.empty())
        {
            config_path = args.config_path;
        }

        config_t config;

        // Try load
        if (std::filesystem::exists(config_path))
        {
            config = load_config_fn(config_path);
        }
        else
        {
            if (!args.config_path.empty())
            {
                std::cerr << "mfetch: cannot access '" << config_path << "': No such file or directory\n";
                return 1;
            }

            std::cerr << "mfetch: cfg not found !!! (was given: \"" + config_path + "\")\n";
            config = load_config_fn("");
        }

        // 2. Initialize Engine
        engine_t engine(config);

        // 3. Render
        engine.render_fn();

        std::cout << "\n\n(used config: \"" + config_path + "\")\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "mfetch error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "mfetch critical error: unknown exception\n";
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    return main_fn(argc, argv);
}
