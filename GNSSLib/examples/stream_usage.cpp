// Sisi/GNSSLib/examples/stream_usage.cpp
//
// Pelda: kozvetlen IQ streaming callbackkel.
// Nincs JSON fajl.
//
// A callback minden alkalommal egy complex<float> sample blokkot kap.

#include <gnss/gnsslib.h>

#include <complex>
#include <cstddef>
#include <iostream>


int main()
{
    gnss::GNSSConfig config;

    // ------------------------------------------------------------------
    // Simulation time
    // ------------------------------------------------------------------

    config.time.year   = 2025;
    config.time.month  = 6;
    config.time.day    = 5;
    config.time.hour   = 10;
    config.time.minute = 5;
    config.time.second = 30.0;

    // ------------------------------------------------------------------
    // Receiver
    // ------------------------------------------------------------------

    config.position.latitude  = 48.4928;
    config.position.longitude = -114.2847;
    config.position.altitude  = 100.0;

    config.velocity.speed  = 0.0;
    config.velocity.course = 318.91;

    // ------------------------------------------------------------------
    // Navigation data
    // ------------------------------------------------------------------

    config.ephemerisFile =
        "../EphData/rinex_v3_20251560000.rnx";

    config.actualizeRinex = true;

    // ------------------------------------------------------------------
    // Signal
    // ------------------------------------------------------------------

    config.signals.push_back({
        gnss::SystemType::GPS,
        gnss::SignalType::GPS_L5,
        true
    });

    // ------------------------------------------------------------------
    // Output
    // ------------------------------------------------------------------

    config.output.sampleRate =
        21.0e6;

    config.output.centerFreq =
        1176.45e6;

    config.output.format =
        gnss::OutputFormat::IQ8;

    config.output.elevationMask =
        5.0;

    config.writeBin = false;

    // ------------------------------------------------------------------
    // Power
    // ------------------------------------------------------------------

    config.power.noiseFloor =
        -172.0;

    config.power.initPower =
        50.0;

    config.power.elevationAdjust =
        true;

    config.gain =
        0.05f;

    // ------------------------------------------------------------------
    // Initialize
    // ------------------------------------------------------------------

    gnss::GNSSGenerator generator;

    if (!generator.initialize(config)) {
        std::cerr
            << "Initialization failed\n";

        return 1;
    }

    // ------------------------------------------------------------------
    // Open stream
    // ------------------------------------------------------------------

    if (!generator.openStream()) {
        std::cerr
            << "Failed to open stream\n";

        return 1;
    }

    // ------------------------------------------------------------------
    // Callback alapú feldolgozas
    // ------------------------------------------------------------------

    std::size_t totalSamples = 0;

    const std::size_t processed =
        generator.stream(
            [&](const std::complex<float>* samples,
                std::size_t count)
            {
                totalSamples += count;

                std::cout
                    << "Received "
                    << count
                    << " samples\n";

                if (count > 0) {
                    std::cout
                        << "First sample: I="
                        << samples[0].real()
                        << " Q="
                        << samples[0].imag()
                        << "\n";
                }

                /*
                 * true  = folytatjuk
                 * false = leallitjuk a streamet
                 */
                return totalSamples < 4000;
            },
            4000
        );

    generator.closeStream();

    std::cout
        << "Processed samples: "
        << processed
        << "\n";

    return 0;
}
