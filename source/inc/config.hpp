#pragma once

#include <string>
#include <unordered_map>
#include <toml++/toml.h>

struct Config {
    std::string ascii_art;
    std::string ascii_position = "left";
    int ascii_width = 30;
    int ascii_padding = 2;
    int indent = 2;

    // preferred unit (gb or mb)
    std::string unit = "gb";

    // formatting templates per-field
    std::unordered_map<std::string, std::string> field_templates;

    int top_padding = 0; int bottom_padding = 0;
};

inline Config load_config(const std::string& path) {
    Config cfg;
    try {
        auto doc = toml::parse_file(path);
        if (auto node = doc["ascii"]["art"].value<std::string>()) cfg.ascii_art = *node;
        if (auto node = doc["ascii"]["position"].value<std::string>()) cfg.ascii_position = *node;
        if (auto node = doc["ascii"]["width"].value<int>()) cfg.ascii_width = *node;
        if (auto node = doc["ascii"]["padding"].value<int>()) cfg.ascii_padding = *node;
        if (auto node = doc["display"]["indent"].value<int>()) cfg.indent = *node;
        if (auto node = doc["display"]["unit"].value<std::string>()) cfg.unit = *node;
        if (auto node = doc["display"]["top_padding"].value<int>()) cfg.top_padding = *node;
        if (auto node = doc["display"]["bottom_padding"].value<int>()) cfg.bottom_padding = *node;


        if (auto fmt = doc["format"].as_table()) {
            for (auto &p : *fmt) {
                if (auto s = p.second.value<std::string>()) {
                    cfg.field_templates.emplace(std::string(p.first.str()), *s);
                }
            }
        }
    } catch (const toml::parse_error& e) {
        throw std::runtime_error("failed to parse: " + std::string(e.what()));
    }

    if (cfg.ascii_position.empty()) cfg.ascii_position = "left";
    if (cfg.field_templates.find("default") == cfg.field_templates.end()) {
        cfg.field_templates["default"] = "[k] [v]";
    }
    return cfg;
}
