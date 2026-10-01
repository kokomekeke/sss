#ifndef GNSS_LIB_H
#define GNSS_LIB_H

#include <cstddef>
#include <cstdint>
#include <complex>
#include <memory>
#include <string>
#include <vector>

#include "gnss/IfGenEngine.h"

namespace gnss {

/** Supported GNSS signal types. */
enum class SignalType {
    GPS_L1CA, GPS_L1C, GPS_L2C, GPS_L2P, GPS_L5,
    BDS_B1C, BDS_B1I, BDS_B2I, BDS_B3I, BDS_B2a, BDS_B2b,
    GAL_E1, GAL_E5a, GAL_E5b, GAL_E5, GAL_E6,
    GLO_G1, GLO_G2, GLO_G3
};

/** GNSS constellation/system. */
enum class SystemType {
    GPS,
    BDS,
    Galileo,
    GLONASS
};

/** Output sample format. */
enum class OutputFormat {
    IQ8,
    IQ16,
    IQ4,
    IQ2
};

/** Trajectory segment type. */
enum class TrajectoryType {
    ConstSpeed,
    ConstAcc,
    VerticalAcc,
    Jerk,
    HorizontalTurn
};

/** Waypoint interpolation mode. */
enum class WaypointInterpolation {
    Linear,   // egyenes szakaszok, a sebesseg a toresponton ugrik
    Spline    // Catmull-Rom/Hermite, folytonos sebesseg
};

/** GNSS signal selection. */
struct SignalConfig {
    SystemType system = SystemType::GPS;
    SignalType signal = SignalType::GPS_L1CA;
    bool enable = true;
};

/**
 * One trajectory segment (segment mode).
 *
 * Used fields per type:
 *   ConstSpeed     : duration          (keeps the current speed; `speed` is not used by SignalSim)
 *   ConstAcc       : duration, acceleration (m/s^2, along the current direction)
 *   VerticalAcc    : duration, acceleration (m/s^2, vertical)
 *   Jerk           : duration, jerk (m/s^3)   [falls back to `acceleration` if jerk == 0]
 *   HorizontalTurn : duration, radius (m, signed) or turnRate (deg/s, signed)
 *                    [falls back to `acceleration` as turn rate if both are 0]
 */
struct TrajectorySegment {
    TrajectoryType type = TrajectoryType::ConstSpeed;

    double duration = 0.0;

    double speed = 0.0;
    double acceleration = 0.0;
    double jerk = 0.0;
    double turnRate = 0.0;     // deg/s
    double course = 0.0;       // informational
    double radius = 0.0;
};

/**
 * One waypoint (waypoint mode).
 *
 * Either ALL waypoints have `time` (strictly increasing, seconds), or NONE
 * of them does; then the segment time is distance / speed, where speed is the
 * `speed` of the segment's starting point, or GNSSConfig::waypointDefaultSpeed.
 */
struct WaypointConfig {
    double latitude  = 0.0;    // deg
    double longitude = 0.0;    // deg
    double altitude  = 0.0;    // m
    double time  = -1.0;       // s, <0: not given
    double speed = -1.0;       // m/s, <0: not given
};

/** Output configuration. */
struct OutputConfig {
    OutputFormat format = OutputFormat::IQ16;

    double sampleRate = 4.0e6;
    double centerFreq = 1575.42e6;

    std::string filename = "output.bin";

    double elevationMask = 5.0;

    bool generateTag = false;

    /** Optional output file override. If empty, filename is used. */
    std::string binFile;
};

/** Signal power/noise configuration. */
struct PowerConfig {
    double noiseFloor = -172.0;

    /** Initial C/N0 in dB-Hz. */
    double initPower = 47.0;

    bool elevationAdjust = true;
};

/** Simulation start time. */
struct TimeConfig {
    int year = 2021;
    int month = 6;
    int day = 19;

    int hour = 0;
    int minute = 0;

    double second = 0.0;

    /** Kept for compatibility/documentation. The engine uses UTC fields. */
    std::string timeType = "UTC";
};

/** Initial receiver position (segment mode only). */
struct PositionConfig {
    double latitude = 47.492721;
    double longitude = 19.015773;
    double altitude = 20.0;
};

/** Initial receiver velocity (segment mode only). */
struct VelocityConfig {
    double speed = 0.0;
    double course = 0.0;
};

/**
 * Complete GNSS simulation configuration.
 *
 * Two trajectory modes (mutually exclusive):
 *
 *   1. Segment mode : position + velocity + trajectory[]
 *   2. Waypoint mode: waypoints[]  (position, velocity, trajectory[] are ignored,
 *                                   trajectory[] must be empty)
 */
struct GNSSConfig {
    TimeConfig time;

    PositionConfig position;

    VelocityConfig velocity;

    /** Segment mode. */
    std::vector<TrajectorySegment> trajectory;

    /** Waypoint mode: if not empty, waypoint mode is used. */
    std::vector<WaypointConfig> waypoints;
    WaypointInterpolation waypointInterpolation = WaypointInterpolation::Spline;
    double waypointDefaultSpeed = 0.0;   // m/s, used if a waypoint has neither time nor speed

    /** RINEX navigation file. */
    std::string ephemerisFile;

    /** If true, RINEX ephemerides are actualized to the simulation start time. */
    bool actualizeRinex = false;

    std::vector<SignalConfig> signals;

    OutputConfig output;

    PowerConfig power;

    /** Engine/runtime options. */
    bool writeBin = true;
    bool writeTag = false;

    bool multiThread = true;
    int nThreads = 0;

    bool verbose = false;

    bool validateOnly = false;

    std::uint64_t noiseSeed = 0;

    float gain = 1.0f;
};

/** Generation statistics. */
struct GenerationStats {
    double duration = 0.0;          // generated signal duration, s

    double dataSizeMB = 0.0;

    std::uint64_t totalSamples = 0;

    int clippedSamples = 0;

    int visibleSatellites = 0;      // number of channels

    double generationTime = 0.0;    // wall time, s
};

/**
 * High-level GNSS generator API.
 *
 * Initialization either from GNSSConfig (preferred) or from a JSON file.
 */
class GNSSGenerator {
public:
    GNSSGenerator();

    ~GNSSGenerator();

    GNSSGenerator(const GNSSGenerator&) = delete;
    GNSSGenerator& operator=(const GNSSGenerator&) = delete;

    GNSSGenerator(GNSSGenerator&&) noexcept;
    GNSSGenerator& operator=(GNSSGenerator&&) noexcept;

    /** Initialize directly from C++ parameters (no JSON is read). */
    bool initialize(const GNSSConfig& config);

    /** Initialize from a JSON configuration file. */
    bool initializeFromJSON(const std::string& jsonFile);

    /** Set/override the output file (applied on the next generate/openStream). */
    void setOutputFile(const std::string& filename);

    /**
     * Generate a simulation into the output file.
     * Generation stops after durationSeconds or at the end of the trajectory,
     * whichever comes first.
     */
    bool generate(double durationSeconds);

    /** Same as above, duration in milliseconds. */
    bool generate(int durationMs);

    /** Open the generator for streaming. */
    bool openStream(
        bool writeBin = false,
        const std::string& binFile = "",
        float gain = 1.0f
    );

    /** Opens the configured simulation into an externally supplied IfGenEngine. */
    bool openStream(
        IfGenEngine& engine,
        bool writeBin = false,
        const std::string& binFile = "",
        float gain = 1.0f
    );

    /** Read generated complex samples; returns the number of samples read. */
    std::size_t read(
        std::complex<float>* out,
        std::size_t n
    );

    /** Stream samples through a callback (return false to stop). */
    std::size_t stream(
        const IfGenEngine::SampleCallback& callback,
        std::size_t chunk = 0
    );

    /** Close the active stream. */
    void closeStream();

    /** Legacy data accessor (normally empty). */
    const std::vector<std::uint8_t>& getData() const
    {
        return m_data;
    }

    const GenerationStats& getStats() const
    {
        return m_stats;
    }

    const GNSSConfig& getConfig() const
    {
        return m_config;
    }

    /**
     * Copy the produced output file to `filename`.
     * With an empty filename nothing is copied; returns true if a produced file exists.
     */
    bool saveToFile(const std::string& filename = "");

    /** Reset the generator. */
    void clear();

    bool isInitialized() const
    {
        return m_initialized;
    }

private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;

    GNSSConfig m_config;

    GenerationStats m_stats;

    std::vector<std::uint8_t> m_data;

    bool m_initialized = false;
};


/** Utility functions. */
namespace Utils {

bool utcToGpsTime(
    int year, int month, int day, int hour, int minute, double second,
    double& gpsWeek, double& gpsSeconds
);

void llaToEcef(
    double latitudeDeg, double longitudeDeg, double altitudeM,
    double& x, double& y, double& z
);

void ecefToLla(
    double x, double y, double z,
    double& latitudeDeg, double& longitudeDeg, double& altitudeM
);

double calculateElevation(
    double receiverX, double receiverY, double receiverZ,
    double satelliteX, double satelliteY, double satelliteZ
);

double calculateAzimuth(
    double receiverX, double receiverY, double receiverZ,
    double satelliteX, double satelliteY, double satelliteZ
);

double calculateDistance(
    double receiverX, double receiverY, double receiverZ,
    double satelliteX, double satelliteY, double satelliteZ
);

double calculateDoppler(
    double carrierFrequency,
    double rangeRate
);

std::string getCurrentUTC();

std::string formatTime(
    const TimeConfig& time
);

} // namespace Utils

} // namespace gnss


/*
 * C API
 *
 * The C API remains available for applications that cannot use
 * the C++ interface. It is JSON based, so waypoint mode is available
 * through the "waypointList" JSON key.
 */
extern "C" {

typedef struct GNSSGeneratorHandle GNSSGeneratorHandle;

GNSSGeneratorHandle* gnss_create_generator();

void gnss_destroy_generator(
    GNSSGeneratorHandle* generator
);

int gnss_init_from_json(
    GNSSGeneratorHandle* generator,
    const char* json_file
);

int gnss_generate(
    GNSSGeneratorHandle* generator,
    double duration_seconds
);

int gnss_save(
    GNSSGeneratorHandle* generator,
    const char* filename
);

int gnss_get_stats(
    GNSSGeneratorHandle* generator,
    double* duration,
    double* size_mb
);


/* Streaming API */

typedef struct gnss_stream gnss_stream;

typedef struct {
    const char* config_file;

    int write_bin;

    const char* bin_file;

    int write_tag;

    int multi_thread;

    int n_threads;

    int verbose;

    int actualize_rinex;

    unsigned long long noise_seed;

    float gain;
} gnss_stream_options;

typedef struct {
    double sample_rate_hz;

    double center_freq_hz;

    size_t samples_per_block;

    size_t total_blocks;

    int n_channels;
} gnss_stream_info;

void gnss_stream_options_default(
    gnss_stream_options* options
);

gnss_stream* gnss_stream_open(
    const gnss_stream_options* options
);

int gnss_stream_get_info(
    gnss_stream* stream,
    gnss_stream_info* info
);

size_t gnss_stream_read(
    gnss_stream* stream,
    void* out,
    size_t samples
);

void gnss_stream_close(
    gnss_stream* stream
);

const char* gnss_last_error(void);

} // extern "C"

#endif // GNSS_LIB_H