#pragma once
#include "config.hpp"
#include "sysinfo.hpp"
#include "util.hpp"

#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <iomanip>

// small helpers
inline std::string indent_str(int n) { return std::string(n, ' '); }

static std::string trim_copy(std::string s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(0,1);
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    return s;
}

static std::string to_lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

static std::string to_upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

// ANSI color helpers: named colors and #RRGGBB
static std::string ansi_color_code(const std::string &name_or_hex) {
    if (name_or_hex.empty()) return "";
    static const std::unordered_map<std::string,std::string> named = {
        {"black","\033[30m"}, {"red","\033[31m"}, {"green","\033[32m"},
        {"yellow","\033[33m"}, {"blue","\033[34m"}, {"magenta","\033[35m"},
        {"cyan","\033[36m"}, {"white","\033[37m"}
    };
    auto it = named.find(name_or_hex);
    if (it != named.end()) return it->second;
    if (name_or_hex.size() >= 7 && name_or_hex[0] == '#') {
        auto hex = name_or_hex.substr(1);
        if (hex.size() >= 6) {
            try {
                int r = std::stoi(hex.substr(0,2), nullptr, 16);
                int g = std::stoi(hex.substr(2,2), nullptr, 16);
                int b = std::stoi(hex.substr(4,2), nullptr, 16);
                std::ostringstream ss;
                ss << "\033[38;2;" << r << ";" << g << ";" << b << "m";
                return ss.str();
            } catch (...) { return ""; }
        }
    }
    return "";
}

static std::vector<std::string> split_words(const std::string &s) {
    std::istringstream ss(s);
    std::vector<std::string> out;
    std::string w;
    while (ss >> w) out.push_back(w);
    return out;
}

// parse a numeric prefix from a string like "15.7 gb" -> returns numeric string and remainder unit
static std::pair<std::string,std::string> split_number_and_unit(const std::string &s) {
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    size_t start = i;
    bool seen_digit = false;
    // accept digits, dots, commas, optional leading +/-
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == ',')) {
        if (std::isdigit((unsigned char)s[i])) seen_digit = true;
        ++i;
    }
    if (!seen_digit) return {trim_copy(s), ""};
    std::string num = s.substr(start, i - start);
    std::string rest = trim_copy(s.substr(i));
    return {trim_copy(num), rest};
}

// convert numeric string from source unit to target unit (supports gb <-> mb)
static std::string convert_unit_numeric(const std::string &num_str, const std::string &src_unit, const std::string &target_unit) {
    if (num_str.empty()) return num_str;
    double val = 0.0;
    try {
        std::string tmp = num_str;
        std::replace(tmp.begin(), tmp.end(), ',', '.');
        val = std::stod(tmp);
    } catch (...) {
        return num_str;
    }
    std::string ssrc = to_lower_copy(src_unit);
    std::string stgt = to_lower_copy(target_unit);
    if (ssrc.find("gb") != std::string::npos && stgt.find("mb") != std::string::npos) {
        val = val * 1024.0;
    } else if (ssrc.find("mb") != std::string::npos && stgt.find("gb") != std::string::npos) {
        val = val / 1024.0;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << val;
    return ss.str();
}

// apply modifiers to a text; supports "-color" followed by a separate token value and "-round" with optional precision
static std::string apply_modifiers(std::string text, const std::vector<std::string>& mods, const Config *cfg_ptr = nullptr) {
    std::string color_code;
    int pad = 0;
    bool round_flag = false;
    int round_precision = 1; // default precision for -round

    for (size_t i = 0; i < mods.size(); ++i) {
        const std::string &m = mods[i];
        if (m == "-lower") text = to_lower_copy(text);
        else if (m == "-upper") text = to_upper_copy(text);
        else if (m == "-trim") text = trim_copy(text);
        else if (m.rfind("-round",0) == 0) {
            round_flag = true;
            // support "-round" and "-round=0" style
            size_t eq = m.find('=');
            if (eq != std::string::npos) {
                try { round_precision = std::stoi(m.substr(eq+1)); }
                catch (...) { round_precision = 1; }
            } else {
                // maybe next token is a numeric precision (e.g., "-round 0")
                if (i + 1 < mods.size() && !mods[i+1].empty() && mods[i+1][0] != '-') {
                    try { round_precision = std::stoi(mods[i+1]); ++i; }
                    catch (...) { /* ignore */ }
                } else {
                    round_precision = 1;
                }
            }
        }
        else if (m.rfind("pad=",0) == 0 || m.rfind("width=",0) == 0) {
            auto eq = m.find('=');
            if (eq != std::string::npos) pad = std::stoi(m.substr(eq+1));
        } else if (m == "-color") {
            if (i + 1 < mods.size()) {
                const std::string &next = mods[i+1];
                if (!next.empty() && next[0] != '-') {
                    color_code = ansi_color_code(next);
                    ++i;
                }
            }
        } else if (m.rfind("color=",0) == 0) {
            auto eq = m.find('=');
            if (eq != std::string::npos) color_code = ansi_color_code(m.substr(eq+1));
        } else if (m == "-no-platform-tokens") {
            const std::vector<std::string> toks = {"gnu/linux","linux","unix","bsd","gnu","os","system"};
            for (auto &t : toks) {
                size_t pos;
                while ((pos = to_lower_copy(text).find(t)) != std::string::npos) {
                    text.erase(pos, t.size());
                }
            }
        } else if (m == "-no-brand") {
            const std::vector<std::string> toks = {"intel","amd","nvidia","geforce","graphics","mobile","processor"};
            std::string low = to_lower_copy(text);
            for (auto &t : toks) {
                size_t pos;
                while ((pos = low.find(t)) != std::string::npos) {
                    text.erase(pos, t.size());
                    low = to_lower_copy(text);
                }
            }
        } else if (m == "-no-speed") {
            std::string low = to_lower_copy(text);
            size_t pos;
            while ((pos = low.find("ghz")) != std::string::npos) {
                size_t start = (pos >= 6) ? pos - 6 : 0;
                text.erase(start, pos - start + 3);
                low = to_lower_copy(text);
            }
        } else if (m == "-no-v-before") {
            if (!text.empty() && text.front() == 'v' && (text.size() == 1 || std::isspace((unsigned char)text[1]))) {
                text.erase(0,1);
                text = trim_copy(text);
            }
        } else if (m == "-force") {
            // no-op
        }
    }

    if (round_flag) {
        // extract numeric prefix and round it to requested precision
        auto [num, rest] = split_number_and_unit(text);
        if (!num.empty()) {
            try {
                std::string tmp = num;
                std::replace(tmp.begin(), tmp.end(), ',', '.');
                double d = std::stod(tmp);
                // compute rounding
                double factor = 1.0;
                if (round_precision > 0) {
                    for (int p = 0; p < round_precision; ++p) factor *= 10.0;
                    d = std::round(d * factor) / factor;
                } else {
                    d = std::round(d);
                }
                std::ostringstream ss;
                if (round_precision <= 0) ss << std::fixed << std::setprecision(0) << d;
                else ss << std::fixed << std::setprecision(round_precision) << d;
                text = ss.str();
                // do not reattach unit here; template should print [unit] separately when desired
            } catch (...) {
                // fallback: leave text unchanged
            }
        }
    }

    if (pad > 0 && (int)text.size() < pad) text += std::string(pad - text.size(), ' ');
    if (!color_code.empty()) text = color_code + text + "\033[0m";
    return text;
}

// If tmpl begins with a numeric token like "[0]" or "[12]" (possibly with leading spaces),
// return {index, trimmed_template_without_token}. If no leading numeric token, return {-1, tmpl}.
static std::pair<int,std::string> extract_leading_index(std::string tmpl) {
    size_t p = 0;
    while (p < tmpl.size() && std::isspace((unsigned char)tmpl[p])) ++p;
    if (p >= tmpl.size()) return {-1, tmpl};
    if (tmpl[p] != '[') return {-1, tmpl};

    size_t q = tmpl.find(']', p);
    if (q == std::string::npos) return {-1, tmpl};

    std::string inside = tmpl.substr(p+1, q - p - 1);
    bool all_digits = !inside.empty();
    for (char c : inside) if (!std::isdigit((unsigned char)c)) { all_digits = false; break; }

    if (!all_digits) return {-1, tmpl};

    int idx = 0;
    try { idx = std::stoi(inside); } catch (...) { return {-1, tmpl}; }

    std::string rest = tmpl.substr(0, p) + tmpl.substr(q+1);
    size_t r = 0;
    while (r < rest.size() && std::isspace((unsigned char)rest[r])) ++r;
    if (r > 0) rest.erase(0, r);

    return {idx, rest};
}

// apply a template to label/value pair with index support and extra map
inline std::string apply_format_template(const Config &cfg,
                                         const std::string &tmpl,
                                         const std::string &label,
                                         const std::string &value,
                                         int index = -1,
                                         const std::unordered_map<std::string,std::string> &extra = {}) {
    std::string out;
    size_t i = 0;
    while (i < tmpl.size()) {
        if (tmpl[i] == '[') {
            size_t j = tmpl.find(']', i);
            if (j == std::string::npos) { out += tmpl.substr(i); break; }
            std::string token = tmpl.substr(i+1, j-i-1);
            auto words = split_words(token);
            if (words.empty()) { i = j+1; continue; }
            std::string key = words[0];
            std::vector<std::string> mods(words.begin()+1, words.end());

            std::string resolved;
            if (key == "k" || key == "label") resolved = label;
            else if (key == "v" || key == "value") resolved = value;
            else if (key == "hostname") resolved = get_hostname();
            else if (key == "kernel" || key == "kpart") resolved = get_kernel();
            else if (key == "os") resolved = get_os();
            else if (key == "cpu") resolved = get_cpu();
            else if (key == "sh") resolved = get_shell();
            else if (key == "term") resolved = get_terminal();
            else if (key == "gpu" || key == "gpus") {
                auto it = extra.find("gpu");
                resolved = (it != extra.end()) ? it->second : value;
            }
            else if (key == "used") {
                auto it = extra.find("used");
                resolved = (it != extra.end()) ? it->second : value;
            }
            else if (key == "all" || key == "total") {
                auto it = extra.find("all");
                resolved = (it != extra.end()) ? it->second : value;
            }
            else if (key == "unit") {
                auto it = extra.find("unit");
                resolved = (it != extra.end()) ? it->second : cfg.unit;
            }
            else if (key == "mount-point" || key == "mount") {
                auto it = extra.find("mount-point");
                resolved = (it != extra.end()) ? it->second : value;
            }
            else if (key == "partition") {
                auto it = extra.find("partition");
                resolved = (it != extra.end()) ? it->second : value;
            }
            else if (key == "n" || key == "index") {
                resolved = (index >= 0) ? std::to_string(index) : "";
            }
            else if (key.rfind("n-",0) == 0) {
                auto it = extra.find(key);
                resolved = (it != extra.end()) ? it->second : "";
            }
            else {
                auto it = extra.find(key);
                if (it != extra.end()) resolved = it->second;
                else resolved = key;
            }

            if (index >= 0) {
                std::string idx = std::to_string(index);
                size_t pos = 0;
                while ((pos = resolved.find("{n}", pos)) != std::string::npos) {
                    resolved.replace(pos, 3, idx);
                    pos += idx.size();
                }
            }

            resolved = apply_modifiers(resolved, mods, &cfg);
            out += resolved;
            i = j + 1;
        } else {
            out.push_back(tmpl[i]);
            ++i;
        }
    }
    return out;
}

// split ascii art lines
inline std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line)) out.push_back(line);
    return out;
}

// pick template for a key
static std::string pick_template(const Config &cfg, const std::string &key) {
    auto it = cfg.field_templates.find(key);
    if (it != cfg.field_templates.end()) return it->second;
    if (key.rfind("gpu",0) == 0) {
        auto it2 = cfg.field_templates.find("gpus");
        if (it2 != cfg.field_templates.end()) return it2->second;
        it2 = cfg.field_templates.find("gpu");
        if (it2 != cfg.field_templates.end()) return it2->second;
    }
    if (key.rfind("mnt",0) == 0 || key.rfind("mount",0) == 0) {
        auto it2 = cfg.field_templates.find("mnts");
        if (it2 != cfg.field_templates.end()) return it2->second;
        it2 = cfg.field_templates.find("mnt");
        if (it2 != cfg.field_templates.end()) return it2->second;
    }
    auto itd = cfg.field_templates.find("default");
    if (itd != cfg.field_templates.end()) return itd->second;
    return "[k] [v]";
}

// render all info lines using templates
inline std::vector<std::string> render_info_lines(const Config &cfg) {
    std::vector<std::pair<int,std::string>> indexed;
    std::vector<std::string> unindexed;

    auto push_templated = [&](const std::string &key,
                              const std::string &label,
                              const std::string &value,
                              int idx_hint,
                              const std::unordered_map<std::string,std::string> &extra = {}) {
        std::string tmpl = pick_template(cfg, key);
        auto [desired_idx, cleaned_tmpl] = extract_leading_index(tmpl);
        std::string formatted = apply_format_template(cfg, cleaned_tmpl, label, value, idx_hint, extra);
        if (desired_idx >= 0) indexed.emplace_back(desired_idx, formatted);
        else unindexed.push_back(formatted);
    };

    // hostname
    push_templated("hostname", "host", get_hostname(), -1);

    // kernel
    {
        std::string raw_kernel = get_kernel();
        std::istringstream ss(raw_kernel);
        std::string kpart, vpart;
        if (ss >> kpart) { std::getline(ss, vpart); vpart = trim_copy(vpart); }
        push_templated("kernel", kpart, vpart, -1);
    }

    // os
    push_templated("os", "os", get_os(), -1);

    // cpu
    push_templated("cpu", "cpu", get_cpu(), -1);

    // gpus
    {
        auto gpus = get_gpus();
        for (size_t i = 0; i < gpus.size(); ++i) {
            std::unordered_map<std::string,std::string> extra;
            extra["gpu"] = gpus[i];
            push_templated("gpus", "gpu", gpus[i], (int)i, extra);
        }
    }

    // ram: convert used/all to cfg.unit numeric strings
    {
        std::string raw = get_ram(); // e.g. "7.3 gb / 15 gb"
        size_t slash = raw.find('/');
        std::string used_raw = raw, all_raw = "";
        if (slash != std::string::npos) {
            used_raw = trim_copy(raw.substr(0, slash));
            all_raw = trim_copy(raw.substr(slash+1));
        }
        auto [used_num, used_unit] = split_number_and_unit(used_raw);
        auto [all_num, all_unit] = split_number_and_unit(all_raw);
        std::string target_unit = cfg.unit;
        std::string used_conv = used_num.empty() ? used_num : convert_unit_numeric(used_num, used_unit, target_unit);
        std::string all_conv  = all_num.empty()  ? all_num  : convert_unit_numeric(all_num, all_unit, target_unit);
        std::unordered_map<std::string,std::string> extra;
        extra["used"] = used_conv;
        extra["all"]  = all_conv;
        extra["unit"] = target_unit;
        push_templated("ram", "ram", used_conv + " / " + all_conv, -1, extra);
    }

    // shell
    push_templated("sh", "sh", get_shell(), -1);

    // term
    push_templated("term", "term", get_terminal(), -1);

    // mounts
    {
        auto disks = get_disks();
        for (size_t i = 0; i < disks.size(); ++i) {
            std::string mount = disks[i];
            std::string used = "", all = "", part = "";
            size_t colon = mount.rfind(" : ");
            if (colon != std::string::npos) {
                part = trim_copy(mount.substr(colon + 3));
                mount = trim_copy(mount.substr(0, colon));
            }
            std::istringstream ms(mount);
            std::string mountpoint;
            ms >> mountpoint;
            std::string rest;
            std::getline(ms, rest);
            rest = trim_copy(rest);
            size_t s = rest.find('/');
            if (s != std::string::npos) {
                used = trim_copy(rest.substr(0, s));
                all = trim_copy(rest.substr(s+1));
            } else {
                used = rest;
            }

            auto [used_num, used_unit] = split_number_and_unit(used);
            auto [all_num, all_unit] = split_number_and_unit(all);
            std::string used_conv = used_num.empty() ? used_num : convert_unit_numeric(used_num, used_unit, cfg.unit);
            std::string all_conv  = all_num.empty()  ? all_num  : convert_unit_numeric(all_num, all_unit, cfg.unit);

            std::string part_norm = part;
            if (!part_norm.empty() && part_norm.front() == 'p') part_norm.erase(0,1);

            std::unordered_map<std::string,std::string> extra;
            extra["mount-point"] = mountpoint;
            extra["used"] = used_conv;
            extra["all"] = all_conv;
            extra["unit"] = cfg.unit;
            extra["partition"] = part_norm;
            push_templated("mnts", "mnt", mountpoint, (int)i, extra);
        }
    }

    // pkgs
    {
        std::string raw = get_packages();
        std::unordered_map<std::string,std::string> extra;
        std::vector<std::string> parts;
        std::istringstream ss(raw);
        std::string token;
        while (std::getline(ss, token, ',')) parts.push_back(trim_copy(token));
        if (!parts.empty()) {
            std::istringstream p0(parts[0]);
            std::string n0, m0;
            p0 >> n0;
            std::getline(p0, m0);
            extra["n-pkg1"] = n0;
            extra["pkgmgr1"] = trim_copy(m0);
        }
        if (parts.size() > 1) {
            std::istringstream p1(parts[1]);
            std::string n1, m1;
            p1 >> n1;
            std::getline(p1, m1);
            extra["n-pkgs2"] = n1;
            extra["pkgmgr2"] = trim_copy(m1);
        }
        push_templated("pkgs", "pkgs", raw, -1, extra);
    }

    // assemble final lines honoring indices
    int max_idx = -1;
    for (auto &p : indexed) if (p.first > max_idx) max_idx = p.first;
    std::vector<std::string> final_lines;
    if (max_idx >= 0) final_lines.assign(max_idx + 1, "");

    for (auto &pr : indexed) {
        int pos = pr.first;
        std::string txt = pr.second;
        if (pos < 0) { unindexed.push_back(txt); continue; }
        if (pos >= (int)final_lines.size()) final_lines.resize(pos + 1, "");
        int p = pos;
        while (p < (int)final_lines.size() && !final_lines[p].empty()) ++p;
        if (p >= (int)final_lines.size()) final_lines.push_back(txt);
        else final_lines[p] = txt;
    }

    for (auto &u : unindexed) final_lines.push_back(u);

    std::vector<std::string> cleaned;
    for (auto &l : final_lines) if (!l.empty()) cleaned.push_back(l);

    return cleaned;
}

// pad helper used by print_with_ascii
inline std::vector<std::string> pad_lines(const std::vector<std::string>& lines, size_t width) {
    std::vector<std::string> out = lines;
    for (auto &l : out) if (l.size() < width) l += std::string(width - l.size(), ' ');
    return out;
}

inline void print_with_ascii(const Config& cfg, const std::vector<std::string>& info_lines) {
    auto art_lines = split_lines(cfg.ascii_art);

    // prepare padded info_lines
    std::vector<std::string> padded_info;
    padded_info.reserve(cfg.top_padding + info_lines.size() + cfg.bottom_padding);
    for (int i = 0; i < cfg.top_padding; ++i) padded_info.push_back("");
    for (auto &l : info_lines) padded_info.push_back(l);
    for (int i = 0; i < cfg.bottom_padding; ++i) padded_info.push_back("");

    if (cfg.ascii_position == "top") {
        for (auto &l : art_lines) std::cout << l << "\n";
        for (auto &l : padded_info) std::cout << indent_str(cfg.indent) << l << "\n";
        return;
    }
    if (cfg.ascii_position == "bottom") {
        for (auto &l : padded_info) std::cout << indent_str(cfg.indent) << l << "\n";
        for (auto &l : art_lines) std::cout << l << "\n";
        return;
    }

    size_t art_w = 0;
    for (auto &l : art_lines) if (l.size() > art_w) art_w = l.size();
    art_w = std::min<size_t>(art_w, cfg.ascii_width);

    auto left = art_lines;
    auto right = padded_info;

    size_t rows = std::max(left.size(), right.size());
    left.resize(rows);
    right.resize(rows);

    auto left_padded = pad_lines(left, art_w);
    for (size_t i = 0; i < rows; ++i) {
        std::string a = left_padded[i];
        std::string b = right[i];
        if (cfg.ascii_position == "left") {
            std::cout << a << std::string(cfg.ascii_padding, ' ') << indent_str(cfg.indent) << b << "\n";
        } else {
            std::cout << indent_str(cfg.indent) << b << std::string(cfg.ascii_padding, ' ') << a << "\n";
        }
    }
}
