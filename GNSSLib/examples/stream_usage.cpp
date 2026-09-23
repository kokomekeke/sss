// Sisi/GNSSLib/examples/stream_usage.cpp
//
// Pelda: kozvetlen IQ streaming callbackkel.
// Nincs JSON fajl.
//
// A callback minden alkalommal egy complex<float> sample blokkot kap.
// A callback visszateresi erteke:
//     true  -> folytatjuk a streamelest
//     false -> leallitjuk a streamet

#include <gnss/gnsslib.h>

#include <complex>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>


namespace {

void PrintError(const std::string& where, const std::string& what)
{
    std::cerr << "[" << where << "] " << what << "\n";
}

constexpr const char* kEphemerisFile = "../EphData/rinex_v3_20251560000.rnx";

} // namespace


int main()
{
    // ------------------------------------------------------------------
    // 0) A RINEX fajl elore ellenorzese
    // ------------------------------------------------------------------

    if (!std::filesystem::exists(kEphemerisFile)) {
        PrintError("stream_usage",
                   std::string("RINEX file not found: ") + kEphemerisFile);
        std::cerr << "  (A peldat a build konyvtarabol kell futtatni, "
                     "vagy allitsd at a kEphemerisFile utvonalat.)\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // GNSSConfig
    // ------------------------------------------------------------------

    gnss::GNSSConfig config;

    // Simulation time (UTC)
    config.time.year   = 2025;
    config.time.month  = 6;
    config.time.day    = 5;
    config.time.hour   = 10;
    config.time.minute = 5;
    config.time.second = 30.0;

    // Receiver
    config.position.latitude  = 48.4928;
    config.position.longitude = -114.2847;
    config.position.altitude  = 100.0;

    config.velocity.speed  = 0.0;
    config.velocity.course = 318.91;

    // Navigation data
    config.ephemerisFile  = kEphemerisFile;
    config.actualizeRinex = true;

    // ------------------------------------------------------------------
    // Signal
    //
    // A SignalConfig::system mezo informativ; a tenyleges rendszert
    // a SignalType neve hatarozza meg.
    // ------------------------------------------------------------------

    config.signals.push_back({
        gnss::SystemType::GPS,
        gnss::SignalType::GPS_L5,
        true
    });

    // ------------------------------------------------------------------
    // Output
    //
    // GPS L5 -> 1176.45 MHz kozeppont.
    // ------------------------------------------------------------------

    config.output.sampleRate    = 21.0e6;
    config.output.centerFreq    = 1176.45e6;
    config.output.format        = gnss::OutputFormat::IQ8;
    config.output.elevationMask = 5.0;

    config.writeBin = false;

    // ------------------------------------------------------------------
    // Power
    // ------------------------------------------------------------------

    config.power.noiseFloor      = -172.0;
    config.power.initPower       = 50.0;
    config.power.elevationAdjust = true;

    config.gain = 0.05f;

    // ------------------------------------------------------------------
    // Initialize
    // ------------------------------------------------------------------

    gnss::GNSSGenerator generator;

    try {
        if (!generator.initialize(config)) {
            PrintError("stream_usage",
                       "Initialization failed "
                       "(see stderr for ValidateIfGenParams errors)");
            return 1;
        }
    }
    catch (const std::exception& e) {
        PrintError("stream_usage",
                   std::string("initialize() threw: ") + e.what());
        return 1;
    }
    catch (...) {
        PrintError("stream_usage",
                   "initialize() threw an unknown exception");
        return 1;
    }

    // ------------------------------------------------------------------
    // Open stream
    //
    // A default parameterekkel: writeBin=false, binFile="", gain=1.0f.
    // A tenyleges gain a config.gain-bol szarmazik, mert az
    // openStream() felulirja a config-ot a sajat parameterjeivel.
    // Itt explicit 0.05f-et adunk, hogy egyertelmu legyen.
    // ------------------------------------------------------------------

    try {
        if (!generator.openStream(
                false,   // writeBin
                "",      // binFile
                0.05f    // gain
            )) {
            PrintError("stream_usage", "Failed to open stream");
            return 1;
        }
    }
    catch (const std::exception& e) {
        PrintError("stream_usage",
                   std::string("openStream() threw: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // Callback alapu feldolgozas
    //
    // A stream() metodus chunk=4000 mintankent hivja a callbacket.
    // A callback visszateresi erteke false -> leallitas.
    //
    // Itt 4000 minta utan leallunk (totalSamples < 4000 feltetel).
    // ------------------------------------------------------------------

    std::size_t totalSamples = 0;

    std::size_t processed = 0;

    try {
        processed = generator.stream(
            [&](const std::complex<float>* samples,
                std::size_t count)
            {
                totalSamples += count;

                std::cout
                    << "Received " << count << " samples\n";

                if (count > 0) {
                    std::cout
                        << "First sample: I=" << samples[0].real()
                        << " Q=" << samples[0].imag()
                        << "\n";
                }

                // true  = folytatjuk
                // false = leallitjuk a streamet
                return totalSamples < 4000;
            },
            4000
        );
    }
    catch (const std::exception& e) {
        PrintError("stream_usage",
                   std::string("stream() threw: ") + e.what());
    }
    catch (...) {
        PrintError("stream_usage",
                   "stream() threw an unknown exception");
    }

    generator.closeStream();

    std::cout
        << "----------------------------------------\n";
    std::cout
        << "Processed samples: " << processed
        << "\n";

    return 0;
}