#pragma once

#include <string>
#include <vector>
#include <fstream>

namespace config_ns
{
    struct module_cfg_t
    {
            std::string type;
            std::string format;
            std::string label;
            std::string color;       // legacy - applies to output
            std::string color_label; // label color
            std::string color_out;   // output/value color
            int indent = 0;
    };

    struct config_t
    {
            std::string ascii_art;
            std::string ascii_position = "left";
            int ascii_margin = 1;
            int gap_size = 2;

            std::vector<module_cfg_t> modules;
    };

    inline std::string trim_val_fn(std::string s)
    {
        auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";
        auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    // Simple robust custom TOML-subset parser
    // Supports [ascii] block for 'art' multi-line string
    // Supports [[module]] layout
    inline config_t load_config_fn(const std::string &path)
    {
        config_t cfg;
        // Default ASCII if not found
        cfg.ascii_art = R"(
₍^. .^₎⟆ ;)";

        bool in_module = false;
        bool in_ascii = false;
        std::string current_art = "";
        bool capturing_art = false;

        std::ifstream f(path);
        if (!f.is_open())
        {
            throw std::runtime_error("cfg not found !!! (was given: \"" + path + "\")");
            // Defaults if no file match
            cfg.modules = {
                {"host", ">> {user} @ {host} //", "", "", "", "", -4},
                {"kernel", "{kernel}", "krnl", "white", "", "", 0},
                {"sh", "{sh}", "sh", "white", "", "", 0},
                {"term", "{term}", "term", "white", "", "", 0},
                {"sep", "...................", "", "white", "", "", 0},
                {"wm", "{wm}", "wm", "white", "", "", 0},
                {"de", "{de}", "de", "white", "", "", 0},
                {"sep", "...................", "", "white", "", "", 0},
                {"cpu", "{cpu}", "cpu", "white", "", "", 0},
                {"gpu", "{gpu}", "gpu", "white", "", "", 0},
                {"sep", "...................", "", "white", "", "", 0},
                {"ram", "{ram_used} / {ram_total}", "ram", "white", "", "", 0},
                {"swap", "{swap_used} / {swap_total}", "swap", "white", "", "", 0}};
            return cfg;
        }

        std::string line;
        while (std::getline(f, line))
        {
            std::string tr = trim_val_fn(line);
            if (tr.empty() || tr[0] == '#')
                continue;

            if (tr == "[ascii]")
            {
                in_ascii = true;
                in_module = false;
                continue;
            }
            if (tr == "[[module]]")
            {
                in_module = true;
                in_ascii = false;
                cfg.modules.push_back({}); // New module
                continue;
            }

            if (in_ascii)
            {
                if (tr.rfind("art = \"\"\"", 0) == 0 || tr.rfind("art = '''", 0) == 0)
                {
                    capturing_art = true;
                    continue;
                }
                if (capturing_art)
                {
                    if (tr.rfind("\"\"\"", 0) == 0 || tr.rfind("'''", 0) == 0)
                    {
                        capturing_art = false;
                        cfg.ascii_art = current_art;
                    }
                    else
                    {
                        current_art += line + "\n";
                    }
                }
            }
            else if (in_module && !cfg.modules.empty())
            {
                auto eq = line.find('=');
                if (eq != std::string::npos)
                {
                    std::string key = trim_val_fn(line.substr(0, eq));
                    std::string val = trim_val_fn(line.substr(eq + 1));
                    // Unwrap quotes
                    if (val.size() >= 2 && (val.front() == '"' || val.front() == '\''))
                    {
                        val = val.substr(1, val.size() - 2);
                    }

                    auto &m = cfg.modules.back();
                    if (key == "type")
                        m.type = val;
                    else if (key == "format")
                        m.format = val;
                    else if (key == "label")
                        m.label = val;
                    else if (key == "color")
                        m.color = val;
                    else if (key == "color_label")
                        m.color_label = val;
                    else if (key == "color_out")
                        m.color_out = val;
                    else if (key == "indent")
                    {
                        try
                        {
                            m.indent = std::stoi(val);
                        }
                        catch (...)
                        {
                        }
                    }
                }
            }
        }

        return cfg;
    }
} // namespace config_ns
