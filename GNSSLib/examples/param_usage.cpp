// Sisi/GNSSLib/examples/params_usage.cpp
//
// Pelda: GNSS szimulacio kozvetlen C++ parameterekkel.
// Nincs JSON fajl.
//
// Ez az ajanlott magas szintu library API:
//
//     GNSSConfig
//          |
//          v
//     GNSSGenerator
//          |
//          v
//     IfGenEngine
//
// Az alkalmazasnak nem kell kozvetlenul IfGenParams-t vagy
// IfGenEngine-t kezelnie.

#include <gnss/gnsslib.h>

#include <complex>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>


namespace {

// Egy kis segito a hibaokok szep kiiratasahoz.
void PrintError(const std::string& where, const std::string& what)
{
    std::cerr << "[" << where << "] " << what << "\n";
}

// A pelda altal hasznalt RINEX fajl. A munkakonyvtarhoz kepest
// ertelmezett relativ utvonal; ha nem letezik, a program
// ertheto hibauzenettel lep ki.
constexpr const char* kEphemerisFile = "../EphData/rinex_v3_20251560000.rnx";

} // namespace


int main()
{
    // ------------------------------------------------------------------
    // 0) A RINEX fajl elore ellenorzese.
    //
    // A ValidateIfGenParams() ugyis ellenorzi, de itt egy baratsagos
    // uzenetet adunk, mielott a generator inicializalasaba kezdenenk.
    // ------------------------------------------------------------------

    if (!std::filesystem::exists(kEphemerisFile)) {
        PrintError("params_usage",
                   std::string("RINEX file not found: ") + kEphemerisFile);
        std::cerr << "  (A peldat a build konyvtarabol kell futtatni, "
                     "vagy allitsd at a kEphemerisFile utvonalat.)\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // 1) GNSSConfig osszeallitasa
    //
    // Nincs JSON fajl.
    // A konfiguraciot kozvetlenul C++ strukturaibol epitjuk fel.
    // ------------------------------------------------------------------

    gnss::GNSSConfig config;

    // Simulation time (UTC)
    config.time.year   = 2025;
    config.time.month  = 6;
    config.time.day    = 5;
    config.time.hour   = 10;
    config.time.minute = 5;
    config.time.second = 30.0;

    // Receiver position (latitude/longitude in degrees, altitude in meters)
    config.position.latitude  = 48.4928;
    config.position.longitude = -114.2847;
    config.position.altitude  = 100.0;

    // Receiver velocity (speed in m/s, course in degrees)
    config.velocity.speed  = 0.0;
    config.velocity.course = 318.91;

    // ------------------------------------------------------------------
    // 2) Dinamikus palya
    //
    // 180 masodpercig allando sebessegu szakasz.
    //
    // A TrajectorySegment aggregate inicializalasa a mezosorrend
    // szerint tortenik:
    //     { type, duration, speed, acceleration, course, radius }
    // ------------------------------------------------------------------

    config.trajectory.push_back({
        gnss::TrajectoryType::ConstSpeed,
        180.0,   // duration [s]
        0.0,     // speed [m/s]
        0.0,     // acceleration
        0.0,     // course
        0.0      // radius
    });

    // ------------------------------------------------------------------
    // 3) RINEX navigacios fajl
    //
    // actualizeRinex=true eseten a library a szimulacio kezdetehez
    // igazithatja a RINEX ephemeriseket.
    // ------------------------------------------------------------------

    config.ephemerisFile = kEphemerisFile;
    config.actualizeRinex = true;

    // ------------------------------------------------------------------
    // 4) Jelvalasztas
    //
    // Csak GPS L5 legyen aktiv.
    //
    // FIGYELEM: a SignalConfig::system mezo informativ. A tenyleges
    // rendszert a SignalType neve hatarozza meg (GPS_L5 -> GPS).
    // A ToIfGenParams() atviszi a system mezot, de a SetupFromParams()
    // a signal nevebol kovetkezteti ki a GnssSystem-t.
    // ------------------------------------------------------------------

    config.signals.push_back({
        gnss::SystemType::GPS,
        gnss::SignalType::GPS_L5,
        true
    });

    // Pelda tovabbi jelre:
    //
    // config.signals.push_back({
    //     gnss::SystemType::GPS,
    //     gnss::SignalType::GPS_L1CA,
    //     true
    // });

    // ------------------------------------------------------------------
    // 5) Output
    //
    // A centerFreq-nek es a sampleRate-nek konzisztensnek kell lennie
    // a kivalasztott jellel: a FilterSignalsToBand() eltavolitja azokat
    // a jeleket, amelyeknek a kozepfrekvenciaja nem esik a
    // [centerFreq - sampleRate/2, centerFreq + sampleRate/2] savba.
    //
    // GPS L5 kozepe: 1176.45 MHz, ezert centerFreq = 1176.45e6.
    // ------------------------------------------------------------------

    config.output.sampleRate    = 21.0e6;
    config.output.centerFreq    = 1176.45e6;
    config.output.format        = gnss::OutputFormat::IQ8;
    config.output.elevationMask = 5.0;

    // Streaming pelda miatt most nem irunk .bin fajlt.
    config.writeBin = false;

    // ------------------------------------------------------------------
    // 6) Power
    // ------------------------------------------------------------------

    config.power.noiseFloor      = -172.0;
    config.power.initPower       = 50.0;
    config.power.elevationAdjust = true;

    // ------------------------------------------------------------------
    // 7) Engine/runtime beallitasok
    // ------------------------------------------------------------------

    config.verbose = true;

    // A streamelt complex<float> mintak amplitudo-skala.
    config.gain = 0.05f;

    // ------------------------------------------------------------------
    // 8) GNSSGenerator inicializalasa
    // ------------------------------------------------------------------

    gnss::GNSSGenerator generator;

    try {
        if (!generator.initialize(config)) {
            PrintError("params_usage",
                       "GNSSGenerator initialization failed "
                       "(see stderr for ValidateIfGenParams errors)");
            return 1;
        }
    }
    catch (const std::exception& e) {
        PrintError("params_usage",
                   std::string("initialize() threw: ") + e.what());
        return 1;
    }
    catch (...) {
        PrintError("params_usage",
                   "initialize() threw an unknown exception");
        return 1;
    }

    std::cout
        << "GNSSGenerator initialized successfully\n";

    // ------------------------------------------------------------------
    // 9) Streaming megnyitasa
    //
    // A library belul:
    //
    //     GNSSConfig
    //         ->
    //     IfGenParams
    //         ->
    //     IfGenEngine
    //
    // Nincs JSON.
    // ------------------------------------------------------------------

    try {
        if (!generator.openStream(
                false,     // writeBin
                "",        // binFile
                0.05f      // gain
            )) {
            PrintError("params_usage", "Failed to open GNSS stream");
            return 1;
        }
    }
    catch (const std::exception& e) {
        PrintError("params_usage",
                   std::string("openStream() threw: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 10) IQ mintak olvasasa
    //
    // A read() egy komplex float tombbe tolti a generalt mintakat.
    // A visszateresi ertek a tenylegesen beolvasott mintak szama;
    // 0 jelenti a szimulacio veget.
    //
    // Ez a pelda csak az elso nem-ures blokkot mutatja meg, majd
    // lezarja a streamet. Valodi alkalmazasban a ciklus a
    // szimulacio vegeig futhat.
    // ------------------------------------------------------------------

    std::vector<std::complex<float>> samples(4000);

    std::size_t elementNumber = 0;

    try {
        while (true) {
            const std::size_t n =
                generator.read(samples.data(), samples.size());

            if (n == 0) {
                break;
            }

            for (std::size_t i = 0; i < n; ++i) {
                const std::complex<float>& sample = samples[i];

                std::cout
                    << "Element #" << elementNumber
                    << " | size=" << sizeof(sample) << " bytes"
                    << " | I=" << sample.real()
                    << " | Q=" << sample.imag()
                    << "\n";

                ++elementNumber;
            }

            // Csak az elso blokk megjelenitese utan kilepunk.
            break;
        }
    }
    catch (const std::exception& e) {
        PrintError("params_usage",
                   std::string("read() threw: ") + e.what());
    }
    catch (...) {
        PrintError("params_usage",
                   "read() threw an unknown exception");
    }

    // ------------------------------------------------------------------
    // 11) Stream lezarasa
    // ------------------------------------------------------------------

    generator.closeStream();

    std::cout
        << "----------------------------------------\n";
    std::cout
        << "Received elements: "
        << elementNumber
        << "\n";

    return 0;
}