// Sisi/GNSSLib/include/gnss/IfGenEngine.h
// IfGenEngine.h  --  SignalSim IF generator as a library.
// A header szandekosan nem tartalmaz SignalSim tipust/makrot (pimpl), igy barhol include-olhato.
#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

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

struct IfGenInfo {
    double      sampleRateHz    = 0.0;
    double      centerFreqHz    = 0.0;
    std::size_t samplesPerBlock = 0;      // 1 ms
    std::size_t totalBlocks     = 0;      // trajektoria hossza ms-ban
    int         nChannels       = 0;
    std::string outputFile;               // a JSON-beli (vagy felulirt) .bin fajlnev
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