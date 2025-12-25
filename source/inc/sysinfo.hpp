#pragma once

#include <cstdint>
#include <string>
#include <unistd.h>
#include <vector>
#include <array>
#include <memory>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>
#include <iomanip>

inline std::string exec_cmd(const std::string& cmd) {
    std::array<char, 128> buf;
    std::string out;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((cmd + " 2>/dev/null").c_str(), "r"), pclose);
    if (!pipe) return {};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe.get())) out += buf.data();
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

inline std::string get_hostname() { return exec_cmd("hostname"); }

inline std::string get_kernel_version() {
    std::string raw = exec_cmd("uname -r");
    size_t dash = raw.find('-');
    if (dash == std::string::npos) return raw;
    std::string version = raw.substr(0, dash);
    size_t next_dash = raw.find('-', dash + 1);
    std::string type = (next_dash != std::string::npos) ? raw.substr(dash + 1, next_dash - dash - 1) : raw.substr(dash + 1);
    if (!type.empty()) version += "-" + type;
    return version;
}

inline std::string get_kernel() { return to_lower(exec_cmd("uname") + " " + get_kernel_version()); }

inline void strip_platform_tokens(std::string& s) {
    const std::array<std::string, 7> tokens = { "gnu/linux","linux","unix","bsd","gnu","os","system" };
    for (const auto& token : tokens) {
        size_t pos;
        while ((pos = s.find(token)) != std::string::npos) {
            bool left_ok = (pos == 0 || s[pos - 1] == ' ');
            bool right_ok = (pos + token.size() == s.size() || s[pos + token.size()] == ' ');
            if (left_ok && right_ok) s.erase(pos, token.size());
            else break;
        }
    }
    while (s.find("  ") != std::string::npos) s.erase(s.find("  "), 1);
    if (!s.empty() && s.front() == ' ') s.erase(0, 1);
    if (!s.empty() && s.back() == ' ') s.pop_back();
}

inline std::string get_os() {
    std::string raw = exec_cmd("cat /etc/os-release");
    const std::string key = "NAME=\"";
    size_t start = raw.find(key);
    std::string os;
    if (start != std::string::npos) {
        start += key.length();
        size_t end = raw.find('"', start);
        if (end != std::string::npos) os = raw.substr(start, end - start);
    }
    os = to_lower(os);
    strip_platform_tokens(os);
    return os;
}

inline std::string get_cpu() {
    std::string raw = exec_cmd("grep -m1 'model name' /proc/cpuinfo");
    if (raw.empty()) return "";
    auto colon = raw.find(':');
    if (colon != std::string::npos) raw = raw.substr(colon + 1);
    std::string s;
    s.reserve(raw.size());
    for (unsigned char c : raw) s.push_back(std::tolower(c));
    auto at = s.find('@');
    if (at != std::string::npos) s.erase(at);
    const std::vector<std::string> remove_tokens = {"intel","(r)","(tm)","amd","cpu","@","ghz"};
    for (auto &token : remove_tokens) {
        size_t pos = 0;
        while ((pos = s.find(token, pos)) != std::string::npos) s.erase(pos, token.size());
    }
    std::string clean;
    bool last_space = false;
    for (char c : s) {
        if (std::isspace((unsigned char)c)) {
            if (!last_space) { clean.push_back(' '); last_space = true; }
        } else { clean.push_back(c); last_space = false; }
    }
    if (!clean.empty() && clean.front() == ' ') clean.erase(0,1);
    if (!clean.empty() && clean.back() == ' ') clean.pop_back();
    return clean;
}

inline std::string extract_brackets(const std::string& s) {
    auto l = s.find('[');
    auto r = s.find(']', l);
    if (l == std::string::npos || r == std::string::npos || r <= l) return "";
    return s.substr(l + 1, r - l - 1);
}

inline std::string clean_gpu_name(std::string s) {
    s = to_lower(s);
    const std::array<std::string, 2> remove_prefixes = { "geforce ", "graphics " };
    for (const auto& p : remove_prefixes) if (s.rfind(p, 0) == 0) s.erase(0, p.size());
    std::string out;
    bool last_space = false;
    for (char c : s) {
        if (std::isspace((unsigned char)c)) {
            if (!last_space) { out += ' '; last_space = true; }
        } else { out += c; last_space = false; }
    }
    if (!out.empty() && out.front() == ' ') out.erase(0, 1);
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

inline std::vector<std::string> get_gpus() {
    std::vector<std::string> gpus;
    std::string out = exec_cmd("lspci | grep -Ei 'vga|3d|display'");
    size_t start = 0;
    while (start < out.size()) {
        size_t end = out.find('\n', start);
        if (end == std::string::npos) end = out.size();
        std::string line = out.substr(start, end - start);
        std::string bracket = extract_brackets(line);
        if (!bracket.empty()) gpus.push_back(clean_gpu_name(bracket));
        start = end + 1;
    }
    return gpus;
}

inline std::string kb_to_gb_str(long kb) {
    double gb = kb / 1024.0 / 1024.0;
    char buf[32];
    if (gb < 10) snprintf(buf, sizeof(buf), "%.1f gb", gb);
    else snprintf(buf, sizeof(buf), "%.0f gb", gb);
    return std::string(buf);
}

inline std::string get_ram() {
    std::string total_raw = exec_cmd("grep MemTotal /proc/meminfo");
    std::string avail_raw = exec_cmd("grep MemAvailable /proc/meminfo");
    size_t colon_total = total_raw.find(':');
    size_t colon_avail = avail_raw.find(':');
    if (colon_total == std::string::npos || colon_avail == std::string::npos) return "";
    long total_kb = std::stol(total_raw.substr(colon_total + 1));
    long avail_kb = std::stol(avail_raw.substr(colon_avail + 1));
    long used_kb = total_kb - avail_kb;
    return kb_to_gb_str(used_kb) + " / " + kb_to_gb_str(total_kb);
}

inline std::string get_partition(const std::string& device) {
    size_t pos = device.find_last_of('/');
    if (pos == std::string::npos) return device;
    std::string part = device.substr(pos + 1);
    if (part.find("nvme") == 0) {
        size_t ppos = part.find_last_of('p');
        if (ppos != std::string::npos) part = part.substr(ppos);
    } else {
        size_t i = part.find_first_of("0123456789");
        if (i != std::string::npos) part = part.substr(i);
    }
    return part;
}

inline std::vector<std::string> get_disks() {
    std::vector<std::string> disks;
    std::string raw = exec_cmd("df --output=source,target,used,size -B1 -x tmpfs -x devtmpfs");
    const std::vector<std::string> ignore = {"/proc","/sys","/dev","/run","/var/tmp","/var/cache","/var/log","/tmp","/srv","/root","/boot/efi","/boot"};
    std::istringstream ss(raw);
    std::string line;
    bool first_line = true;
    while (std::getline(ss, line)) {
        if (first_line) { first_line = false; continue; }
        line.erase(std::unique(line.begin(), line.end(), [](char a, char b){ return a==' ' && b==' '; }), line.end());
        while (!line.empty() && std::isspace((unsigned char)line.front())) line.erase(0,1);
        std::istringstream ls(line);
        std::string device, mount, used_str, size_str;
        if (!(ls >> device >> mount >> used_str >> size_str)) continue;
        bool skip = false;
        for (auto &blk : ignore) if (mount == blk || mount.find(blk + "/") == 0) { skip = true; break; }
        if (skip) continue;
        try {
            uint64_t used = std::stoull(used_str);
            uint64_t size = std::stoull(size_str);
            std::string part = get_partition(device);
            std::ostringstream out;
            out << mount << " " << std::fixed << std::setprecision(1)
                << (used >= 1024ULL*1024ULL*1024ULL ? (used / (1024.0*1024.0*1024.0)) : (used / (1024.0*1024.0)))
                << (used >= 1024ULL*1024ULL*1024ULL ? " gb" : " mb")
                << " / "
                << (size >= 1024ULL*1024ULL*1024ULL ? (size / (1024.0*1024.0*1024.0)) : (size / (1024.0*1024.0)))
                << (size >= 1024ULL*1024ULL*1024ULL ? " gb" : " mb")
                << " : " << part;
            disks.push_back(out.str());
        } catch (...) { continue; }
    }
    return disks;
}

inline std::string get_shell() {
    std::string path = to_lower(exec_cmd("echo $SHELL"));
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) path = path.substr(slash + 1);
    return path;
}

inline std::string get_terminal() {
    struct TermEnv { const char* var; const char* name; };
    const TermEnv terms[] = {
        {"ALACRITTY_SOCKET", "alacritty"},
        {"KITTY_WINDOW_ID", "kitty"},
        {"TERMINATOR_UUID", "terminator"},
        {"TILIX_ID", "tilix"},
        {"VTE_VERSION", "vte"},
        {"WEZTERM_PANE", "wezterm"},
        {"WT_SESSION", "windows terminal"},
        {"TERM_PROGRAM", ""}
    };
    for (auto &t : terms) {
        const char* val = std::getenv(t.var);
        if (val) {
            std::string name;
            if (std::string(t.var) == "TERM_PROGRAM") {
                std::string tmp = to_lower(val);
                if (tmp.find("vscode") != std::string::npos) name = "vscode";
                else if (tmp.find("iterm") != std::string::npos) name = "iterm";
                else if (tmp.find("hyper") != std::string::npos) name = "hyper";
                else name = tmp;
            } else name = t.name;
            return name;
        }
    }
    pid_t pid = getppid();
    for (int i = 0; i < 3; ++i) {
        std::ifstream f("/proc/" + std::to_string(pid) + "/cmdline");
        std::string cmd;
        if (!f) break;
        std::getline(f, cmd, '\0');
        if (!cmd.empty()) {
            size_t slash = cmd.find_last_of('/');
            if (slash != std::string::npos) cmd = cmd.substr(slash + 1);
            cmd = to_lower(cmd);
            if (cmd.find("alacritty") != std::string::npos) return "alacritty";
            if (cmd.find("kitty") != std::string::npos) return "kitty";
            if (cmd.find("gnome-terminal") != std::string::npos) return "gnome-terminal";
            if (cmd.find("konsole") != std::string::npos) return "konsole";
            if (cmd.find("terminator") != std::string::npos) return "terminator";
            if (cmd.find("tilix") != std::string::npos) return "tilix";
            if (cmd.find("wezterm") != std::string::npos) return "wezterm";
            if (cmd.find("xterm") != std::string::npos) return "xterm";
            if (cmd.find("vscode") != std::string::npos) return "vscode";
            return cmd;
        }
        std::ifstream statf("/proc/" + std::to_string(pid) + "/stat");
        std::string dummy; char par; pid_t ppid;
        statf >> dummy >> par >> ppid;
        if (!ppid) break;
        pid = ppid;
    }
    return "unknown";
}

inline std::string get_packages() {
    const std::vector<std::pair<std::string,std::string>> managers = {
        {"dpkg -l 2>/dev/null | wc -l", "apt"},
        {"pacman -Q 2>/dev/null | wc -l", "pacman"},
        {"rpm -qa 2>/dev/null | wc -l", "rpm"},
        {"dnf list installed 2>/dev/null | wc -l", "dnf"},
        {"flatpak list 2>/dev/null | wc -l", "flatpak"},
        {"snap list 2>/dev/null | wc -l", "snap"},
        {"brew list 2>/dev/null | wc -l", "brew"}
    };
    std::string result;
    for (auto &mgr : managers) {
        std::string count = exec_cmd(mgr.first);
        while (!count.empty() && (count.back() == '\n' || count.back() == '\r')) count.pop_back();
        if (!count.empty() && count != "0") {
            if (!result.empty()) result += ", ";
            result += count + " " + mgr.second;
        }
    }
    return result.empty() ? "none" : result;
}
