#include "gnss/gnsslib.h"
#include "gnss/IfGenEngine.h"

#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace gnss;

/*
 * GNSSConfig -> IfGenParams (a motor kanonikus belso reprezentacioja).
 */
IfGenParams ToIfGenParams(const GNSSConfig& c)
{
    IfGenParams p{};

    /* Runtime / engine options. */
    p.writeBin       = c.writeBin;
    p.writeTag       = c.writeTag;
    p.multiThread    = c.multiThread;
    p.nThreads       = c.nThreads;
    p.verbose        = c.verbose;
    p.validateOnly   = c.validateOnly;
    p.actualizeRinex = c.actualizeRinex;
    p.noiseSeed      = c.noiseSeed;
    p.gain           = c.gain;

    /* Time. */
    p.time.year   = c.time.year;
    p.time.month  = c.time.month;
    p.time.day    = c.time.day;
    p.time.hour   = c.time.hour;
    p.time.minute = c.time.minute;
    p.time.second = c.time.second;

    /* Position / velocity (segment mode). */
    p.position.latitudeDeg  = c.position.latitude;
    p.position.longitudeDeg = c.position.longitude;
    p.position.altitudeM    = c.position.altitude;

    p.velocity.speedMps  = c.velocity.speed;
    p.velocity.courseDeg = c.velocity.course;

    /* Trajectory segments (segment mode). */
    p.trajectory.clear();
    p.trajectory.reserve(c.trajectory.size());

    for (const auto& src : c.trajectory) {
        IfGenTrajectorySegment dst{};

        switch (src.type) {
        case TrajectoryType::ConstSpeed:
            dst.type     = IfGenTrajectoryType::ConstSpeed;
            dst.speedMps = src.speed;
            break;

        case TrajectoryType::ConstAcc:
            dst.type      = IfGenTrajectoryType::ConstAcc;
            dst.accelMps2 = src.acceleration;
            break;

        case TrajectoryType::VerticalAcc:
            dst.type      = IfGenTrajectoryType::VerticalAcc;
            dst.accelMps2 = src.acceleration;
            break;

        case TrajectoryType::Jerk:
            dst.type     = IfGenTrajectoryType::Jerk;
            dst.jerkMps3 = (src.jerk != 0.0) ? src.jerk : src.acceleration;  // regi kompatibilitas
            break;

        case TrajectoryType::HorizontalTurn:
            dst.type         = IfGenTrajectoryType::HorizontalTurn;
            dst.radiusM      = src.radius;
            // regi kompatibilitas: ha nincs turnRate es radius, az acceleration a szogsebesseg
            dst.turnRateDegS = (src.turnRate != 0.0) ? src.turnRate
                             : (src.radius == 0.0)   ? src.acceleration
                                                     : 0.0;
            break;
        }

        dst.durationS = src.duration;
        dst.courseDeg = src.course;

        p.trajectory.push_back(dst);
    }

    /* Waypoints (waypoint mode). */
    p.waypoints.clear();
    p.waypoints.reserve(c.waypoints.size());
    for (const auto& w : c.waypoints) {
        IfGenWaypoint g{};
        g.latitudeDeg  = w.latitude;
        g.longitudeDeg = w.longitude;
        g.altitudeM    = w.altitude;
        g.timeS        = w.time;
        g.speedMps     = w.speed;
        p.waypoints.push_back(g);
    }
    p.waypointInterp = (c.waypointInterpolation == WaypointInterpolation::Linear)
                           ? IfGenWaypointInterp::Linear
                           : IfGenWaypointInterp::Spline;
    p.waypointDefaultSpeedMps = c.waypointDefaultSpeed;

    /* Navigation file. */
    p.ephemerisFile = c.ephemerisFile;

    /* Output. */
    p.output.sampleRateHz     = c.output.sampleRate;
    p.output.centerFreqHz     = c.output.centerFreq;
    p.output.elevationMaskDeg = c.output.elevationMask;
    p.output.binFile          = c.output.binFile;

    if (!c.output.filename.empty() && p.output.binFile.empty()) {
        p.output.binFile = c.output.filename;
    }

    switch (c.output.format) {
    case OutputFormat::IQ2:  p.output.format = IfGenFormat::IQ2;  break;
    case OutputFormat::IQ4:  p.output.format = IfGenFormat::IQ4;  break;
    case OutputFormat::IQ8:  p.output.format = IfGenFormat::IQ8;  break;
    case OutputFormat::IQ16: p.output.format = IfGenFormat::IQ16; break;
    }

    /* Power. */
    p.power.noiseFloorDbm   = c.power.noiseFloor;
    p.power.initCN0DbHz     = c.power.initPower;
    p.power.elevationAdjust = c.power.elevationAdjust;

    /* Signals. */
    p.signals.clear();
    p.signals.reserve(c.signals.size());

    for (const auto& src : c.signals) {
        if (!src.enable) {
            continue;
        }

        IfGenSignalSelect dst{};
        dst.enable = true;

        switch (src.system) {
        case SystemType::GPS:     dst.system = IfGenSystem::GPS;     break;
        case SystemType::BDS:     dst.system = IfGenSystem::BDS;     break;
        case SystemType::Galileo: dst.system = IfGenSystem::Galileo; break;
        case SystemType::GLONASS: dst.system = IfGenSystem::GLONASS; break;
        }

        switch (src.signal) {
        case SignalType::GPS_L1CA: dst.signal = IfGenSignal::GPS_L1CA; break;
        case SignalType::GPS_L1C:  dst.signal = IfGenSignal::GPS_L1C;  break;
        case SignalType::GPS_L2C:  dst.signal = IfGenSignal::GPS_L2C;  break;
        case SignalType::GPS_L2P:  dst.signal = IfGenSignal::GPS_L2P;  break;
        case SignalType::GPS_L5:   dst.signal = IfGenSignal::GPS_L5;   break;

        case SignalType::BDS_B1C: dst.signal = IfGenSignal::BDS_B1C; break;
        case SignalType::BDS_B1I: dst.signal = IfGenSignal::BDS_B1I; break;
        case SignalType::BDS_B2I: dst.signal = IfGenSignal::BDS_B2I; break;
        case SignalType::BDS_B3I: dst.signal = IfGenSignal::BDS_B3I; break;
        case SignalType::BDS_B2a: dst.signal = IfGenSignal::BDS_B2a; break;
        case SignalType::BDS_B2b: dst.signal = IfGenSignal::BDS_B2b; break;

        case SignalType::GAL_E1:  dst.signal = IfGenSignal::GAL_E1;  break;
        case SignalType::GAL_E5a: dst.signal = IfGenSignal::GAL_E5a; break;
        case SignalType::GAL_E5b: dst.signal = IfGenSignal::GAL_E5b; break;
        case SignalType::GAL_E5:  dst.signal = IfGenSignal::GAL_E5;  break;
        case SignalType::GAL_E6:  dst.signal = IfGenSignal::GAL_E6;  break;

        case SignalType::GLO_G1:  dst.signal = IfGenSignal::GLO_G1;  break;
        case SignalType::GLO_G2:  dst.signal = IfGenSignal::GLO_G2;  break;
        case SignalType::GLO_G3:  dst.signal = IfGenSignal::GLO_G3;  break;
        }

        p.signals.push_back(dst);
    }

    return p;
}

/*
 * Lefuttatja a generalast a megadott idotartamig (vagy a trajektoria vegeig),
 * lezarja az engine-t (igy a .bin fajl veglegesen kiirodik), majd kitolti a statisztikat.
 */
struct RunResult {
    std::uint64_t samples = 0;
    double        sampleRateHz = 0.0;
    int           channels = 0;
    std::string   outputFile;
    double        elapsedS = 0.0;
};

RunResult RunEngine(IfGenEngine& engine, double durationSeconds)
{
    const auto start = std::chrono::steady_clock::now();

    const IfGenInfo info = engine.Info();           // masolat: a Close() utan mar nem elerheto
    const double maxSamples = durationSeconds * info.sampleRateHz;

    std::uint64_t total = 0;
    engine.Stream([&](const std::complex<float>*, std::size_t n) {
        total += n;
        return static_cast<double>(total) < maxSamples;   // 1 ms-os blokk-granularitas
    });

    engine.Close();                                 // fajl flush/close

    RunResult r;
    r.samples      = total;
    r.sampleRateHz = info.sampleRateHz;
    r.channels     = info.nChannels;
    r.outputFile   = info.outputFile;
    r.elapsedS     = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return r;
}

} // anonymous namespace


namespace gnss {


struct GNSSGenerator::Impl {
    std::unique_ptr<IfGenEngine> engine;
    std::string jsonFile;
    std::string outputOverride;
    std::string producedFile;
    bool streamOpen = false;
};


GNSSGenerator::GNSSGenerator()
    : m_impl(std::make_unique<Impl>())
{
}


GNSSGenerator::~GNSSGenerator() = default;


GNSSGenerator::GNSSGenerator(GNSSGenerator&& other) noexcept
    : m_impl(std::move(other.m_impl)),
      m_config(std::move(other.m_config)),
      m_stats(other.m_stats),
      m_data(std::move(other.m_data)),
      m_initialized(other.m_initialized)
{
    other.m_initialized = false;
}


GNSSGenerator& GNSSGenerator::operator=(GNSSGenerator&& other) noexcept
{
    if (this == &other) return *this;

    m_impl         = std::move(other.m_impl);
    m_config       = std::move(other.m_config);
    m_stats        = other.m_stats;
    m_data         = std::move(other.m_data);
    m_initialized  = other.m_initialized;

    other.m_initialized = false;
    return *this;
}


bool GNSSGenerator::initialize(const GNSSConfig& config)
{
    if (!m_impl) m_impl = std::make_unique<Impl>();   // move utan nullptr lehet

    try {
        IfGenParams params = ToIfGenParams(config);

        std::vector<std::string> errors;
        if (!ValidateIfGenParams(params, errors)) {
            std::cerr << "GNSSGenerator::initialize(): invalid configuration\n";
            for (const auto& error : errors) {
                std::cerr << "  - " << error << '\n';
            }
            return false;
        }

        auto engine = std::make_unique<IfGenEngine>();
        engine->Init(params);

        m_config = config;

        m_impl->jsonFile.clear();
        m_impl->outputOverride.clear();
        m_impl->producedFile.clear();

        m_impl->engine     = std::move(engine);
        m_impl->streamOpen = false;

        m_initialized = true;
        m_stats = GenerationStats{};
        m_data.clear();

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::initialize(): " << e.what() << '\n';
        m_initialized = false;
        m_impl->engine.reset(); m_impl->streamOpen = false;
        return false;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::initialize(): unknown exception\n";
        m_initialized = false;
        m_impl->engine.reset(); m_impl->streamOpen = false;
        return false;
    }
}


bool GNSSGenerator::initializeFromJSON(const std::string& jsonFile)
{
    if (jsonFile.empty()) {
        std::cerr << "GNSSGenerator::initializeFromJSON(): empty JSON file path\n";
        return false;
    }
    if (!m_impl) m_impl = std::make_unique<Impl>();

    try {
        auto engine = std::make_unique<IfGenEngine>();

        IfGenOptions options;
        options.configFile     = jsonFile;
        options.validateOnly   = true;
        options.actualizeRinex = false;

        engine->Init(options);

        const IfGenInfo& info = engine->Info();

        m_config = GNSSConfig{};
        m_config.output.filename   = info.outputFile;
        m_config.output.sampleRate = info.sampleRateHz;
        m_config.output.centerFreq = info.centerFreqHz;

        switch (info.formatBits) {
        case 2:  m_config.output.format = OutputFormat::IQ2;  break;
        case 4:  m_config.output.format = OutputFormat::IQ4;  break;
        case 16: m_config.output.format = OutputFormat::IQ16; break;
        default: m_config.output.format = OutputFormat::IQ8;  break;
        }

        m_impl->jsonFile = jsonFile;
        m_impl->outputOverride.clear();
        m_impl->producedFile.clear();

        m_impl->engine.reset();
        m_impl->streamOpen = false;

        m_initialized = true;
        m_stats = GenerationStats{};
        m_data.clear();

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::initializeFromJSON(): " << e.what() << '\n';
        m_initialized = false;
        return false;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::initializeFromJSON(): unknown exception\n";
        m_initialized = false;
        return false;
    }
}


void GNSSGenerator::setOutputFile(const std::string& filename)
{
    if (!m_impl) m_impl = std::make_unique<Impl>();

    m_impl->outputOverride = filename;
    m_config.output.filename = filename;
    m_config.output.binFile  = filename;
}


bool GNSSGenerator::generate(double durationSeconds)
{
    if (!m_initialized || !m_impl) {
        std::cerr << "GNSSGenerator::generate(): generator is not initialized\n";
        return false;
    }
    if (durationSeconds <= 0.0) {
        std::cerr << "GNSSGenerator::generate(): duration must be positive\n";
        return false;
    }

    try {
        closeStream();   // egy esetleg nyitott stream ugyanazt a fajlt irna

        auto engine = std::make_unique<IfGenEngine>();

        if (!m_impl->jsonFile.empty()) {
            IfGenOptions options;
            options.configFile     = m_impl->jsonFile;
            options.writeBin       = true;
            options.actualizeRinex = m_config.actualizeRinex;
            options.gain           = m_config.gain;
            if (!m_impl->outputOverride.empty()) {
                options.binFile = m_impl->outputOverride;
            }
            engine->Init(options);
        }
        else {
            GNSSConfig cfg = m_config;
            cfg.writeBin = true;

            if (!m_impl->outputOverride.empty()) {
                cfg.output.binFile  = m_impl->outputOverride;
                cfg.output.filename = m_impl->outputOverride;
            }

            IfGenParams params = ToIfGenParams(cfg);
            params.writeBin = true;
            engine->Init(params);
        }

        const RunResult r = RunEngine(*engine, durationSeconds);

        m_stats.duration          = (r.sampleRateHz > 0.0) ? static_cast<double>(r.samples) / r.sampleRateHz : 0.0;
        m_stats.generationTime    = r.elapsedS;
        m_stats.totalSamples      = r.samples;
        m_stats.visibleSatellites = r.channels;

        std::error_code ec;
        if (!r.outputFile.empty() && std::filesystem::exists(r.outputFile, ec)) {
            const auto size = std::filesystem::file_size(r.outputFile, ec);
            if (!ec) m_stats.dataSizeMB = static_cast<double>(size) / (1024.0 * 1024.0);
            m_impl->producedFile = r.outputFile;
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::generate(): " << e.what() << '\n';
        return false;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::generate(): unknown exception\n";
        return false;
    }
}


bool GNSSGenerator::generate(int durationMs)
{
    if (durationMs <= 0) return false;
    return generate(static_cast<double>(durationMs) / 1000.0);
}


bool GNSSGenerator::openStream(bool writeBin, const std::string& binFile, float gain)
{
    if (!m_initialized || !m_impl) {
        std::cerr << "GNSSGenerator::openStream(): generator is not initialized\n";
        return false;
    }

    try {
        closeStream();

        if (!m_impl->jsonFile.empty()) {
            auto engine = std::make_unique<IfGenEngine>();
            IfGenOptions options;
            options.configFile     = m_impl->jsonFile;
            options.writeBin       = writeBin;
            options.actualizeRinex = m_config.actualizeRinex;
            options.gain           = gain;

            if (!binFile.empty())                       options.binFile = binFile;
            else if (!m_impl->outputOverride.empty())   options.binFile = m_impl->outputOverride;

            engine->Init(options);

            m_impl->engine     = std::move(engine);
            m_impl->streamOpen = true;
            return true;
        }

        GNSSConfig cfg = m_config;
        cfg.writeBin = writeBin;
        cfg.gain     = gain;

        if (!binFile.empty()) {
            cfg.output.binFile  = binFile;
            cfg.output.filename = binFile;
        } else if (!m_impl->outputOverride.empty()) {
            cfg.output.binFile  = m_impl->outputOverride;
            cfg.output.filename = m_impl->outputOverride;
        }

        IfGenParams params = ToIfGenParams(cfg);
        params.writeBin = writeBin;
        params.gain     = gain;

        auto engine = std::make_unique<IfGenEngine>();
        engine->Init(params);

        m_impl->engine     = std::move(engine);
        m_impl->streamOpen = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::openStream(): " << e.what() << '\n';
        m_impl->engine.reset();
        m_impl->streamOpen = false;
        return false;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::openStream(): unknown exception\n";
        m_impl->engine.reset();
        m_impl->streamOpen = false;
        return false;
    }
}


bool GNSSGenerator::openStream(IfGenEngine& engine, bool writeBin, const std::string& binFile, float gain)
{
    if (!m_initialized || !m_impl) {
        std::cerr << "GNSSGenerator::openStream(engine): generator is not initialized\n";
        return false;
    }

    try {
        if (!m_impl->jsonFile.empty()) {
            IfGenOptions options;
            options.configFile     = m_impl->jsonFile;
            options.writeBin       = writeBin;
            options.actualizeRinex = m_config.actualizeRinex;
            options.gain           = gain;

            if (!binFile.empty())                       options.binFile = binFile;
            else if (!m_impl->outputOverride.empty())   options.binFile = m_impl->outputOverride;

            engine.Init(options);
            return true;
        }

        GNSSConfig cfg = m_config;
        cfg.writeBin = writeBin;
        cfg.gain     = gain;

        if (!binFile.empty()) {
            cfg.output.binFile  = binFile;
            cfg.output.filename = binFile;
        } else if (!m_impl->outputOverride.empty()) {
            cfg.output.binFile  = m_impl->outputOverride;
            cfg.output.filename = m_impl->outputOverride;
        }

        IfGenParams params = ToIfGenParams(cfg);
        params.writeBin = writeBin;
        params.gain     = gain;

        engine.Init(params);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::openStream(engine): " << e.what() << '\n';
        return false;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::openStream(engine): unknown exception\n";
        return false;
    }
}


std::size_t GNSSGenerator::read(std::complex<float>* out, std::size_t n)
{
    if (!m_initialized || !m_impl || !m_impl->engine || !m_impl->streamOpen) return 0;
    if (out == nullptr || n == 0) return 0;

    try {
        return m_impl->engine->Read(out, n);
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::read(): " << e.what() << '\n';
        return 0;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::read(): unknown exception\n";
        return 0;
    }
}


std::size_t GNSSGenerator::stream(
    const IfGenEngine::SampleCallback& callback,
    std::size_t chunk)
{
    if (!m_initialized || !m_impl || !m_impl->engine || !m_impl->streamOpen) return 0;
    if (!callback) return 0;

    try {
        return m_impl->engine->Stream(callback, chunk);
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::stream(): " << e.what() << '\n';
        return 0;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::stream(): unknown exception\n";
        return 0;
    }
}


void GNSSGenerator::closeStream()
{
    if (!m_impl) return;

    if (m_impl->engine) {
        try {
            m_impl->engine->Close();
        }
        catch (...) {
            /* Close is intentionally noexcept from the public generator perspective. */
        }
        m_impl->engine.reset();
    }

    m_impl->streamOpen = false;
}


bool GNSSGenerator::saveToFile(const std::string& filename)
{
    if (!m_impl) return false;

    /* A forras mindig a generalt fajl; a filename a CEL. */
    const std::string& source = m_impl->producedFile;

    if (source.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(source, ec) || ec) {
        return false;
    }

    /* Nincs cel: nincs mit masolni, a generalt fajl letezik. */
    if (filename.empty()) {
        return true;
    }

    /* Ugyanaz a fajl: nincs teendo. */
    {
        std::error_code ec2;
        if (std::filesystem::exists(filename, ec2) && !ec2 &&
            std::filesystem::equivalent(source, filename, ec2) && !ec2) {
            return true;
        }
    }

    try {
        std::filesystem::copy_file(
            source,
            filename,
            std::filesystem::copy_options::overwrite_existing
        );

        m_impl->producedFile = filename;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "GNSSGenerator::saveToFile(): " << e.what() << '\n';
        return false;
    }
    catch (...) {
        std::cerr << "GNSSGenerator::saveToFile(): unknown exception\n";
        return false;
    }
}


void GNSSGenerator::clear()
{
    closeStream();

    if (m_impl) {
        m_impl->jsonFile.clear();
        m_impl->outputOverride.clear();
        m_impl->producedFile.clear();
    }

    m_config = GNSSConfig{};
    m_stats  = GenerationStats{};
    m_data.clear();

    m_initialized = false;
}


} // namespace gnss