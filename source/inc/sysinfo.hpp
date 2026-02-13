#pragma once

#include "util.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace sysinfo_ns
{

    class sysinfo_t
    {
        public:
            static sysinfo_t &instance_fn()
            {
                static sysinfo_t instance;
                return instance;
            }

            std::string get_hostname_fn() const
            {
                return util_ns::exec_cmd_fn("hostname");
            }

            std::string get_username_fn() const
            {
                const char *user = std::getenv("USER");
                return user ? std::string(user) : util_ns::exec_cmd_fn("whoami");
            }

            std::string get_kernel_fn() const
            {
                std::string k = util_ns::exec_cmd_fn("uname -r");
                auto pos = k.find('-');
                if (pos != std::string::npos)
                    return k.substr(0, pos);
                return k;
            }

            std::string get_os_fn() const
            {
                std::ifstream file("/etc/os-release");
                std::string line;
                while (std::getline(file, line))
                {
                    if (line.rfind("PRETTY_NAME=", 0) == 0)
                    {
                        std::string val = line.substr(12);
                        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                            val = val.substr(1, val.size() - 2);
                        return val;
                    }
                }
                return util_ns::exec_cmd_fn("uname -o");
            }

            std::string get_cpu_fn() const
            {
                std::string cpu = util_ns::exec_cmd_fn("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2");
                std::string clean = util_ns::trim_fn(cpu);

                // replace dashes with spaces
                std::replace(clean.begin(), clean.end(), '-', ' ');

                const std::vector<std::string> remove = {"(R)", "(TM)", "Intel", "AMD", "Core", "CPU", "Processor", "@", "GHz", "ghz"};
                for (const auto &rm : remove)
                {
                    size_t pos;
                    while ((pos = clean.find(rm)) != std::string::npos)
                    {
                        clean.erase(pos, rm.length());
                    }
                }

                // Tokenize and rebuild to filter out frequencies (e.g. 2.50)
                std::stringstream ss(clean);
                std::string segment;
                std::string result;
                while (ss >> segment)
                {
                    // Check if segment is a frequency (contains digit and dot)
                    bool is_freq = false;
                    bool has_dot = (segment.find('.') != std::string::npos);
                    bool has_digit = std::any_of(segment.begin(), segment.end(), ::isdigit);

                    if (has_dot && has_digit)
                        continue; // Skip 2.50, 4.20, etc.

                    if (!result.empty())
                        result += " ";
                    result += segment;
                }

                return result;
            }

            struct gpu_info_t
            {
                    std::string name;
            };

            std::vector<gpu_info_t> get_gpus_fn() const
            {
                std::vector<gpu_info_t> gpus;
                std::string out = util_ns::exec_cmd_fn("lspci | grep -Ei 'vga|3d|display'");
                auto lines = util_ns::split_fn(out, '\n');

                for (const auto &line : lines)
                {
                    std::string name;
                    auto open = line.find('[');
                    auto close = line.find(']', open);
                    if (open != std::string::npos && close != std::string::npos && close > open)
                        name = line.substr(open + 1, close - open - 1);
                    else
                    {
                        auto colon = line.find_last_of(':');
                        if (colon != std::string::npos)
                            name = line.substr(colon + 1);
                    }

                    if (!name.empty())
                    {
                        // Clean common junk
                        const std::vector<std::string> junk = {"GeForce", "Mobile", "NVIDIA", "Corporation", "Inc", "geforce", "mobile", "nvidia"};
                        for (const auto &j : junk)
                        {
                            size_t p;
                            while ((p = name.find(j)) != std::string::npos)
                            {
                                name.erase(p, j.length());
                            }
                        }
                        // Collapse spaces
                        auto end = std::unique(name.begin(), name.end(), [](char a, char b)
                                               { return std::isspace(a) && std::isspace(b); });
                        name.erase(end, name.end());

                        name = util_ns::trim_fn(name);
                        gpus.push_back({name});
                    }
                }
                if (gpus.empty())
                    gpus.push_back({"unknown"});
                return gpus;
            }

            struct mem_info_t
            {
                    long used;
                    long total;
                    long swap_used;
                    long swap_total;
            };

            mem_info_t get_mem_fn() const
            {
                std::ifstream file("/proc/meminfo");
                std::string key, unit;
                long val;
                long total = 0, avail = 0, swap_total = 0, swap_free = 0;

                while (file >> key >> val >> unit)
                {
                    if (key == "MemTotal:")
                        total = val;
                    else if (key == "MemAvailable:")
                        avail = val;
                    else if (key == "SwapTotal:")
                        swap_total = val;
                    else if (key == "SwapFree:")
                        swap_free = val;
                }
                return {total - avail, total, swap_total - swap_free, swap_total};
            }

            std::string get_wm_fn() const
            {
                // Simple heuristic
                const char *xdg = std::getenv("XDG_CURRENT_DESKTOP");
                if (xdg)
                {
                    std::string s(xdg);
                    if (s.find("Hyprland") != std::string::npos)
                        return "hyprland";
                    if (s.find("GNOME") != std::string::npos)
                        return "mutter";
                    if (s.find("KDE") != std::string::npos)
                        return "kwin";
                    if (s.find("SWAY") != std::string::npos)
                        return "sway";
                    return util_ns::to_lower_fn(s);
                }
                return "unknown";
            }

            std::string get_de_fn() const
            {
                const char *hypr_cmd = std::getenv("HYPRLAND_CMD");
                if (hypr_cmd)
                {
                    std::string cmd = util_ns::to_lower_fn(hypr_cmd);
                    if (cmd.find("hypryou") != std::string::npos)
                        return "hypryou";
                }

                const char *de = std::getenv("XDG_SESSION_DESKTOP");
                if (de)
                    return util_ns::to_lower_fn(de);

                const char *d = std::getenv("DESKTOP_SESSION");
                if (d)
                    return util_ns::to_lower_fn(d);

                return "unknown";
            }

            std::string get_shell_fn() const
            {
                std::string shell = util_ns::exec_cmd_fn("echo $SHELL");
                if (shell.empty())
                    return "unknown";
                std::filesystem::path p(shell);
                return p.filename().string();
            }

            std::string get_term_fn() const
            {
                const char *term = std::getenv("TERM_PROGRAM");
                if (term)
                    return term;
                std::string comm = util_ns::exec_cmd_fn("ps -p $(ps -p $$ -o ppid=) -o comm=");
                return comm.empty() ? "term" : comm;
            }

            struct pkg_info_t
            {
                    int count;
                    std::string manager;
            };

            std::vector<pkg_info_t> get_pkgs_fn() const
            {
                std::vector<pkg_info_t> pkgs;
                auto check = [&](const std::string &cmd, const std::string &name)
                {
                    std::string res = util_ns::exec_cmd_fn(cmd);
                    if (!res.empty())
                    {
                        try
                        {
                            int c = std::stoi(res);
                            if (c > 0)
                                pkgs.push_back({c, name});
                        }
                        catch (...)
                        {
                        }
                    }
                };

                check("pacman -Q 2>/dev/null | wc -l", "pacman");
                check("flatpak list 2>/dev/null | wc -l", "flatpak");
                check("dpkg -l 2>/dev/null | wc -l", "dpkg");
                check("rpm -qa 2>/dev/null | wc -l", "rpm");
                return pkgs;
            }

            std::string get_proc_count_fn() const
            {
                std::string res = util_ns::exec_cmd_fn("ps -e | wc -l");
                return res.empty() ? "0" : res;
            }

        private:
            sysinfo_t() = default;
    };
} // namespace sysinfo_ns
