//----------------------------------------------------------------------
// gnsslib.h:
//   Unified GNSS Signal Processing Library
//
//   Combines SignalSim and RinexActualizer functionality
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//          Modified and extended for library use
//----------------------------------------------------------------------

#ifndef GNSS_LIB_H
#define GNSS_LIB_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

//----------------------------------------------------------------------
// Forward declarations
//----------------------------------------------------------------------

struct UTC_TIME;
struct GNSS_TIME;
struct GPS_EPHEMERIS;
struct GLONASS_EPHEMERIS;
struct IONO_PARAM;
struct UTC_PARAM;

//----------------------------------------------------------------------
// Configuration Structures
//----------------------------------------------------------------------

namespace gnss {

// Signal types
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

// System types
enum class SystemType {
    GPS,
    BDS,
    Galileo,
    GLONASS
};

// Output formats
enum class OutputFormat {
    IQ8,
    IQ16,
    IQ4,
    IQ2
};

// Trajectory types
enum class TrajectoryType {
    ConstSpeed,
    ConstAcc,
    VerticalAcc,
    Jerk,
    HorizontalTurn
};

// Configuration for a single signal
struct SignalConfig {
    SystemType system;
    SignalType signal;
    bool enable = true;
};

// Configuration for trajectory segment
struct TrajectorySegment {
    TrajectoryType type = TrajectoryType::ConstSpeed;
    double duration = 0.0;      // seconds
    double speed = 0.0;         // m/s (for ConstSpeed)
    double acceleration = 0.0;  // m/s² (for ConstAcc)
    double course = 0.0;        // degrees (for HorizontalTurn)
    double radius = 0.0;        // meters (for HorizontalTurn)
};

// Output configuration
struct OutputConfig {
    OutputFormat format = OutputFormat::IQ16;
    double sampleRate = 4.0e6;      // Hz
    double centerFreq = 1575.42e6;  // Hz
    std::string filename = "output.bin";
    double elevationMask = 5.0;     // degrees
    bool generateTag = false;
};

// Power configuration
struct PowerConfig {
    double noiseFloor = -172.0;     // dBm
    double initPower = 47.0;        // dBHz
    bool elevationAdjust = true;
};

// Time configuration
struct TimeConfig {
    int year = 2021;
    int month = 6;
    int day = 19;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
    std::string timeType = "UTC";
};

// Position configuration
struct PositionConfig {
    double latitude = 47.492721;    // degrees
    double longitude = 19.015773;   // degrees
    double altitude = 20.0;         // meters
};

// Velocity configuration
struct VelocityConfig {
    double speed = 0.0;             // m/s
    double course = 0.0;            // degrees (0 = North, 90 = East)
};

// Complete GNSS configuration
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

//----------------------------------------------------------------------
// Statistics
//----------------------------------------------------------------------

struct GenerationStats {
    double duration = 0.0;          // seconds
    double dataSizeMB = 0.0;
    int totalSamples = 0;
    int clippedSamples = 0;
    int visibleSatellites = 0;
    double generationTime = 0.0;    // seconds
};

//----------------------------------------------------------------------
// Main Library Class
//----------------------------------------------------------------------

class GNSSGenerator {
public:
    GNSSGenerator();
    ~GNSSGenerator();

    // Initialize from configuration
    bool initialize(const GNSSConfig& config);

    // Initialize from JSON file
    bool initializeFromJSON(const std::string& jsonFile);

    // Generate IF data
    bool generate(double durationSeconds);
    bool generate(int durationMs);

    // Get generated data
    const std::vector<uint8_t>& getData() const { return m_data; }

    // Get statistics
    const GenerationStats& getStats() const { return m_stats; }

    // Get configuration
    const GNSSConfig& getConfig() const { return m_config; }

    // Save generated data to file
    bool saveToFile(const std::string& filename = "");

    // Clear all data
    void clear();

private:
    GNSSConfig m_config;
    GenerationStats m_stats;
    std::vector<uint8_t> m_data;
    bool m_initialized = false;
    std::string m_outputFile;

    // Internal methods
    bool parseJSON(const std::string& jsonFile);
    bool validateConfig();
    bool setupEphemeris();
    bool setupTrajectory();
    bool generateInternal();
    void resetState();
};

//----------------------------------------------------------------------
// Utility Functions
//----------------------------------------------------------------------

namespace Utils {
    // Time conversion
    bool utcToGpsTime(int year, int month, int day, int hour, int minute,
                      double second, int& week, int& msOfWeek);

    // Coordinate conversion
    void llaToEcef(double lat, double lon, double alt,
                   double& x, double& y, double& z);
    void ecefToLla(double x, double y, double z,
                   double& lat, double& lon, double& alt);

    // Distance and angles
    double calculateElevation(double receiverLat, double receiverLon, double receiverAlt,
                              double satX, double satY, double satZ);
    double calculateAzimuth(double receiverLat, double receiverLon, double receiverAlt,
                            double satX, double satY, double satZ);
    double calculateDistance(double x1, double y1, double z1,
                            double x2, double y2, double z2);

    // Doppler calculation
    double calculateDoppler(double freq, double relativeSpeed);

    // String helpers
    std::string getCurrentUTC();
    std::string formatTime(const TimeConfig& time);
}

//----------------------------------------------------------------------
// C Interface for external usage
//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef struct GNSSGeneratorHandle GNSSGeneratorHandle;

// Create/destroy
GNSSGeneratorHandle* gnss_create_generator(void);
void gnss_destroy_generator(GNSSGeneratorHandle* handle);

// Configuration
int gnss_init_from_json(GNSSGeneratorHandle* handle, const char* jsonFile);
int gnss_init_config(GNSSGeneratorHandle* handle, const char* configFile);

// Generation
int gnss_generate(GNSSGeneratorHandle* handle, double durationSeconds);
int gnss_save(GNSSGeneratorHandle* handle, const char* filename);

// Info
int gnss_get_data_size(GNSSGeneratorHandle* handle);
const uint8_t* gnss_get_data(GNSSGeneratorHandle* handle);
void gnss_get_stats(GNSSGeneratorHandle* handle, double* duration, double* sizeMB);

#ifdef __cplusplus
}
#endif

} // namespace gnss

#endif // GNSS_LIB_H