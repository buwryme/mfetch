#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <array>
#include <iostream>
#include <iomanip>
#include <unordered_map>

namespace util_ns
{

    inline std::string to_lower_fn(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return std::tolower(c); });
        return s;
    }

    inline std::string to_upper_fn(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return std::toupper(c); });
        return s;
    }

    inline std::string trim_fn(std::string s)
    {
        const auto str_begin = s.find_first_not_of(" \t\n\r");
        if (str_begin == std::string::npos)
            return "";

        const auto str_end = s.find_last_not_of(" \t\n\r");
        const auto str_range = str_end - str_begin + 1;

        return s.substr(str_begin, str_range);
    }

    inline std::string exec_cmd_fn(const std::string &cmd)
    {
        std::array<char, 128> buffer;
        std::string result;
        // Use a safe wrapper or assume popen works enough
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((cmd + " 2>/dev/null").c_str(), "r"), pclose);

        if (!pipe)
            return "";

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

        return trim_fn(result);
    }

    inline std::vector<std::string> split_fn(const std::string &s, char delimiter)
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream token_stream(s);
        while (std::getline(token_stream, token, delimiter))
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    // Helper for Hex to Dec
    inline int hex_to_int(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return 0;
    }

    inline std::string color_fn(const std::string &text, const std::string &style_str)
    {
        if (style_str.empty())
            return text;

        std::string ansi = "";
        std::string s = to_lower_fn(style_str);

        // Parse space-separated tokens "bold red"
        std::istringstream ss(s);
        std::string token;

        while (ss >> token)
        {
            if (token == "bold")
                ansi += "1;";
            else if (token == "dim")
                ansi += "2;";
            else if (token == "italic")
                ansi += "3;";
            else if (token == "underline")
                ansi += "4;";
            else if (token == "blink")
                ansi += "5;";
            else if (token[0] == '#')
            {
                // Hex #RRGGBB
                if (token.length() >= 7)
                {
                    int r = (hex_to_int(token[1]) << 4) + hex_to_int(token[2]);
                    int g = (hex_to_int(token[3]) << 4) + hex_to_int(token[4]);
                    int b = (hex_to_int(token[5]) << 4) + hex_to_int(token[6]);
                    ansi += "38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + ";";
                }
            }
            else
            {
                // Standard Colors (foreground)
                if (token == "black")
                    ansi += "30;";
                else if (token == "red")
                    ansi += "31;";
                else if (token == "green")
                    ansi += "32;";
                else if (token == "yellow")
                    ansi += "33;";
                else if (token == "blue")
                    ansi += "34;";
                else if (token == "magenta")
                    ansi += "35;";
                else if (token == "cyan")
                    ansi += "36;";
                else if (token == "white")
                    ansi += "37;";
                else if (token == "reset")
                    ansi += "0;";
            }
        }

        if (!ansi.empty() && ansi.back() == ';')
            ansi.pop_back(); // Remove trailing ;
        if (ansi.empty())
            return text;

        return "\033[" + ansi + "m" + text + "\033[0m";
    }
} // namespace util_ns
