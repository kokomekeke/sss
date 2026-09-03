//----------------------------------------------------------------------
// json_usage.cpp:
//   Example using JSON configuration with GNSSLib
//----------------------------------------------------------------------

#include <gnss/gnsslib.h>
#include <iostream>

using namespace gnss;

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "GNSSLib - JSON Configuration Example" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* jsonFile = (argc > 1) ? argv[1] : "config.json";

    std::cout << "\nLoading configuration from: " << jsonFile << std::endl;

    GNSSGenerator generator;

    if (!generator.initializeFromJSON(jsonFile)) {
        std::cerr << "Failed to initialize from JSON!" << std::endl;
        return 1;
    }

    std::cout << "✓ Configuration loaded successfully" << std::endl;

    auto config = generator.getConfig();
    std::cout << "\nConfiguration Summary:" << std::endl;
    std::cout << "  Time: " << Utils::formatTime(config.time) << std::endl;
    std::cout << "  Position: " << config.position.latitude << "°, "
              << config.position.longitude << "°" << std::endl;
    std::cout << "  Ephemeris: " << config.ephemerisFile << std::endl;
    std::cout << "  Signals: " << config.signals.size() << " enabled" << std::endl;
    std::cout << "  Duration: " << config.trajectory[0].duration << "s" << std::endl;

    std::cout << "\nGenerating signal..." << std::endl;

    if (!generator.generate(300.0)) {
        std::cerr << "Generation failed!" << std::endl;
        return 1;
    }

    auto stats = generator.getStats();
    std::cout << "\n✓ Generation complete!" << std::endl;
    std::cout << "  Data size: " << stats.dataSizeMB << " MB" << std::endl;
    std::cout << "  Generation time: " << stats.generationTime << "s" << std::endl;

    return 0;
}