#include "support.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    opheap::stress::options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scale" && i + 1 < argc) {
            opts.scale = static_cast<unsigned>(std::stoul(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            opts.seed = std::stoull(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "usage: opheap_stress [--scale N] [--seed N] [scenario...]\n"
                         "  --scale N   multiplies default iteration/thread/payload counts (default 1)\n"
                         "  --seed N    seeds the deterministic RNGs used by randomized scenarios\n"
                         "  scenario    run only the named scenario(s); default runs all\n";
            return EXIT_SUCCESS;
        } else {
            opts.only.push_back(arg);
        }
    }
    if (opts.scale == 0) opts.scale = 1;
    return opheap::stress::run_all(opts);
}
