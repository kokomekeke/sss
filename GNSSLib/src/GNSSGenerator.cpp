// Sisi/GNSSLib/src/GNSSGenerator.cpp
//----------------------------------------------------------------------
// GNSSGenerator.cpp:
//   Implementation of the unified GNSS library on top of IfGenEngine
//----------------------------------------------------------------------
#include "gnss/gnsslib.h"

#include "SignalSim.h"       // csak az Utils fuggvenyekhez (Coordinate, GnssTime, ...)

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace gnss {

struct GNSSGenerator::Impl {
    std::string jsonFile;
    std::string outputOverride;   // ures -> a JSON output filename-je
    std::string producedFile;     // a generate() altal irt fajl
};

GNSSGenerator::GNSSGenerator() : m_impl(std::make_unique<Impl>()) {}
GNSSGenerator::~GNSSGenerator() = default;

bool GNSSGenerator::initialize(const GNSSConfig&) {
    std::cerr << "GNSSGenerator::initialize(GNSSConfig) is not supported; use initializeFromJSON()\n";
    return false;
}

bool GNSSGenerator::initializeFromJSON(const std::string& jsonFile) {
    try {
        // Olcso probe: JSON + RINEX beolvasas, csatornak epitese nelkul.
        IfGenEngine probe;
        IfGenOptions o;
        o.configFile     = jsonFile;
        o.validateOnly   = true;
        o.actualizeRinex = false;
        probe.Init(o);
        const IfGenInfo& i = probe.Info();

        m_impl->jsonFile = jsonFile;
        m_config.output.filename   = i.outputFile;
        m_config.output.sampleRate = i.sampleRateHz;
        m_config.output.centerFreq = i.centerFreqHz;
        switch (i.formatBits) {
            case 2:  m_config.output.format = OutputFormat::IQ2;  break;
            case 4:  m_config.output.format = OutputFormat::IQ4;  break;
            case 16: m_config.output.format = OutputFormat::IQ16; break;
            default: m_config.output.format = OutputFormat::IQ8;  break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize from JSON '" << jsonFile << "': " << e.what() << std::endl;
        return false;
    }
    m_initialized = true;
    return true;
}

void GNSSGenerator::setOutputFile(const std::string& filename) {
    m_impl->outputOverride = filename;
    if (!filename.empty()) m_config.output.filename = filename;
}

bool GNSSGenerator::openStream(IfGenEngine& engine, bool writeBin,
                               const std::string& binFile, float gain) {
    if (!m_initialized) {
        std::cerr << "Generator not initialized. Call initializeFromJSON() first." << std::endl;
        return false;
    }
    try {
        IfGenOptions o;
        o.configFile = m_impl->jsonFile;
        o.writeBin   = writeBin;
        o.binFile    = !binFile.empty() ? binFile : m_impl->outputOverride;
        o.gain       = gain;
        engine.Init(o);
    } catch (const std::exception& e) {
        std::cerr << "openStream failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool GNSSGenerator::generate(double durationSeconds) {
    return generate(static_cast<int>(durationSeconds * 1000.0));
}

bool GNSSGenerator::generate(int durationMs) {
    if (!m_initialized) {
        std::cerr << "Generator not initialized. Call initializeFromJSON() first." << std::endl;
        return false;
    }
    auto t0 = std::chrono::steady_clock::now();
    try {
        IfGenEngine eng;
        IfGenOptions o;
        o.configFile = m_impl->jsonFile;
        o.writeBin   = true;
        o.binFile    = m_impl->outputOverride;
        o.writeTag   = m_config.output.generateTag;
        eng.Init(o);
        const IfGenInfo info = eng.Info();

        const std::size_t maxBlocks = durationMs > 0 ? std::size_t(durationMs)
                                                     : std::numeric_limits<std::size_t>::max();
        std::size_t blocks = 0;
        eng.Stream([&](const std::complex<float>*, std::size_t) { return ++blocks < maxBlocks; });
        eng.Close();                                     // .bin flush + lezaras

        double bytesPerSample = info.formatBits == 2 ? 0.5 : info.formatBits == 4 ? 1.0
                              : info.formatBits == 16 ? 4.0 : 2.0;
        double samples = double(blocks) * double(info.samplesPerBlock);
        m_stats = GenerationStats();
        m_stats.duration     = blocks / 1000.0;
        m_stats.dataSizeMB   = samples * bytesPerSample / (1024.0 * 1024.0);
        m_stats.totalSamples = samples > 2147483647.0 ? std::numeric_limits<int>::max() : int(samples);
        m_impl->producedFile = info.outputFile;
        m_config.output.filename = info.outputFile;
    } catch (const std::exception& e) {
        std::cerr << "generate failed: " << e.what() << std::endl;
        return false;
    }
    m_stats.generationTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return true;
}

bool GNSSGenerator::saveToFile(const std::string& filename) {
    if (m_impl->producedFile.empty()) {
        std::cerr << "Nothing generated yet. Call generate() first." << std::endl;
        return false;
    }
    if (filename.empty() || filename == m_impl->producedFile) return true;
    std::error_code ec;
    std::filesystem::copy_file(m_impl->producedFile, filename,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Failed to copy output file: " << ec.message() << std::endl;
        return false;
    }
    return true;
}

void GNSSGenerator::clear() {
    m_impl = std::make_unique<Impl>();
    m_data.clear();
    m_stats = GenerationStats();
    m_config = GNSSConfig();
    m_initialized = false;
}

//----------------------------------------------------------------------
// Utility Functions
//----------------------------------------------------------------------

namespace Utils {

bool utcToGpsTime(int year, int month, int day, int hour, int minute,
                  double second, int& week, int& msOfWeek) {
    UTC_TIME utc;
    utc.Year = year; utc.Month = month; utc.Day = day;
    utc.Hour = hour; utc.Minute = minute; utc.Second = second;
    GNSS_TIME gps = UtcToGpsTime(utc, FALSE);
    week = gps.Week;
    msOfWeek = gps.MilliSeconds;
    return true;
}

void llaToEcef(double lat, double lon, double alt, double& x, double& y, double& z) {
    LLA_POSITION lla;
    lla.lat = lat * PI / 180.0;
    lla.lon = lon * PI / 180.0;
    lla.alt = alt;
    KINEMATIC_INFO ecef = LlaToEcef(lla);
    x = ecef.x; y = ecef.y; z = ecef.z;
}

void ecefToLla(double x, double y, double z, double& lat, double& lon, double& alt) {
    KINEMATIC_INFO ecef;
    ecef.x = x; ecef.y = y; ecef.z = z;
    LLA_POSITION lla = EcefToLla(ecef);
    lat = lla.lat * 180.0 / PI;
    lon = lla.lon * 180.0 / PI;
    alt = lla.alt;
}

static void elAz(double lat, double lon, double alt, double sx, double sy, double sz,
                 double& el, double& az) {
    LLA_POSITION lla;
    lla.lat = lat * PI / 180.0;
    lla.lon = lon * PI / 180.0;
    lla.alt = alt;
    KINEMATIC_INFO receiver = LlaToEcef(lla);
    KINEMATIC_INFO satellite;
    satellite.x = sx; satellite.y = sy; satellite.z = sz;
    el = 0.0; az = 0.0;
    SatElAz(&receiver, &satellite, &el, &az);
}

double calculateElevation(double lat, double lon, double alt, double sx, double sy, double sz) {
    double el, az;
    elAz(lat, lon, alt, sx, sy, sz, el, az);
    return el * 180.0 / PI;
}

double calculateAzimuth(double lat, double lon, double alt, double sx, double sy, double sz) {
    double el, az;
    elAz(lat, lon, alt, sx, sy, sz, el, az);
    return az * 180.0 / PI;
}

double calculateDistance(double x1, double y1, double z1, double x2, double y2, double z2) {
    double dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double calculateDoppler(double freq, double relativeSpeed) {
    return -freq * relativeSpeed / LIGHT_SPEED;
}

std::string getCurrentUTC() {
    auto now = std::chrono::system_clock::now();
    auto tv = std::chrono::system_clock::to_time_t(now);
    std::tm t{};
#if defined(_WIN32)
    gmtime_s(&t, &tv);
#else
    gmtime_r(&tv, &t);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return std::string(buf);
}

std::string formatTime(const TimeConfig& time) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%06.3f",
                  time.year, time.month, time.day, time.hour, time.minute, time.second);
    return std::string(buf);
}

} // namespace Utils

} // namespace gnss

//----------------------------------------------------------------------
// C Interface (global scope)
//----------------------------------------------------------------------

namespace {
    thread_local std::string g_lastError;
    void SetErr(const char* m) { g_lastError = m ? m : ""; }
}

extern "C" {

// ---------------------------------------------------------------- batch

struct GNSSGeneratorHandle {
    std::unique_ptr<gnss::GNSSGenerator> generator;
};

GNSSGeneratorHandle* gnss_create_generator(void) {
    auto* h = new GNSSGeneratorHandle();
    h->generator = std::make_unique<gnss::GNSSGenerator>();
    return h;
}

void gnss_destroy_generator(GNSSGeneratorHandle* h) { delete h; }

int gnss_init_from_json(GNSSGeneratorHandle* h, const char* jsonFile) {
    if (!h || !h->generator || !jsonFile) return 0;
    return h->generator->initializeFromJSON(jsonFile) ? 1 : 0;
}

int gnss_generate(GNSSGeneratorHandle* h, double durationSeconds) {
    if (!h || !h->generator) return 0;
    return h->generator->generate(durationSeconds) ? 1 : 0;
}

int gnss_save(GNSSGeneratorHandle* h, const char* filename) {
    if (!h || !h->generator) return 0;
    return h->generator->saveToFile(filename ? filename : "") ? 1 : 0;
}

int gnss_get_data_size(GNSSGeneratorHandle*) { return 0; }
const uint8_t* gnss_get_data(GNSSGeneratorHandle*) { return nullptr; }

void gnss_get_stats(GNSSGeneratorHandle* h, double* duration, double* sizeMB) {
    if (!h || !h->generator) return;
    const auto& s = h->generator->getStats();
    if (duration) *duration = s.duration;
    if (sizeMB)   *sizeMB   = s.dataSizeMB;
}

// ---------------------------------------------------------------- streaming

struct gnss_stream {
    IfGenEngine engine;
};

void gnss_stream_options_default(gnss_stream_options* o) {
    if (!o) return;
    std::memset(o, 0, sizeof(*o));
    o->multi_thread = 1;
    o->actualize_rinex = 1;
    o->noise_seed = 1;
    o->gain = 1.0f;
}

gnss_stream* gnss_stream_open(const gnss_stream_options* o) {
    if (!o || !o->config_file) { SetErr("config_file is required"); return nullptr; }
    try {
        auto s = std::make_unique<gnss_stream>();
        IfGenOptions opt;
        opt.configFile     = o->config_file;
        opt.writeBin       = o->write_bin != 0;
        opt.binFile        = o->bin_file ? o->bin_file : "";
        opt.writeTag       = o->write_tag != 0;
        opt.multiThread    = o->multi_thread != 0;
        opt.nThreads       = o->n_threads;
        opt.verbose        = o->verbose != 0;
        opt.actualizeRinex = o->actualize_rinex != 0;
        opt.noiseSeed      = o->noise_seed;
        opt.gain           = o->gain;
        s->engine.Init(opt);
        SetErr("");
        return s.release();
    } catch (const std::exception& e) {
        SetErr(e.what());
    } catch (...) {
        SetErr("unknown error");
    }
    return nullptr;
}

int gnss_stream_get_info(gnss_stream* s, gnss_stream_info* info) {
    if (!s || !info) return 0;
    try {
        const IfGenInfo& i = s->engine.Info();
        info->sample_rate_hz    = i.sampleRateHz;
        info->center_freq_hz    = i.centerFreqHz;
        info->samples_per_block = i.samplesPerBlock;
        info->total_blocks      = i.totalBlocks;
        info->n_channels        = i.nChannels;
        return 1;
    } catch (const std::exception& e) { SetErr(e.what()); }
    return 0;
}

size_t gnss_stream_read(gnss_stream* s, float* iq, size_t nSamples) {
    if (!s || !iq) return 0;
    try {
        // std::complex<float> layout-kompatibilis a float[2]-vel
        return s->engine.Read(reinterpret_cast<std::complex<float>*>(iq), nSamples);
    } catch (const std::exception& e) { SetErr(e.what()); }
    return 0;
}

void gnss_stream_close(gnss_stream* s) { delete s; }

const char* gnss_last_error(void) { return g_lastError.c_str(); }

} // extern "C"