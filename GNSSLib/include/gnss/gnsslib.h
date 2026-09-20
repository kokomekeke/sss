// Sisi/GNSSLib/include/gnss/gnsslib.h
//----------------------------------------------------------------------
// gnsslib.h:
//   Unified GNSS Signal Processing Library
//
//   Combines SignalSim and RinexActualizer functionality.
//
//   Two ways to use it:
//     1. Streaming (recommended): gnss::IfGenEngine (gnss/IfGenEngine.h)
//        or the C API gnss_stream_* below -> 1 ms IQ blocks from a JSON config.
//     2. Batch: gnss::GNSSGenerator::initializeFromJSON() + generate()
//        -> writes the .bin file configured in the JSON.
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//          Modified and extended for library use
//----------------------------------------------------------------------

#ifndef GNSS_LIB_H
#define GNSS_LIB_H

#include <stddef.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "gnss/IfGenEngine.h"

//----------------------------------------------------------------------
// Configuration Structures
//----------------------------------------------------------------------

namespace gnss {

// Signal types
enum class SignalType {
    GPS_L1CA, GPS_L1C, GPS_L2C, GPS_L2P, GPS_L5,
    BDS_B1C, BDS_B1I, BDS_B2I, BDS_B3I, BDS_B2a, BDS_B2b,
    GAL_E1, GAL_E5a, GAL_E5b, GAL_E5, GAL_E6,
    GLO_G1, GLO_G2, GLO_G3
};

// System types
enum class SystemType { GPS, BDS, Galileo, GLONASS };

// Output formats
enum class OutputFormat { IQ8, IQ16, IQ4, IQ2 };

// Trajectory types
enum class TrajectoryType { ConstSpeed, ConstAcc, VerticalAcc, Jerk, HorizontalTurn };

struct SignalConfig {
    SystemType system;
    SignalType signal;
    bool enable = true;
};

struct TrajectorySegment {
    TrajectoryType type = TrajectoryType::ConstSpeed;
    double duration = 0.0;      // seconds
    double speed = 0.0;         // m/s (for ConstSpeed)
    double acceleration = 0.0;  // m/s^2 (for ConstAcc)
    double course = 0.0;        // degrees (for HorizontalTurn)
    double radius = 0.0;        // meters (for HorizontalTurn)
};

struct OutputConfig {
    OutputFormat format = OutputFormat::IQ16;
    double sampleRate = 4.0e6;      // Hz
    double centerFreq = 1575.42e6;  // Hz
    std::string filename = "output.bin";
    double elevationMask = 5.0;     // degrees
    bool generateTag = false;
};

struct PowerConfig {
    double noiseFloor = -172.0;     // dBm
    double initPower = 47.0;        // dBHz
    bool elevationAdjust = true;
};

struct TimeConfig {
    int year = 2021;
    int month = 6;
    int day = 19;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
    std::string timeType = "UTC";
};

struct PositionConfig {
    double latitude = 47.492721;    // degrees
    double longitude = 19.015773;   // degrees
    double altitude = 20.0;         // meters
};

struct VelocityConfig {
    double speed = 0.0;             // m/s
    double course = 0.0;            // degrees (0 = North, 90 = East)
};

struct GNSSConfig {
    TimeConfig time;
    PositionConfig position;
    VelocityConfig velocity;
    std::vector<TrajectorySegment> trajectory;
    std::string ephemerisFile;
    std::vector<SignalConfig> signals;
    OutputConfig output;
    PowerConfig power;
};

struct GenerationStats {
    double duration = 0.0;          // seconds
    double dataSizeMB = 0.0;
    int totalSamples = 0;
    int clippedSamples = 0;
    int visibleSatellites = 0;
    double generationTime = 0.0;    // seconds
};

//----------------------------------------------------------------------
// Batch generator (JSON driven)
//----------------------------------------------------------------------

class GNSSGenerator {
public:
    GNSSGenerator();
    ~GNSSGenerator();

    // NOT SUPPORTED yet: the programmatic GNSSConfig path was never functional.
    // Returns false; use initializeFromJSON().
    bool initialize(const GNSSConfig& config);

    // Reads + validates the JSON (cheap probe, no signal generation) and fills getConfig().output
    bool initializeFromJSON(const std::string& jsonFile);

    // Override the .bin file name from the JSON (call after initializeFromJSON)
    void setOutputFile(const std::string& filename);

    // Runs the simulation and writes the .bin file. durationMs <= 0 -> whole trajectory.
    bool generate(double durationSeconds);
    bool generate(int durationMs);

    // Streaming: initialises `engine` from the JSON given to initializeFromJSON().
    // writeBin = true keeps the clean .bin file writing alongside the stream.
    bool openStream(IfGenEngine& engine, bool writeBin = false,
                    const std::string& binFile = "", float gain = 1.0f);

    // Legacy: in-memory buffer is not kept any more (always empty). Use the streaming API.
    const std::vector<uint8_t>& getData() const { return m_data; }
    const GenerationStats& getStats() const { return m_stats; }
    const GNSSConfig& getConfig() const { return m_config; }

    // Copies the generated .bin to `filename` (empty -> just checks that it was generated)
    bool saveToFile(const std::string& filename = "");

    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    GNSSConfig m_config;
    GenerationStats m_stats;
    std::vector<uint8_t> m_data;
    bool m_initialized = false;
};

//----------------------------------------------------------------------
// Utility Functions
//----------------------------------------------------------------------

namespace Utils {
    bool utcToGpsTime(int year, int month, int day, int hour, int minute,
                      double second, int& week, int& msOfWeek);

    void llaToEcef(double lat, double lon, double alt,
                   double& x, double& y, double& z);
    void ecefToLla(double x, double y, double z,
                   double& lat, double& lon, double& alt);

    double calculateElevation(double receiverLat, double receiverLon, double receiverAlt,
                              double satX, double satY, double satZ);
    double calculateAzimuth(double receiverLat, double receiverLon, double receiverAlt,
                            double satX, double satY, double satZ);
    double calculateDistance(double x1, double y1, double z1,
                             double x2, double y2, double z2);
    double calculateDoppler(double freq, double relativeSpeed);

    std::string getCurrentUTC();
    std::string formatTime(const TimeConfig& time);
}

} // namespace gnss

//----------------------------------------------------------------------
// C Interface (global scope, C linkage)
//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// ---- Batch generator ----
typedef struct GNSSGeneratorHandle GNSSGeneratorHandle;

GNSSGeneratorHandle* gnss_create_generator(void);
void gnss_destroy_generator(GNSSGeneratorHandle* handle);
int  gnss_init_from_json(GNSSGeneratorHandle* handle, const char* jsonFile);
int  gnss_generate(GNSSGeneratorHandle* handle, double durationSeconds);
int  gnss_save(GNSSGeneratorHandle* handle, const char* filename);
int  gnss_get_data_size(GNSSGeneratorHandle* handle);          // legacy: always 0
const uint8_t* gnss_get_data(GNSSGeneratorHandle* handle);     // legacy: always NULL
void gnss_get_stats(GNSSGeneratorHandle* handle, double* duration, double* sizeMB);

// ---- Streaming (1 ms blocks of complex float IQ, interleaved I,Q) ----
typedef struct gnss_stream gnss_stream;

typedef struct {
    const char* config_file;          // JSON
    int   write_bin;                  // 1 -> clean .bin is written as well
    const char* bin_file;             // NULL/"" -> from JSON
    int   write_tag;
    int   multi_thread;               // default 1
    int   n_threads;                  // 0 -> auto
    int   verbose;
    int   actualize_rinex;            // default 1
    unsigned long long noise_seed;    // default 1
    float gain;                       // default 1.0 (scales the stream, not the .bin)
} gnss_stream_options;

typedef struct {
    double sample_rate_hz;
    double center_freq_hz;
    size_t samples_per_block;         // per 1 ms
    size_t total_blocks;              // trajectory length in ms
    int    n_channels;
} gnss_stream_info;

void         gnss_stream_options_default(gnss_stream_options* opt);
gnss_stream* gnss_stream_open(const gnss_stream_options* opt);   // NULL on error -> gnss_last_error()
int          gnss_stream_get_info(gnss_stream* s, gnss_stream_info* info);
// Writes up to nSamples complex samples (2*nSamples floats) to iq. Returns samples written; 0 = end/error.
size_t       gnss_stream_read(gnss_stream* s, float* iq, size_t nSamples);
void         gnss_stream_close(gnss_stream* s);
const char*  gnss_last_error(void);                              // thread-local

#ifdef __cplusplus
}
#endif

#endif // GNSS_LIB_H