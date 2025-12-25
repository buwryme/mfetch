#include <string>

#include "inc/config.hpp"
#include "inc/ascii.hpp"
#include "inc/util.hpp"

int main(int argc, char** argv) {
    std::string toml_file = "data/mfetch.toml";

    Config cfg = load_config(toml_file);

    auto info_lines = render_info_lines(cfg);
    print_with_ascii(cfg, info_lines);

    return 0;
}
