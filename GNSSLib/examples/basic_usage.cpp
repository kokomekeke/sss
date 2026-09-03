//----------------------------------------------------------------------
// basic_usage.cpp:
//   Basic usage example of GNSSLib
//----------------------------------------------------------------------

#include <gnss/gnsslib.h>
#include <iostream>

using namespace gnss;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "GNSSLib - Basic Usage Example" << std::endl;
    std::cout << "========================================" << std::endl;

    // Create configuration
    GNSSConfig config;

    // Set time (UTC)
    config.time.year = 2021;
    config.time.month = 6;
    config.time.day = 19;
    config.time.hour = 0;
    config.time.minute = 0;
    config.time.second = 0.0;

    // Set position (Budapest)
    config.position.latitude = 47.492721;
    config.position.longitude = 19.015773;
    config.position.altitude = 20.0;

    // Set velocity (static)
    config.velocity.speed = 0.0;
    config.velocity.course = 0.0;

    // Add trajectory (static for 60 seconds)
    TrajectorySegment traj;
    traj.type = TrajectoryType::ConstSpeed;
    traj.duration = 60.0;
    traj.speed = 0.0;
    config.trajectory.push_back(traj);

    // Set ephemeris file
    config.ephemerisFile = "../EphData/BRDC00IGS_R_20211700000_01D_MN.rnx";

    // Output configuration
    config.output.format = OutputFormat::IQ16;
    config.output.sampleRate = 4.0e6;
    config.output.centerFreq = 1575.42e6;
    config.output.filename = "gnss_output.bin";
    config.output.elevationMask = 5.0;

    // Power configuration
    config.power.noiseFloor = -172.0;
    config.power.initPower = 47.0;
    config.power.elevationAdjust = true;

    // Add signals
    SignalConfig gps;
    gps.system = SystemType::GPS;
    gps.signal = SignalType::GPS_L1CA;
    gps.enable = true;
    config.signals.push_back(gps);

    SignalConfig gal;
    gal.system = SystemType::Galileo;
    gal.signal = SignalType::GAL_E1;
    gal.enable = true;
    config.signals.push_back(gal);

    // Create generator
    GNSSGenerator generator;

    // Initialize
    std::cout << "\nInitializing generator..." << std::endl;
    if (!generator.initialize(config)) {
        std::cerr << "Failed to initialize!" << std::endl;
        return 1;
    }
    std::cout << "✓ Generator initialized successfully" << std::endl;

    // Generate
    std::cout << "\nGenerating 60 seconds of IF data..." << std::endl;
    if (!generator.generate(60.0)) {
        std::cerr << "Failed to generate!" << std::endl;
        return 1;
    }
    std::cout << "✓ Generation complete" << std::endl;

    // Show statistics
    auto stats = generator.getStats();
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Duration: " << stats.duration << " seconds" << std::endl;
    std::cout << "  Data size: " << stats.dataSizeMB << " MB" << std::endl;
    std::cout << "  Generation time: " << stats.generationTime << " seconds" << std::endl;
    std::cout << "  Visible satellites: " << stats.visibleSatellites << std::endl;

    // Save to file
    std::cout << "\nSaving to file: " << config.output.filename << std::endl;
    if (!generator.saveToFile()) {
        std::cerr << "Failed to save!" << std::endl;
        return 1;
    }
    std::cout << "✓ File saved successfully" << std::endl;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Done!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}