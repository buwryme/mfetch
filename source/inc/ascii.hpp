#pragma once

#include "config.hpp"
#include "sysinfo.hpp"
#include "util.hpp"
#include <sstream>
#include <vector>
#include <iostream>
#include <map>
#include <iomanip>

namespace renderer_ns
{
    using namespace sysinfo_ns;
    using namespace config_ns;
    using namespace util_ns;

    inline std::string fmt_mem_fn(long kb)
    {
        double gb = kb / 1024.0 / 1024.0;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << gb << "g";
        return ss.str();
    }

    // Improved visible length for UTF-8 and ANSI
    inline size_t visible_len_fn(const std::string &s)
    {
        size_t len = 0;
        bool in_esc = false;
        for (size_t i = 0; i < s.size(); ++i)
        {
            unsigned char c = s[i];
            if (c == '\033')
                in_esc = true;
            else if (in_esc)
            {
                if (c == 'm')
                    in_esc = false;
            }
            else
            {
                if ((c & 0xc0) != 0x80)
                {
                    len++;
                }
            }
        }
        return len;
    }

    class engine_t
    {
            config_t config;
            sysinfo_t &sys;
            std::map<std::string, std::string> ctx_cache;

            std::string get_val_lazy(const std::string &key)
            {
                if (ctx_cache.count(key))
                    return ctx_cache[key];

                std::string val = "unknown";
                if (key == "host")
                    val = util_ns::to_lower_fn(sys.get_hostname_fn());
                else if (key == "user")
                    val = util_ns::to_lower_fn(sys.get_username_fn());
                else if (key == "kernel")
                    val = util_ns::to_lower_fn(sys.get_kernel_fn());
                else if (key == "os")
                    val = util_ns::to_lower_fn(sys.get_os_fn());
                else if (key == "cpu")
                    val = util_ns::to_lower_fn(sys.get_cpu_fn());
                else if (key == "sh")
                    val = util_ns::to_lower_fn(sys.get_shell_fn());
                else if (key == "term")
                    val = util_ns::to_lower_fn(sys.get_term_fn());
                else if (key == "proc")
                    val = sys.get_proc_count_fn();
                else if (key == "wm")
                    val = sys.get_wm_fn();
                else if (key == "de")
                    val = sys.get_de_fn();
                else if (key == "ram_used" || key == "ram_total" || key == "swap_used" || key == "swap_total")
                {
                    // Fetch mem once
                    if (ctx_cache.find("ram_used") == ctx_cache.end())
                    {
                        auto mem = sys.get_mem_fn();
                        ctx_cache["ram_used"] = fmt_mem_fn(mem.used);
                        ctx_cache["ram_total"] = fmt_mem_fn(mem.total);
                        ctx_cache["swap_used"] = fmt_mem_fn(mem.swap_used);
                        ctx_cache["swap_total"] = fmt_mem_fn(mem.swap_total);
                    }
                    val = ctx_cache[key];
                }
                else if (key == "pkgs")
                {
                    auto p = sys.get_pkgs_fn();
                    std::string s = "";
                    for (size_t i = 0; i < p.size(); ++i)
                    {
                        if (i > 0)
                            s += ", ";
                        s += std::to_string(p[i].count) + " " + p[i].manager;
                    }
                    if (s.empty())
                        s = "0";
                    val = s;
                }

                ctx_cache[key] = val;
                return val;
            }

            // Helper to resolve string with lazy fetches
            std::string resolve(std::string fmt)
            {
                size_t start_pos = 0;
                while ((start_pos = fmt.find('{', start_pos)) != std::string::npos)
                {
                    size_t end_pos = fmt.find('}', start_pos);
                    if (end_pos == std::string::npos)
                        break;

                    std::string key = fmt.substr(start_pos + 1, end_pos - start_pos - 1);
                    std::string val = get_val_lazy(key);

                    fmt.replace(start_pos, end_pos - start_pos + 1, val);
                    start_pos += val.length();
                }
                return fmt;
            }

        public:
            engine_t(const config_t &c) : config(c), sys(sysinfo_t::instance_fn())
            {
            }

            void render_fn()
            {
                // 1. Pre-calculate layout (labels and expansion)
                // We need to expand GPUs and determine max label width BEFORE printing anything
                // to maintain alignment.

                struct render_item_t
                {
                        int type; // 0=text/title, 1=pair
                        std::string label_text;
                        std::string value_fmt;   // format string or key
                        std::string color;       // output color (legacy/fallback)
                        std::string color_label; // label color
                        int indent;

                        // For direct value items (gpus expanded)
                        std::string direct_value;
                };

                std::vector<render_item_t> items;
                size_t max_label_w = 0;

                // Pre-fetch GPUs for expansion (relatively fast)
                auto gpus = sys.get_gpus_fn();

                for (const auto &mod : config.modules)
                {
                    if (mod.type == "gpu")
                    {
                        for (size_t i = 0; i < gpus.size(); ++i)
                        {
                            std::string lbl = mod.label.empty() ? "gpu" : mod.label;
                            if (gpus.size() > 1)
                                lbl += std::to_string(i);

                            std::string val = util_ns::to_lower_fn(gpus[i].name);
                            size_t lw = visible_len_fn(lbl);
                            if (lw > max_label_w)
                                max_label_w = lw;

                            std::string out_color = !mod.color_out.empty() ? mod.color_out : (!mod.color.empty() ? mod.color : "white");
                            std::string lbl_color = !mod.color_label.empty() ? mod.color_label : "white";
                            items.push_back({1, lbl, "", out_color, lbl_color, mod.indent, val});
                        }
                    }
                    else if (mod.type == "text" || mod.type == "sep" || mod.type == "empty" || mod.type == "host" || mod.type == "title")
                    {
                        items.push_back({0, "", mod.format, mod.color, "", mod.indent, ""});
                    }
                    else
                    {
                        // Regular Key-Value
                        std::string lbl = mod.label.empty() ? mod.type : mod.label;
                        size_t lw = visible_len_fn(lbl);
                        if (lw > max_label_w)
                            max_label_w = lw;

                        std::string fmt = mod.format;
                        if (fmt.empty())
                        {
                            if (mod.type == "ram")
                                fmt = "{ram_used} / {ram_total}";
                            else if (mod.type == "swap")
                                fmt = "{swap_used} / {swap_total}";
                            else
                                fmt = "{" + mod.type + "}";
                        }

                        std::string out_color = !mod.color_out.empty() ? mod.color_out : mod.color;
                        std::string lbl_color = !mod.color_label.empty() ? mod.color_label : "white";
                        items.push_back({1, lbl, fmt, out_color, lbl_color, mod.indent, ""});
                    }
                }

                // 2. Procedural Print Loop
                std::vector<std::string> art = split_fn(config.ascii_art, '\n');
                size_t art_w = 0;
                for (const auto &s : art)
                {
                    size_t l = visible_len_fn(s);
                    if (l > art_w)
                        art_w = l;
                }

                size_t total_rows = std::max(art.size(), items.size());

                for (size_t i = 0; i < total_rows; ++i)
                {
                    // A. Print Left side (ASCII)
                    std::string a = (i < art.size()) ? art[i] : "";
                    size_t alen = visible_len_fn(a);
                    size_t pad_base = (art_w > alen) ? art_w - alen : 0;

                    std::cout << " " << a;

                    // B. Print Separator/Indent
                    if (i < items.size())
                    {
                        auto &item = items[i];
                        int spaces = (int)pad_base + config.gap_size + item.indent;
                        if (spaces < 1)
                            spaces = 1;
                        std::cout << std::string(spaces, ' ');

                        // C. Print Item Info
                        if (item.type == 0)
                        {
                            // Text/Title - Resolve immediately (might pause here if it was slow data)
                            // Usually titles are fast.
                            std::string t = resolve(item.value_fmt);
                            if (!item.color.empty())
                                t = color_fn(t, item.color);
                            std::cout << t;
                        }
                        else
                        {
                            // Label - Value pair
                            // Print Label
                            size_t l_len = visible_len_fn(item.label_text);
                            size_t p = (max_label_w > l_len) ? (max_label_w - l_len) : 0;
                            std::cout << std::string(p, ' ') << color_fn(item.label_text, item.color_label) << " - ";
                            std::cout.flush(); // FORCE OUTPUT BEFORE FETCHING VALUE

                            // Fetch Value
                            std::string val = item.direct_value;
                            if (val.empty())
                                val = resolve(item.value_fmt);

                            if (!item.color.empty())
                                val = color_fn(val, item.color);

                            std::cout << val;
                        }
                    }

                    std::cout << "\n";
                    // Only flush if we are doing procedural stuff
                    // With package manager loops, it's nice to flush.
                    std::cout.flush();
                }
            }
    };
} // namespace renderer_ns
