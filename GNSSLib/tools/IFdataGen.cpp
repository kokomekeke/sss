// Sisi/GNSSLib/tools/IFdataGen.cpp
// IfDataGenMain.cpp -- vekony CLI a konyvtar fole. A regi main.cpp-t ez valtja le
// (a regi globalok es a monolitikus main NEM kerulhetnek bele a konyvtarba).
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include "gnss/IfGenEngine.h"

static void ShowHelp(const char *prog) {
    std::cout << "IFDataGen - GNSS IF Data Generator\n\n"
              << "Usage: " << prog << " -c <config.json> [options]\n\n"
              << "   -c,  --config <FILE>   Configuration file (JSON)\n"
              << "   -o,  --output <FILE>   Output IF data file (overrides config)\n"
              << "   -vo, --validate-only   Validate configuration and exit\n"
              << "   -mt, --multi-thread    Force multi-thread (default)\n"
              << "   -st, --single-thread   Force single-thread\n"
              << "   -t,  --tag             Write <output>.tag file\n"
              << "   -h,  --help            Show this help\n";
}

int main(int argc, char **argv) {
    if (argc == 1) { ShowHelp(argv[0]); return 0; }

    IfGenOptions o;
    o.writeBin = true;          // a CLI mindig fajlba ir
    o.verbose = true;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto needArg = [&]() -> const char* {
            if (i + 1 >= argc || argv[i + 1][0] == '-') { std::cerr << "[ERROR] " << a << " requires an argument\n"; return nullptr; }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") { ShowHelp(argv[0]); return 0; }
        else if (a == "-c" || a == "--config") { auto v = needArg(); if (!v) return 1; o.configFile = v; }
        else if (a == "-o" || a == "--output") { auto v = needArg(); if (!v) return 1; o.binFile = v; }
        else if (a == "-vo" || a == "--validate-only") o.validateOnly = true;
        else if (a == "-mt" || a == "--multi-thread") o.multiThread = true;
        else if (a == "-st" || a == "--single-thread") o.multiThread = false;
        else if (a == "-t" || a == "--tag") o.writeTag = true;
        else std::cerr << "[WARNING] Unknown option " << a << "\n";
    }

    try {
        IfGenEngine eng;
        eng.Init(o);
        const IfGenInfo &info = eng.Info();
        if (o.validateOnly) { std::cerr << "Configuration validation completed\n"; return 0; }

        auto t0 = std::chrono::steady_clock::now();
        std::size_t blocks = 0;
        eng.Stream([&](const std::complex<float> *, std::size_t) {
            if (++blocks % 25 == 0) {
                double pct = info.totalBlocks ? 100.0 * blocks / info.totalBlocks : 0.0;
                std::fprintf(stderr, "\r[IfGen] %6.2f%%  %zu/%zu ms", pct, blocks, info.totalBlocks);
            }
            return true;
        });
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::fprintf(stderr, "\r[IfGen] done: %zu ms of signal in %.2f s\n", blocks, s);
        eng.Close();
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
    return 0;
}