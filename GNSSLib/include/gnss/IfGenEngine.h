// Sisi/GNSSLib/include/gnss/IfGenEngine.h
// IfGenEngine.h  --  SignalSim IF generator as a library.
// A header szandekosan nem tartalmaz SignalSim tipust/makrot (pimpl), igy barhol include-olhato.
//
// Ket setup-ut van:
//   1) JSON alapu:      IfGenOptions + Init(const IfGenOptions&)      -- a JSON-t a library olvassa be.
//   2) Strukturalt:      IfGenParams  + Init(const IfGenParams&)      -- a hivo fel (pl. SCG) allitja
//      ossze a parametereket a sajat (pl. MQTT-n erkezo) formatumabol; a library ekkor semmilyen
//      JSON-t nem olvas. Ez az ut valositja meg azt, hogy "a setup fuggveny vegye at a parametereket,
//      ne a signalsim/gnss oldal olvassa ki a hivo sajat formatumat".
//
// ValidateIfGenParams() a teljes motor felepitese (muholdcsatornak, RINEX beolvasas) nelkul ellenorzi
// a parametereket, hogy hiba eseten meg a felvetel/teszt elfogadasa elott kiderulhessen.
#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// JSON alapu setup (meglevo ut, valtozatlan)
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
// Strukturalt setup: a hivo fel epiti fel a sajat formatumabol.
//
// A mertekegysegek es koordinatak SI/fok alapuak (nem a SignalSim belso radian/kHz
// reprezentacioja), hogy a hivo oldalnak ne kelljen ismernie a SignalSim konvencioit.
// ============================================================================

// UTC ido. A hetfovel (Week) es a leap secondokkal a library szamol, ezt nem kell megadni.
struct IfGenTime {
    int    year   = 1980;
    int    month  = 1;
    int    day    = 6;
    int    hour   = 0;
    int    minute = 0;
    double second = 0.0;
};

// Kezdopozicio, fokban / meterben.
struct IfGenPosition {
    double latitudeDeg  = 0.0;
    double longitudeDeg = 0.0;
    double altitudeM    = 0.0;
};

// Kezdosebesseg. course: 0 = eszak, 90 = kelet.
struct IfGenVelocity {
    double speedMps  = 0.0;
    double courseDeg = 0.0;
};

// A CTrajectory harom tamogatott szegmens-tipusa.
enum class IfGenTrajectoryType {
    ConstSpeed,     // allando sebesseg `durationS` masodpercig
    ConstAcc,       // allando gyorsulas `durationS` masodpercig
    HorizontalTurn  // vizszintes kanyar allando szogsebesseggel `durationS` masodpercig
};

struct IfGenTrajectorySegment {
    IfGenTrajectoryType type = IfGenTrajectoryType::ConstSpeed;
    double durationS    = 0.0;   // szegmens hossza, s
    double speedMps      = 0.0;  // ConstSpeed eseten
    double accelMps2     = 0.0;  // ConstAcc eseten
    double turnRateDegS  = 0.0;  // HorizontalTurn eseten, elojeles fok/s
};

// Tamogatott GNSS jelek. A nev egyertelmuen tartalmazza a rendszert is, ez donti el a
// belso GnssSystem + jel-index parost; a rendszer nem kulon parameter.
enum class IfGenSignal {
    GPS_L1CA, GPS_L1C, GPS_L2C, GPS_L2P, GPS_L5,
    BDS_B1C, BDS_B1I, BDS_B2I, BDS_B3I, BDS_B2a, BDS_B2b,
    GAL_E1, GAL_E5a, GAL_E5b, GAL_E5, GAL_E6,
    GLO_G1, GLO_G2
};

struct IfGenSignalSelect {
    IfGenSignal signal;
    bool        enable = true;
};

enum class IfGenFormat { IQ2, IQ4, IQ8, IQ16 };

struct IfGenOutput {
    double      sampleRateHz    = 4.0e6;
    double      centerFreqHz    = 1575.42e6;
    IfGenFormat format          = IfGenFormat::IQ8;
    double      elevationMaskDeg = 5.0;
    std::string binFile;          // csak akkor kell, ha writeBin=true; ures -> "output.bin"
};

struct IfGenPower {
    double noiseFloorDbm  = -172.0;
    double initCN0DbHz    = 47.0;
    bool   elevationAdjust = true;
};

struct IfGenParams {
    IfGenTime     time;
    IfGenPosition position;
    IfGenVelocity velocity;
    std::vector<IfGenTrajectorySegment> trajectory;   // legalabb 1 elem

    std::string ephemerisFile;                        // RINEX nav fajl eleresi utja
    bool        actualizeRinex = true;                // az ephemerist a `time`-hoz aktualizalja-e

    std::vector<IfGenSignalSelect> signals;            // legalabb 1 enable=true elem
    IfGenOutput output;
    IfGenPower  power;

    // motor-szintu beallitasok (megfelelnek az IfGenOptions azonos nevu mezoinek)
    bool          writeBin     = false;
    bool          writeTag     = false;
    bool          multiThread  = true;
    int           nThreads     = 0;
    bool          verbose      = false;
    bool          validateOnly = false;
    std::uint64_t noiseSeed    = 1;
    float         gain         = 1.0f;
};

// Ellenorzi a parametereket a motor felepitese (RINEX beolvasas, csatornak) nelkul: ertektartomanyok,
// legalabb egy engedelyezett jel, a RINEX fajl letezik-e. `errors` ures marad, ha minden rendben.
// Hivhato a teszt elfogadasa elott, meg jóval a tenyleges felvetel/Init() elott.
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
    std::string outputFile;               // a (JSON-beli vagy felulirt) .bin fajlnev
    int         formatBits      = 0;      // .bin formatum: 2 / 4 / 8 / 16 bit komponensenkent
    std::size_t totalSamples() const { return samplesPerBlock * totalBlocks; }
};

class IfGenEngine {
public:
    IfGenEngine();
    ~IfGenEngine();
    IfGenEngine(const IfGenEngine&) = delete;
    IfGenEngine& operator=(const IfGenEngine&) = delete;

    // JSON + RINEX beolvasas, muholdak/csatornak felepitese. Hiba: std::runtime_error.
    void Init(const IfGenOptions &opt);

    // Strukturalt setup: nem olvas semmilyen JSON-t, csak a `params`-ban kapott ertekekbol dolgozik.
    // Elobb ValidateIfGenParams()-t hivja; ervenytelen parameterek eseten std::runtime_error-t dob,
    // az osszegyujtott hibauzenetekkel egyutt. Utana ugyanugy RINEX-et olvas es aktualizal, csatornakat
    // epit, mint a JSON-os ut.
    void Init(const IfGenParams &params);

    bool IsOpen() const { return bool(impl); }
    const IfGenInfo& Info() const;

    // --- Pull API ---
    // Egy 1 ms-os blokk (zero-copy, a kovetkezo hivasig ervenyes). false = vege a trajektoriának.
    bool NextBlock(const std::complex<float> *&samples, std::size_t &n);
    // Tetszoleges meretu olvasas; visszaadja a kiirt mintak szamat (< n, ha vege).
    std::size_t Read(std::complex<float> *out, std::size_t n);

    // --- Push API ---
    // Vegigstreameli a szimulaciot. chunk=0 -> natív 1 ms blokkok, egyebkent pontosan chunk minta
    // (az utolso rovidebb lehet). A callback false-szal megszakithatja. Visszaadja az atadott mintak szamat.
    using SampleCallback = std::function<bool(const std::complex<float>*, std::size_t)>;
    std::size_t Stream(const SampleCallback &cb, std::size_t chunk = 0);

    // .bin flush + lezaras, erőforrasok felszabaditasa. Tobbszor hivhato.
    void Close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};