// Sisi/GNSSLib/examples/basic_usage.cpp
//----------------------------------------------------------------------
// basic_usage.cpp:
//   Batch use of GNSSLib: JSON config -> .bin IF file
//   usage: basic_usage <config.json> [duration_seconds] [output.bin]
//----------------------------------------------------------------------
#include <gnss/gnsslib.h>
#include <cstdlib>
#include <iostream>

using namespace gnss;

int main(int argc, char* argv[]) {
    std::cout << "GNSSLib - Basic Usage Example\n";

    if (argc < 2) {
        std::cout << "usage: " << argv[0] << " <config.json> [duration_s (0 = all)] [output.bin]\n";
        return 0;
    }

    GNSSGenerator generator;
    if (!generator.initializeFromJSON(argv[1])) {
        std::cerr << "Failed to initialize from JSON!\n";
        return 1;
    }
    if (argc > 3) generator.setOutputFile(argv[3]);

    const auto& out = generator.getConfig().output;
    std::cout << "  Sample rate : " << out.sampleRate / 1e6 << " MHz\n"
              << "  Center freq : " << out.centerFreq / 1e6 << " MHz\n"
              << "  Output file : " << out.filename << "\n";

    double duration = (argc > 2) ? std::atof(argv[2]) : 0.0;
    if (!generator.generate(duration)) {
        std::cerr << "Generation failed!\n";
        return 1;
    }

    const auto& stats = generator.getStats();
    std::cout << "Done: " << stats.duration << " s, " << stats.dataSizeMB
              << " MB, generation time " << stats.generationTime << " s\n";
    return 0;
}