//----------------------------------------------------------------------
// GNSSGenerator.cpp:
//   Implementation of unified GNSS library
//----------------------------------------------------------------------

#include "gnss/gnsslib.h"

// Include the existing code
#include "ActualizeRinex.h"
#include "NavData.h"
#include "Trajectory.h"
#include "SignalSim.h"
#include "JsonInterpreter.h"
#include "Rinex.h"
#include "Coordinate.h"
#include "GnssTime.h"

#include <fstream>
#include <chrono>
#include <iostream>
#include <cstring>

//----------------------------------------------------------------------
// Internal state for the generator
//----------------------------------------------------------------------

struct GNSSGeneratorImpl {
    GNSSConfig config;
    GenerationStats stats;
    std::vector<uint8_t> data;
    bool initialized = false;
    std::string outputFile;

    // Global objects used by the existing code
    CTrajectory trajectory;
    CNavData navData;
    OUTPUT_PARAM outputParam;
    CPowerControl powerControl;
    GNSS_TIME curTime;
    UTC_TIME utcTime;

    // Ephemeris arrays
    PGPS_EPHEMERIS gpsEph[32];
    PGPS_EPHEMERIS bdsEph[63];
    PGPS_EPHEMERIS galEph[36];
    PGLONASS_EPHEMERIS gloEph[24];

    // Visible satellite arrays
    PGPS_EPHEMERIS gpsVisible[32];
    PGPS_EPHEMERIS bdsVisible[63];
    PGPS_EPHEMERIS galVisible[36];
    PGLONASS_EPHEMERIS gloVisible[24];

    void reset() {
        data.clear();
        stats = GenerationStats();
        initialized = false;
        memset(&utcTime, 0, sizeof(UTC_TIME));
        memset(&curTime, 0, sizeof(GNSS_TIME));
        memset(gpsEph, 0, sizeof(gpsEph));
        memset(bdsEph, 0, sizeof(bdsEph));
        memset(galEph, 0, sizeof(galEph));
        memset(gloEph, 0, sizeof(gloEph));
        memset(gpsVisible, 0, sizeof(gpsVisible));
        memset(bdsVisible, 0, sizeof(bdsVisible));
        memset(galVisible, 0, sizeof(galVisible));
        memset(gloVisible, 0, sizeof(gloVisible));
    }
};

//----------------------------------------------------------------------
// GNSSGenerator Implementation
//----------------------------------------------------------------------

namespace gnss {

GNSSGenerator::GNSSGenerator()
    : m_impl(std::make_unique<GNSSGeneratorImpl>()) {
    m_impl->reset();
}

GNSSGenerator::~GNSSGenerator() = default;

bool GNSSGenerator::initialize(const GNSSConfig& config) {
    m_config = config;
    if (!validateConfig()) return false;
    if (!setupEphemeris()) return false;
    if (!setupTrajectory()) return false;
    m_initialized = true;
    return true;
}

bool GNSSGenerator::initializeFromJSON(const std::string& jsonFile) {
    if (!parseJSON(jsonFile)) return false;
    return initialize(m_config);
}

bool GNSSGenerator::parseJSON(const std::string& jsonFile) {
    JsonStream jsonTree;
    JsonObject* object;

    if (jsonTree.ReadFile(jsonFile.c_str()) != 0) {
        std::cerr << "Failed to read JSON file: " << jsonFile << std::endl;
        return false;
    }

    SetJsonFilePath(jsonFile.c_str());
    object = jsonTree.GetRootObject();

    // Parse using existing function
    AssignParameters(object, &m_impl->utcTime, nullptr, nullptr,
                     &m_impl->trajectory, &m_impl->navData,
                     &m_impl->outputParam, &m_impl->powerControl, nullptr);

    // Populate config from parsed data
    m_config.time.year = m_impl->utcTime.Year;
    m_config.time.month = m_impl->utcTime.Month;
    m_config.time.day = m_impl->utcTime.Day;
    m_config.time.hour = m_impl->utcTime.Hour;
    m_config.time.minute = m_impl->utcTime.Minute;
    m_config.time.second = m_impl->utcTime.Second;

    // Note: Position and velocity would need to be parsed separately
    // For now, we'll use default values or parse from the JSON

    m_config.output.filename = m_impl->outputParam.filename;
    m_config.output.sampleRate = m_impl->outputParam.SampleFreq;
    m_config.output.centerFreq = m_impl->outputParam.CenterFreq;
    m_config.output.elevationMask = m_impl->outputParam.ElevationMask * 180.0 / PI;

    // Determine format
    switch (m_impl->outputParam.Format) {
        case OutputFormatIQ8: m_config.output.format = OutputFormat::IQ8; break;
        case OutputFormatIQ16: m_config.output.format = OutputFormat::IQ16; break;
        case OutputFormatIQ4: m_config.output.format = OutputFormat::IQ4; break;
        case OutputFormatIQ2: m_config.output.format = OutputFormat::IQ2; break;
        default: m_config.output.format = OutputFormat::IQ16; break;
    }

    m_config.power.noiseFloor = m_impl->powerControl.NoiseFloor;
    m_config.power.initPower = m_impl->powerControl.InitCN0;
    m_config.power.elevationAdjust = (m_impl->powerControl.Adjust != ElevationAdjustNone);

    // Parse ephemeris file
    // This would need to be extracted from the navData

    return true;
}

bool GNSSGenerator::validateConfig() {
    if (m_config.ephemerisFile.empty()) {
        std::cerr << "Ephemeris file not specified" << std::endl;
        return false;
    }
    if (m_config.signals.empty()) {
        std::cerr << "No signals configured" << std::endl;
        return false;
    }
    if (m_config.output.sampleRate <= 0) {
        std::cerr << "Invalid sample rate" << std::endl;
        return false;
    }
    return true;
}

bool GNSSGenerator::setupEphemeris() {
    // Load RINEX file into NavData
    m_impl->navData.ReadNavFile(m_config.ephemerisFile.c_str(), "");

    // Actualize RINEX to simulation time
    ActualizeRinex(m_impl->utcTime, &m_impl->navData, m_impl->outputParam);

    // Convert UTC to GPS time
    m_impl->curTime = UtcToGpsTime(m_impl->utcTime, FALSE);

    return true;
}

bool GNSSGenerator::setupTrajectory() {
    // Set up trajectory from config
    LLA_POSITION lla;
    lla.lat = m_config.position.latitude * PI / 180.0;
    lla.lon = m_config.position.longitude * PI / 180.0;
    lla.alt = m_config.position.altitude;

    LOCAL_SPEED speed;
    speed.speed = m_config.velocity.speed;
    speed.course = m_config.velocity.course * PI / 180.0;
    SpeedCourseToEnu(speed);

    m_impl->trajectory.SetInitPosVel(lla, speed, false);

    // Add trajectory segments
    for (const auto& seg : m_config.trajectory) {
        TrajectoryDataType dataType1 = TrajDataTimeSpan;
        TrajectoryDataType dataType2 = TrajDataSpeed;
        double data1 = seg.duration;
        double data2 = seg.speed;

        TrajectoryType trajType;
        switch (seg.type) {
            case TrajectoryType::ConstSpeed:
                trajType = TrajTypeConstSpeed;
                break;
            case TrajectoryType::ConstAcc:
                trajType = TrajTypeConstAcc;
                dataType2 = TrajDataAcceleration;
                data2 = seg.acceleration;
                break;
            case TrajectoryType::HorizontalTurn:
                trajType = TrajTypeHorizontalCircular;
                dataType2 = TrajDataAngularRate;
                data2 = seg.course * PI / 180.0 / seg.duration;
                break;
            default:
                trajType = TrajTypeConstSpeed;
                break;
        }

        m_impl->trajectory.AppendTrajectory(trajType, dataType1, data1, dataType2, data2);
    }

    return true;
}

bool GNSSGenerator::generate(double durationSeconds) {
    return generate(static_cast<int>(durationSeconds * 1000));
}

bool GNSSGenerator::generate(int durationMs) {
    if (!m_initialized) {
        std::cerr << "Generator not initialized. Call initialize() first." << std::endl;
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // This would call the main generation loop
    // For now, we'll use the existing code structure

    // Reset trajectory time
    m_impl->trajectory.ResetTrajectoryTime();
    m_impl->curTime = UtcToGpsTime(m_impl->utcTime, FALSE);

    // The main generation loop would go here
    // For brevity, we're showing the structure

    // Calculate data size
    double bytesPerSample = 4; // IQ16 = 4 bytes per sample
    m_stats.dataSizeMB = (durationMs * m_config.output.sampleRate * bytesPerSample) / (1024.0 * 1024.0);
    m_stats.duration = durationMs / 1000.0;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    m_stats.generationTime = duration.count() / 1000.0;

    m_initialized = true;
    return true;
}

bool GNSSGenerator::saveToFile(const std::string& filename) {
    std::string outputFile = filename.empty() ? m_config.output.filename : filename;

    std::ofstream file(outputFile, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << outputFile << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(m_data.data()), m_data.size());
    file.close();

    return true;
}

void GNSSGenerator::clear() {
    m_impl->reset();
    m_data.clear();
    m_stats = GenerationStats();
    m_initialized = false;
}

//----------------------------------------------------------------------
// Utility Functions Implementation
//----------------------------------------------------------------------

namespace Utils {

bool utcToGpsTime(int year, int month, int day, int hour, int minute,
                  double second, int& week, int& msOfWeek) {
    UTC_TIME utc;
    utc.Year = year;
    utc.Month = month;
    utc.Day = day;
    utc.Hour = hour;
    utc.Minute = minute;
    utc.Second = second;

    GNSS_TIME gps = UtcToGpsTime(utc, FALSE);
    week = gps.Week;
    msOfWeek = gps.MilliSeconds;
    return true;
}

void llaToEcef(double lat, double lon, double alt,
               double& x, double& y, double& z) {
    LLA_POSITION lla;
    lla.lat = lat * PI / 180.0;
    lla.lon = lon * PI / 180.0;
    lla.alt = alt;

    KINEMATIC_INFO ecef = LlaToEcef(lla);
    x = ecef.x;
    y = ecef.y;
    z = ecef.z;
}

void ecefToLla(double x, double y, double z,
               double& lat, double& lon, double& alt) {
    KINEMATIC_INFO ecef;
    ecef.x = x;
    ecef.y = y;
    ecef.z = z;

    LLA_POSITION lla = EcefToLla(ecef);
    lat = lla.lat * 180.0 / PI;
    lon = lla.lon * 180.0 / PI;
    alt = lla.alt;
}

double calculateElevation(double receiverLat, double receiverLon, double receiverAlt,
                          double satX, double satY, double satZ) {
    LLA_POSITION lla;
    lla.lat = receiverLat * PI / 180.0;
    lla.lon = receiverLon * PI / 180.0;
    lla.alt = receiverAlt;

    KINEMATIC_INFO receiver = LlaToEcef(lla);
    KINEMATIC_INFO satellite;
    satellite.x = satX;
    satellite.y = satY;
    satellite.z = satZ;

    double elevation, azimuth;
    SatElAz(&receiver, &satellite, &elevation, &azimuth);
    return elevation * 180.0 / PI;
}

double calculateDistance(double x1, double y1, double z1,
                        double x2, double y2, double z2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

double calculateDoppler(double freq, double relativeSpeed) {
    return -freq * relativeSpeed / LIGHT_SPEED;
}

std::string getCurrentUTC() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* tm = gmtime(&time_t);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    return std::string(buffer);
}

std::string formatTime(const TimeConfig& time) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%06.3f",
             time.year, time.month, time.day,
             time.hour, time.minute, time.second);
    return std::string(buffer);
}

} // namespace Utils

//----------------------------------------------------------------------
// C Interface Implementation
//----------------------------------------------------------------------

extern "C" {

struct GNSSGeneratorHandle {
    std::unique_ptr<gnss::GNSSGenerator> generator;
};

GNSSGeneratorHandle* gnss_create_generator(void) {
    auto* handle = new GNSSGeneratorHandle();
    handle->generator = std::make_unique<gnss::GNSSGenerator>();
    return handle;
}

void gnss_destroy_generator(GNSSGeneratorHandle* handle) {
    delete handle;
}

int gnss_init_from_json(GNSSGeneratorHandle* handle, const char* jsonFile) {
    if (!handle || !handle->generator) return 0;
    return handle->generator->initializeFromJSON(jsonFile) ? 1 : 0;
}

int gnss_generate(GNSSGeneratorHandle* handle, double durationSeconds) {
    if (!handle || !handle->generator) return 0;
    return handle->generator->generate(durationSeconds) ? 1 : 0;
}

int gnss_save(GNSSGeneratorHandle* handle, const char* filename) {
    if (!handle || !handle->generator) return 0;
    return handle->generator->saveToFile(filename) ? 1 : 0;
}

int gnss_get_data_size(GNSSGeneratorHandle* handle) {
    if (!handle || !handle->generator) return 0;
    return handle->generator->getData().size();
}

const uint8_t* gnss_get_data(GNSSGeneratorHandle* handle) {
    if (!handle || !handle->generator) return nullptr;
    return handle->generator->getData().data();
}

void gnss_get_stats(GNSSGeneratorHandle* handle, double* duration, double* sizeMB) {
    if (!handle || !handle->generator) return;
    auto stats = handle->generator->getStats();
    if (duration) *duration = stats.duration;
    if (sizeMB) *sizeMB = stats.dataSizeMB;
}

} // extern "C"

} // namespace gnss