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
#include <iostream>
#include <string>
#include <vector>


int main()
{
    // ------------------------------------------------------------------
    // 1) GNSSConfig osszeallitasa
    //
    // Nincs JSON fajl.
    // A konfiguraciot kozvetlenul C++ strukturaibol epitjuk fel.
    // ------------------------------------------------------------------

    gnss::GNSSConfig config;

    // Simulation time
    config.time.year   = 2025;
    config.time.month  = 6;
    config.time.day    = 5;
    config.time.hour   = 10;
    config.time.minute = 5;
    config.time.second = 30.0;

    // Receiver position
    config.position.latitude  = 48.4928;
    config.position.longitude = -114.2847;
    config.position.altitude  = 100.0;

    // Receiver velocity
    config.velocity.speed  = 0.0;
    config.velocity.course = 318.91;

    // ------------------------------------------------------------------
    // 2) Dinamikus palya
    //
    // 180 masodpercig allando sebessegu szakasz.
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

    config.ephemerisFile =
        "../EphData/rinex_v3_20251560000.rnx";

    config.actualizeRinex = true;

    // ------------------------------------------------------------------
    // 4) Jelvalasztas
    //
    // Csak GPS L5 legyen aktiv.
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
    // ------------------------------------------------------------------

    config.output.sampleRate =
        21.0e6;

    config.output.centerFreq =
        1176.45e6;

    config.output.format =
        gnss::OutputFormat::IQ8;

    config.output.elevationMask =
        5.0;

    /*
     * Streaming pelda miatt most nem irunk .bin fajlt.
     */
    config.writeBin = false;

    // ------------------------------------------------------------------
    // 6) Power
    // ------------------------------------------------------------------

    config.power.noiseFloor =
        -172.0;

    config.power.initPower =
        50.0;

    config.power.elevationAdjust =
        true;

    // ------------------------------------------------------------------
    // 7) Engine/runtime beallitasok
    // ------------------------------------------------------------------

    config.verbose =
        true;

    /*
     * A streamelt complex<float> mintak amplitudo-skala.
     */
    config.gain =
        0.05f;

    // ------------------------------------------------------------------
    // 8) GNSSGenerator
    // ------------------------------------------------------------------

    gnss::GNSSGenerator generator;

    if (!generator.initialize(config)) {
        std::cerr
            << "GNSSGenerator initialization failed\n";

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

    if (!generator.openStream(
            false,     // writeBin
            "",        // binFile
            0.05f      // gain
        )) {

        std::cerr
            << "Failed to open GNSS stream\n";

        return 1;
    }

    // ------------------------------------------------------------------
    // 10) IQ mintak olvasasa
    // ------------------------------------------------------------------

    std::vector<std::complex<float>> samples(
        4000
    );

    std::size_t elementNumber = 0;

    while (true) {

        const std::size_t n =
            generator.read(
                samples.data(),
                samples.size()
            );

        if (n == 0) {
            break;
        }

        for (std::size_t i = 0; i < n; ++i) {

            const std::complex<float>& sample =
                samples[i];

            std::cout
                << "Element #" << elementNumber
                << " | size="
                << sizeof(sample)
                << " bytes"
                << " | I="
                << sample.real()
                << " | Q="
                << sample.imag()
                << "\n";

            ++elementNumber;
        }

        /*
         * Ez a pelda csak az elso blokkot mutatja meg.
         *
         * Valodi alkalmazasban ezt a break-et el lehet hagyni,
         * es a stream a teljes szimulacio vegeig futhat.
         */
        break;
    }

    // ------------------------------------------------------------------
    // 11) Stream lezárasa
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
