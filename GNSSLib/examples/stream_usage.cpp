// Sisi/GNSSLib/examples/stream_usage.cpp
//----------------------------------------------------------------------
// stream_usage.cpp:
//   Streaming use of GNSSLib: JSON config -> 1 ms complex<float> blocks
//   usage: stream_usage <config.json> [--ms N] [--gain G] [--bin file.bin]
//   --bin keeps the clean IQ .bin file writing alongside the stream.
//----------------------------------------------------------------------
#include <gnss/gnsslib.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "usage: " << argv[0] << " <config.json> [--ms N] [--gain G] [--bin file.bin]\n";
        return 0;
    }

    IfGenOptions o;
    o.configFile = argv[1];
    std::size_t maxMs = 0;
    for (int i = 2; i < argc; i++) {
        if (!std::strcmp(argv[i], "--ms") && i + 1 < argc)        maxMs = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--gain") && i + 1 < argc) o.gain = float(std::atof(argv[++i]));
        else if (!std::strcmp(argv[i], "--bin") && i + 1 < argc)  { o.writeBin = true; o.binFile = argv[++i]; }
    }

    IfGenEngine eng;
    try {
        eng.Init(o);
    } catch (const std::exception& e) {
        std::cerr << "Init failed: " << e.what() << "\n";
        return 1;
    }

    const IfGenInfo& info = eng.Info();
    std::cout << "fs = " << info.sampleRateHz / 1e6 << " MHz, fc = " << info.centerFreqHz / 1e6
              << " MHz, " << info.nChannels << " channels, " << info.totalBlocks << " ms\n";
    if (maxMs == 0 || maxMs > info.totalBlocks) maxMs = info.totalBlocks;

    double energy = 0.0;
    std::size_t samples = 0, blocks = 0;
    eng.Stream([&](const std::complex<float>* s, std::size_t n) {
        // <- ide jon a jammer osszekeveres / tovabbitas (pl. SCG StreamSource)
        for (std::size_t i = 0; i < n; i++) energy += std::norm(s[i]);
        samples += n;
        return ++blocks < maxMs;
    });
    eng.Close();

    std::cout << "streamed " << samples << " samples, RMS = "
              << (samples ? std::sqrt(energy / double(samples)) : 0.0) << "\n";
    return 0;
}