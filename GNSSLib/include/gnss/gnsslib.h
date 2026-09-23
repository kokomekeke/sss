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

/**
 * Supported GNSS signal types.
 */
enum class SignalType {
    GPS_L1CA,
    GPS_L1C,
    GPS_L2C,
    GPS_L2P,
    GPS_L5,

    BDS_B1C,
    BDS_B1I,
    BDS_B2I,
    BDS_B3I,
    BDS_B2a,
    BDS_B2b,

    GAL_E1,
    GAL_E5a,
    GAL_E5b,
    GAL_E5,
    GAL_E6,

    GLO_G1,
    GLO_G2,
    GLO_G3
};

/**
 * GNSS constellation/system.
 */
enum class SystemType {
    GPS,
    BDS,
    Galileo,
    GLONASS
};

/**
 * Output sample format.
 */
enum class OutputFormat {
    IQ8,
    IQ16,
    IQ4,
    IQ2
};

/**
 * Trajectory segment type.
 */
enum class TrajectoryType {
    ConstSpeed,
    ConstAcc,
    VerticalAcc,
    Jerk,
    HorizontalTurn
};

/**
 * GNSS signal selection.
 */
struct SignalConfig {
    SystemType system = SystemType::GPS;
    SignalType signal = SignalType::GPS_L1CA;
    bool enable = true;
};

/**
 * One trajectory segment.
 */
struct TrajectorySegment {
    TrajectoryType type = TrajectoryType::ConstSpeed;

    double duration = 0.0;

    double speed = 0.0;
    double acceleration = 0.0;
    double course = 0.0;
    double radius = 0.0;
};

/**
 * Output configuration.
 */
struct OutputConfig {
    OutputFormat format = OutputFormat::IQ16;

    double sampleRate = 4.0e6;
    double centerFreq = 1575.42e6;

    std::string filename = "output.bin";

    double elevationMask = 5.0;

    bool generateTag = false;

    /**
     * Optional output file override.
     *
     * If empty, filename is used.
     */
    std::string binFile;
};

/**
 * Signal power/noise configuration.
 */
struct PowerConfig {
    double noiseFloor = -172.0;

    /**
     * Initial C/N0 in dB-Hz.
     */
    double initPower = 47.0;

    bool elevationAdjust = true;
};

/**
 * Simulation start time.
 */
struct TimeConfig {
    int year = 2021;
    int month = 6;
    int day = 19;

    int hour = 0;
    int minute = 0;

    double second = 0.0;

    /**
     * Kept for compatibility/documentation.
     *
     * The current IfGenParams interface uses UTC fields.
     */
    std::string timeType = "UTC";
};

/**
 * Initial receiver position.
 *
 * Latitude/longitude are degrees.
 * Altitude is meters.
 */
struct PositionConfig {
    double latitude = 47.492721;
    double longitude = 19.015773;
    double altitude = 20.0;
};

/**
 * Initial receiver velocity.
 *
 * Speed is m/s.
 * Course is degrees.
 */
struct VelocityConfig {
    double speed = 0.0;
    double course = 0.0;
};

/**
 * Complete GNSS simulation configuration.
 *
 * This is the primary programmatic configuration API.
 *
 * No JSON file is required when using initialize(const GNSSConfig&).
 */
struct GNSSConfig {
    TimeConfig time;

    PositionConfig position;

    VelocityConfig velocity;

    std::vector<TrajectorySegment> trajectory;

    /**
     * RINEX navigation file.
     */
    std::string ephemerisFile;

    /**
     * If true, RINEX ephemerides are actualized to the
     * requested simulation start time.
     */
    bool actualizeRinex = false;

    std::vector<SignalConfig> signals;

    OutputConfig output;

    PowerConfig power;

    /**
     * Engine/runtime options.
     */
    bool writeBin = true;
    bool writeTag = false;

    bool multiThread = true;
    int nThreads = 0;

    bool verbose = false;

    bool validateOnly = false;

    std::uint64_t noiseSeed = 0;

    float gain = 1.0f;
};

/**
 * Generation statistics.
 */
struct GenerationStats {
    double duration = 0.0;

    double dataSizeMB = 0.0;

    int totalSamples = 0;

    int clippedSamples = 0;

    int visibleSatellites = 0;

    double generationTime = 0.0;
};

/**
 * High-level GNSS generator API.
 *
 * This is the recommended API for applications embedding GNSSLib.
 *
 * The generator can be initialized either:
 *
 *   1. directly from GNSSConfig
 *   2. from a JSON configuration file
 *
 * For library integration, GNSSConfig is the preferred interface.
 */
class GNSSGenerator {
public:
    GNSSGenerator();

    ~GNSSGenerator();

    GNSSGenerator(const GNSSGenerator&) = delete;
    GNSSGenerator& operator=(const GNSSGenerator&) = delete;

    GNSSGenerator(GNSSGenerator&&) noexcept;
    GNSSGenerator& operator=(GNSSGenerator&&) noexcept;

    /**
     * Initialize directly from C++ parameters.
     *
     * No JSON file is read.
     *
     * Internally:
     *
     *   GNSSConfig -> IfGenParams -> IfGenEngine
     */
    bool initialize(const GNSSConfig& config);

    /**
     * Initialize from a JSON configuration file.
     *
     * This is retained for standalone/config-file based use.
     */
    bool initializeFromJSON(const std::string& jsonFile);

    /**
     * Set/override the output file.
     *
     * If the generator is already initialized, the new output
     * file is applied on the next generation/openStream call.
     */
    void setOutputFile(const std::string& filename);

    /**
     * Generate a complete simulation.
     *
     * durationSeconds specifies the generated signal duration.
     */
    bool generate(double durationSeconds);

    /**
     * Generate a complete simulation.
     *
     * durationMs specifies the generated signal duration.
     */
    bool generate(int durationMs);

    /**
     * Open the generator for streaming.
     *
     * If writeBin is true, the engine may also write the generated
     * samples to a binary output file.
     */
    bool openStream(
        bool writeBin = false,
        const std::string& binFile = "",
        float gain = 1.0f
    );

    /**
     * Compatibility overload.
     *
     * Opens the configured simulation into an externally supplied
     * IfGenEngine.
     */
    bool openStream(
        IfGenEngine& engine,
        bool writeBin = false,
        const std::string& binFile = "",
        float gain = 1.0f
    );

    /**
     * Read generated complex samples.
     *
     * Returns the number of samples actually read.
     *
     * The stream must have been opened with openStream().
     */
    std::size_t read(
        std::complex<float>* out,
        std::size_t n
    );

    /**
     * Stream generated samples through a callback.
     *
     * Returning false from the callback stops the stream.
     *
     * Returns the number of samples processed.
     */
    std::size_t stream(
        const IfGenEngine::SampleCallback& callback,
        std::size_t chunk = 0
    );

    /**
     * Close the active stream.
     */
    void closeStream();

    /**
     * Legacy data accessor.
     *
     * The current implementation is stream-oriented and therefore
     * this vector is normally empty.
     */
    const std::vector<std::uint8_t>& getData() const
    {
        return m_data;
    }

    /**
     * Get generation statistics.
     */
    const GenerationStats& getStats() const
    {
        return m_stats;
    }

    /**
     * Get the active high-level configuration.
     */
    const GNSSConfig& getConfig() const
    {
        return m_config;
    }

    /**
     * Save generated output.
     *
     * If generation was performed with writeBin=true, this copies/
     * finalizes the produced output to filename when supported by
     * the current backend.
     */
    bool saveToFile(const std::string& filename = "");

    /**
     * Reset the generator.
     */
    void clear();

    /**
     * Check whether the generator has been initialized.
     */
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


/**
 * Utility functions.
 */
namespace Utils {

bool utcToGpsTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    double second,
    double& gpsWeek,
    double& gpsSeconds
);

void llaToEcef(
    double latitudeDeg,
    double longitudeDeg,
    double altitudeM,
    double& x,
    double& y,
    double& z
);

void ecefToLla(
    double x,
    double y,
    double z,
    double& latitudeDeg,
    double& longitudeDeg,
    double& altitudeM
);

double calculateElevation(
    double receiverX,
    double receiverY,
    double receiverZ,
    double satelliteX,
    double satelliteY,
    double satelliteZ
);

double calculateAzimuth(
    double receiverX,
    double receiverY,
    double receiverZ,
    double satelliteX,
    double satelliteY,
    double satelliteZ
);

double calculateDistance(
    double receiverX,
    double receiverY,
    double receiverZ,
    double satelliteX,
    double satelliteY,
    double satelliteZ
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
 * the C++ interface.
 */
extern "C" {

typedef struct GNSSGeneratorHandle GNSSGeneratorHandle;

/**
 * Create/destroy generator.
 */
GNSSGeneratorHandle* gnss_create_generator();

void gnss_destroy_generator(
    GNSSGeneratorHandle* generator
);

/**
 * JSON initialization.
 */
int gnss_init_from_json(
    GNSSGeneratorHandle* generator,
    const char* json_file
);

/**
 * Generate complete simulation.
 */
int gnss_generate(
    GNSSGeneratorHandle* generator,
    double duration_seconds
);

/**
 * Save generated output.
 */
int gnss_save(
    GNSSGeneratorHandle* generator,
    const char* filename
);

/**
 * Retrieve basic statistics.
 */
int gnss_get_stats(
    GNSSGeneratorHandle* generator,
    double* duration,
    double* size_mb
);


/*
 * Streaming API
 */

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