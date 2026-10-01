// Sisi/GNSSLib/include/gnss/IfGenEngine.h
// IfGenEngine.h  --  SignalSim IF generator as a library.
// A header szandekosan nem tartalmaz SignalSim tipust/makrot (pimpl), igy barhol include-olhato.
//
// Ket setup-ut van:
//   1) JSON alapu:      IfGenOptions + Init(const IfGenOptions&)      -- a JSON-t a library olvassa be.
//   2) Strukturalt:     IfGenParams  + Init(const IfGenParams&)       -- a hivo fel allitja ossze a
//      parametereket; a library ekkor semmilyen JSON-t nem olvas.
//
// Ketfele trajektoria-mod van (mindket setup-utnal):
//   a) szegmens-mod : kezdopozicio + kezdosebesseg + szegmenslista (ConstSpeed, ConstAcc, ...)
//   b) waypoint-mod : pontok listaja (opcionalis idovel / sebesseggel), linearis vagy spline
//      interpolacioval. Ha van waypoint, a position/velocity/trajectory[] NEM szamit.
//
// ValidateIfGenParams() a teljes motor felepitese nelkul ellenorzi a parametereket.
#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// JSON alapu setup
// ============================================================================

struct IfGenOptions {
    std::string configFile = "IfGenTest.json";
    bool        writeBin       = false;   // true -> a tiszta GNSS jel .bin fajlba is kiirodik
    std::string binFile;                  // ures -> a JSON output filename-je
    bool        writeTag       = false;   // <binFile>.tag (PocketSDR)
    bool        multiThread    = true;
    int         nThreads       = 0;       // 0 -> OpenMP auto
    bool        verbose        = false;   // osszefoglalo a stderr-re
    bool        validateOnly   = false;   // csatornakat nem epit, Read()/Stream() 0-t ad
    bool        actualizeRinex = true;    // ActualizeRinex() hivasa Init-ben
    std::uint64_t noiseSeed    = 1;       // reprodukalhato zaj
    float       gain           = 1.0f;    // a kiadott (stream) mintak skalaja; a .bin-t NEM erinti
};

// ============================================================================
// Strukturalt setup
// ============================================================================

// UTC ido.
struct IfGenTime {
    int    year   = 1980;
    int    month  = 1;
    int    day    = 6;
    int    hour   = 0;
    int    minute = 0;
    double second = 0.0;
};

// Kezdopozicio, fokban / meterben (csak szegmens-modban szamit).
struct IfGenPosition {
    double latitudeDeg  = 0.0;
    double longitudeDeg = 0.0;
    double altitudeM    = 0.0;
};

// Kezdosebesseg. course: 0 = eszak, 90 = kelet (csak szegmens-modban szamit).
struct IfGenVelocity {
    double speedMps  = 0.0;
    double courseDeg = 0.0;
};

// A SignalSim Trajectory.h szegmens-tipusai.
enum class IfGenTrajectoryType {
    ConstSpeed,
    ConstAcc,
    VerticalAcc,
    Jerk,
    HorizontalTurn
};

struct IfGenTrajectorySegment {
    IfGenTrajectoryType type = IfGenTrajectoryType::ConstSpeed;
    double durationS     = 0.0;   // szegmens hossza, s
    double speedMps      = 0.0;   // ConstSpeed (a SignalSim a meglevo sebesseget tartja, ezt az erteket nem hasznalja)
    double accelMps2     = 0.0;   // ConstAcc / VerticalAcc
    double jerkMps3      = 0.0;   // Jerk (m/s^3)
    double courseDeg     = 0.0;   // informativ, nem kerul at a CTrajectory-ba
    double radiusM       = 0.0;   // HorizontalTurn: ha != 0, ez hatarozza meg a fordulast (elojeles)
    double turnRateDegS  = 0.0;   // HorizontalTurn: elojeles fok/s (ha radiusM == 0)
};

// ---------------------------------------------------------------------------
// Waypoint-mod
// ---------------------------------------------------------------------------

// Egy utvonalpont. Fok / meter / masodperc.
struct IfGenWaypoint {
    double latitudeDeg  = 0.0;
    double longitudeDeg = 0.0;
    double altitudeM    = 0.0;
    double timeS        = -1.0;  // <0: nincs megadva. Ha megadod, MINDEN pontnal kell (szigoruan novekvo).
    double speedMps     = -1.0;  // <0: nincs megadva. Az ebbol a pontbol indulo szakasz sebessege
                                 // (csak ha nincs timeS)
};

enum class IfGenWaypointInterp { Linear, Spline };

// Tamogatott GNSS jelek. A nev egyertelmuen tartalmazza a rendszert is.
enum class IfGenSignal {
    GPS_L1CA, GPS_L1C, GPS_L2C, GPS_L2P, GPS_L5,
    BDS_B1C, BDS_B1I, BDS_B2I, BDS_B3I, BDS_B2a, BDS_B2b,
    GAL_E1, GAL_E5a, GAL_E5b, GAL_E5, GAL_E6,
    GLO_G1, GLO_G2, GLO_G3
};

enum class IfGenSystem {
    GPS,
    BDS,
    Galileo,
    GLONASS
};

struct IfGenSignalSelect {
    IfGenSignal signal = IfGenSignal::GPS_L1CA;
    IfGenSystem system = IfGenSystem::GPS;   // informativ; a signal neve is hordozza
    bool        enable = true;
};

enum class IfGenFormat { IQ2, IQ4, IQ8, IQ16 };

struct IfGenOutput {
    double      sampleRateHz     = 4.0e6;
    double      centerFreqHz     = 1575.42e6;
    IfGenFormat format           = IfGenFormat::IQ8;
    double      elevationMaskDeg = 5.0;
    std::string binFile;
};

struct IfGenPower {
    double noiseFloorDbm   = -172.0;
    double initCN0DbHz     = 47.0;
    bool   elevationAdjust = true;
};

struct IfGenParams {
    IfGenTime     time;
    IfGenPosition position;                            // szegmens-mod
    IfGenVelocity velocity;                            // szegmens-mod
    std::vector<IfGenTrajectorySegment> trajectory;    // szegmens-mod

    // Waypoint-mod: ha nem ures, a position/velocity/trajectory[] nem szamit
    // (a kezdopont az elso waypoint). trajectory[] ekkor ures kell legyen.
    std::vector<IfGenWaypoint> waypoints;
    IfGenWaypointInterp waypointInterp = IfGenWaypointInterp::Spline;
    double waypointDefaultSpeedMps = 0.0;   // ha nincs timeS es a pontnal nincs speedMps

    std::string ephemerisFile;
    bool        actualizeRinex = true;

    std::vector<IfGenSignalSelect> signals;            // legalabb 1 enable=true elem
    IfGenOutput output;
    IfGenPower  power;

    bool          writeBin     = false;
    bool          writeTag     = false;
    bool          multiThread  = true;
    int           nThreads     = 0;
    bool          verbose      = false;
    bool          validateOnly = false;
    std::uint64_t noiseSeed    = 1;
    float         gain         = 1.0f;
};

bool ValidateIfGenParams(const IfGenParams &params, std::vector<std::string> &errors);

// ============================================================================
// Az elore szamolt info a streamrol / .bin-rol
// ============================================================================

struct IfGenInfo {
    double      sampleRateHz    = 0.0;
    double      centerFreqHz    = 0.0;
    std::size_t samplesPerBlock = 0;      // 1 ms
    std::size_t totalBlocks     = 0;      // trajektoria hossza ms-ban
    int         nChannels       = 0;
    std::string outputFile;
    int         formatBits      = 0;
    std::size_t totalSamples() const { return samplesPerBlock * totalBlocks; }
};

class IfGenEngine {
public:
    IfGenEngine();
    ~IfGenEngine();
    IfGenEngine(const IfGenEngine&) = delete;
    IfGenEngine& operator=(const IfGenEngine&) = delete;

    void Init(const IfGenOptions &opt);
    void Init(const IfGenParams &params);

    bool IsOpen() const { return bool(impl); }
    const IfGenInfo& Info() const;

    // --- Pull API ---
    bool NextBlock(const std::complex<float> *&samples, std::size_t &n);
    std::size_t Read(std::complex<float> *out, std::size_t n);

    // --- Push API ---
    using SampleCallback = std::function<bool(const std::complex<float>*, std::size_t)>;
    std::size_t Stream(const SampleCallback &cb, std::size_t chunk = 0);

    void Close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};