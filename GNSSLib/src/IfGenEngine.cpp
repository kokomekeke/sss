// Sisi/GNSSLib/src/IfGenEngine.cpp
// IfGenEngine.cpp -- az IFDataGen main()-jenek globalok nelkuli konyvtari valtozata.
//
// Ket setup-ut van, amik ugyanabba a kozos vegbe (FinishSetup) futnak bele:
//   - Setup(IfGenOptions)      : JSON-t olvas be (AssignParameters), mint eddig.
//   - SetupFromParams(IfGenParams): semmilyen JSON-t nem olvas.
//
// Ketfele trajektoria-forras:
//   - szegmens-mod : SignalSim CTrajectory (kezdopont + kezdosebesseg + szegmensek)
//   - waypoint-mod : WaypointPath (pontlista, linearis / spline), a SignalSim-et nem erinti
//
// FONTOS: ez a fajl NEM modositja a SignalSim forrasfajljait.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wreturn-type"
#  pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif

#include "SignalSim.h"
#include "ActualizeRinex.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

#include "gnss/IfGenEngine.h"

#define TOTAL_GPS_SAT 32
#define TOTAL_BDS_SAT 63
#define TOTAL_GAL_SAT 36
#define TOTAL_GLO_SAT 24
#define TOTAL_SAT_CHANNEL 128
#define NAVBIT_COUNT 12

namespace {

enum DataBitType {
    DataBitLNav, DataBitCNav, DataBitCNav2,
    DataBitGNav, DataBitGNav2,
    DataBitD1D2, DataBitBCNav1, DataBitBCNav2, DataBitBCNav3,
    DataBitINav, DataBitFNav, DataBitECNav,
    DataBitSbas,
};

// Rendszerenkenti kozepfrekvenciak a jel-indexek sorrendjeben (GPS, BDS, Galileo, GLONASS).
// GLO_G3 (CDMA L3) szandekosan nincs benne; a ValidateIfGenParams() elutasitja.
const int SignalCenterFreq[][8] = {
    { FREQ_GPS_L1, FREQ_GPS_L1, FREQ_GPS_L2, FREQ_GPS_L2, FREQ_GPS_L5 },
    { FREQ_BDS_B1C, FREQ_BDS_B1I, FREQ_BDS_B2I, FREQ_BDS_B3I, FREQ_BDS_B2a, FREQ_BDS_B2b, FREQ_BDS_B2ab },
    { FREQ_GAL_E1, FREQ_GAL_E5a, FREQ_GAL_E5b, FREQ_GAL_E5, FREQ_GAL_E6 },
    { FREQ_GLO_G1, FREQ_GLO_G2, 0 },
};

// ============================================================================
// Waypoint utvonal
// ============================================================================

class WaypointPath {
public:
    void Build(const std::vector<IfGenWaypoint>& wp, IfGenWaypointInterp interp, double defSpeed) {
        const std::size_t n = wp.size();
        spline = (interp == IfGenWaypointInterp::Spline) && n > 2;
        P.resize(n);
        T.assign(n, 0.0);
        M.assign(n, std::array<double, 3>{ 0.0, 0.0, 0.0 });

        for (std::size_t i = 0; i < n; i++) {
            LLA_POSITION l;
            l.lat = wp[i].latitudeDeg  * PI / 180.0;
            l.lon = wp[i].longitudeDeg * PI / 180.0;
            l.alt = wp[i].altitudeM;
            KINEMATIC_INFO k = LlaToEcef(l);
            P[i] = { k.x, k.y, k.z };
        }

        const bool timed = wp[0].timeS >= 0.0;
        for (std::size_t i = 1; i < n; i++) {
            if (timed) {
                T[i] = wp[i].timeS - wp[0].timeS;
            } else {
                double dx = P[i][0] - P[i-1][0], dy = P[i][1] - P[i-1][1], dz = P[i][2] - P[i-1][2];
                double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                double v = wp[i-1].speedMps > 0.0 ? wp[i-1].speedMps : defSpeed;
                T[i] = T[i-1] + std::max(dist / v, 1e-3);
            }
        }

        // Catmull-Rom tangensek (nem egyenletes idokozre), a veg-pontokon egyoldali kulonbseg.
        for (std::size_t i = 0; i < n; i++) {
            std::size_t a = (i == 0) ? 0 : i - 1;
            std::size_t b = (i == n - 1) ? n - 1 : i + 1;
            for (int c = 0; c < 3; c++) M[i][c] = (P[b][c] - P[a][c]) / (T[b] - T[a]);
        }
    }

    double Duration() const { return T.empty() ? 0.0 : T.back(); }

    KINEMATIC_INFO At(double t) const {
        t = std::min(std::max(t, 0.0), T.back());
        std::size_t i = std::upper_bound(T.begin(), T.end(), t) - T.begin();
        i = (i == 0) ? 0 : i - 1;
        if (i > T.size() - 2) i = T.size() - 2;

        const double h = T[i+1] - T[i];
        const double u = (t - T[i]) / h;
        double pos[3], vel[3];

        for (int c = 0; c < 3; c++) {
            if (!spline) {
                pos[c] = P[i][c] + (P[i+1][c] - P[i][c]) * u;
                vel[c] = (P[i+1][c] - P[i][c]) / h;
            } else {
                const double u2 = u*u, u3 = u2*u;
                pos[c] = (2*u3 - 3*u2 + 1) * P[i][c]   + (u3 - 2*u2 + u) * h * M[i][c]
                       + (-2*u3 + 3*u2)    * P[i+1][c] + (u3 - u2)       * h * M[i+1][c];
                vel[c] = ((6*u2 - 6*u) * P[i][c]   + (3*u2 - 4*u + 1) * h * M[i][c]
                        + (-6*u2 + 6*u) * P[i+1][c] + (3*u2 - 2*u)     * h * M[i+1][c]) / h;
            }
        }
        KINEMATIC_INFO k{};
        k.x = pos[0]; k.y = pos[1]; k.z = pos[2];
        k.vx = vel[0]; k.vy = vel[1]; k.vz = vel[2];
        return k;
    }

private:
    bool spline = false;
    std::vector<std::array<double, 3>> P, M;
    std::vector<double> T;
};

// Waypointok ellenorzese (kozos a JSON es a struct ut kozott).
void ValidateWaypoints(const std::vector<IfGenWaypoint>& w, double defSpeed,
                       std::vector<std::string>& errors) {
    if (w.size() < 2) { errors.push_back("waypoints needs at least 2 points"); return; }

    std::size_t timed = 0;
    for (auto& p : w) if (p.timeS >= 0.0) timed++;
    if (timed != 0 && timed != w.size())
        errors.push_back("waypoints: timeS must be given for all points or for none");

    for (std::size_t i = 0; i < w.size(); i++) {
        const std::string id = "waypoints[" + std::to_string(i) + "]";
        if (w[i].latitudeDeg < -90 || w[i].latitudeDeg > 90)
            errors.push_back(id + ".latitudeDeg out of range");
        if (w[i].longitudeDeg < -180 || w[i].longitudeDeg > 180)
            errors.push_back(id + ".longitudeDeg out of range");
        if (timed == w.size() && i > 0 && w[i].timeS <= w[i-1].timeS)
            errors.push_back(id + ".timeS must be strictly increasing");
        if (timed == 0 && i + 1 < w.size() && !(w[i].speedMps > 0.0) && !(defSpeed > 0.0))
            errors.push_back(id + ": no timeS and no speed (set speedMps or waypointDefaultSpeedMps)");
    }
}

// "trajectory" -> "waypointList" kiolvasasa a JSON fabol (a SignalSim JsonInterpreter-t nem erintjuk).
bool ParseWaypointsJson(JsonObject *root, std::vector<IfGenWaypoint>& out,
                        IfGenWaypointInterp& interp, double& defSpeed) {
    out.clear();
    interp = IfGenWaypointInterp::Spline;
    defSpeed = 0.0;

    for (JsonObject *o = root->GetFirstObject(); o; o = o->GetNextObject()) {
        if (std::strcmp(o->Key, "trajectory") != 0) continue;
        for (JsonObject *t = o->GetFirstObject(); t; t = t->GetNextObject()) {
            if (!std::strcmp(t->Key, "interpolation") && t->Type == JsonObject::ValueTypeString)
                interp = std::strcmp(t->String, "linear") == 0 ? IfGenWaypointInterp::Linear
                                                               : IfGenWaypointInterp::Spline;
            else if (!std::strcmp(t->Key, "defaultSpeed"))
                defSpeed = GET_DOUBLE_VALUE(t);
            else if (!std::strcmp(t->Key, "waypointList") && t->Type == JsonObject::ValueTypeArray) {
                for (JsonObject *e = t->GetFirstObject(); e; e = e->GetNextObject()) {
                    if (e->Type != JsonObject::ValueTypeObject) continue;
                    IfGenWaypoint w;
                    for (JsonObject *f = e->GetFirstObject(); f; f = f->GetNextObject()) {
                        if      (!std::strcmp(f->Key, "latitude"))  w.latitudeDeg  = GET_DOUBLE_VALUE(f);
                        else if (!std::strcmp(f->Key, "longitude")) w.longitudeDeg = GET_DOUBLE_VALUE(f);
                        else if (!std::strcmp(f->Key, "altitude"))  w.altitudeM    = GET_DOUBLE_VALUE(f);
                        else if (!std::strcmp(f->Key, "time"))      w.timeS        = GET_DOUBLE_VALUE(f);
                        else if (!std::strcmp(f->Key, "speed"))     w.speedMps     = GET_DOUBLE_VALUE(f);
                    }
                    out.push_back(w);
                }
            }
        }
    }
    return !out.empty();
}

// ============================================================================
// Kvantalas
// ============================================================================

int Quant2(const complex_number *s, int n, unsigned char *q, double gs) {
    int clip = 0;
    const double th = 1.1 / gs, ct = 5 * th;
    auto bits = [&](double v, int sBit, int mBit) {
        double a = std::fabs(v);
        if (a >= ct) ++clip;
        return (v < 0.0 ? sBit : 0) | (a < th ? 0 : mBit);
    };
    for (int i = 0; i + 1 < n; i += 2) {
        q[i / 2] = (unsigned char)(bits(s[i].real, 2, 1) | bits(s[i].imag, 8, 4) |
                                   bits(s[i + 1].real, 0x20, 0x10) | bits(s[i + 1].imag, 0x80, 0x40));
    }
    return clip;
}

int Quant4(const complex_number *s, int n, unsigned char *q, double gs) {
    int clip = 0;
    const double g = gs * 3.0;
    auto one = [&](double v) {
        int m = (int)(std::fabs(v) * g);
        if (m > 7) { m = 7; ++clip; }
        if (v < 0.0) m |= 8;
        return m;
    };
    for (int i = 0; i < n; i++) q[i] = (unsigned char)((one(s[i].real) << 4) | one(s[i].imag));
    return clip;
}

int Quant8(const complex_number *s, int n, unsigned char *q, double gs) {
    int clip = 0;
    const double g = gs * 25.0;
    auto one = [&](double v) {
        int x = (int)(v * g);
        if (x > 127) { x = 127; ++clip; } else if (x < -128) { x = -128; ++clip; }
        return (unsigned char)(x & 0xff);
    };
    for (int i = 0; i < n; i++) { q[2 * i] = one(s[i].real); q[2 * i + 1] = one(s[i].imag); }
    return clip;
}

int Quant16(const complex_number *s, int n, unsigned char *q, double gs) {
    int clip = 0;
    const double g = gs * 3277;
    auto one = [&](double v, unsigned char *dst) {
        int x = (int)(v * g);
        if (x > 32767) { x = 32767; ++clip; } else if (x < -32768) { x = -32768; ++clip; }
        dst[0] = (unsigned char)(x & 0xff);
        dst[1] = (unsigned char)((x >> 8) & 0xff);
    };
    for (int i = 0; i < n; i++) { one(s[i].real, q + 4 * i); one(s[i].imag, q + 4 * i + 2); }
    return clip;
}

// ============================================================================
// .bin iro
// ============================================================================

class BinWriter {
public:
    bool Open(const char *path, int format, int samplesPerMs, bool verb) {
        f = std::fopen(path, "wb");
        fmt = format; n = samplesPerMs; verbose = verb;
        q.assign((size_t)n * 4, 0);
        return f != nullptr;
    }
    void Write(const complex_number *s, int len) {
        int clip = 0; size_t bytes = 0;
        switch (fmt) {
        case OutputFormatIQ2:  clip = Quant2 (s, len, q.data(), agc); bytes = len / 2; break;
        case OutputFormatIQ4:  clip = Quant4 (s, len, q.data(), agc); bytes = len;     break;
        case OutputFormatIQ16: clip = Quant16(s, len, q.data(), agc); bytes = (size_t)len * 4; break;
        default:               clip = Quant8 (s, len, q.data(), agc); bytes = (size_t)len * 2; break;
        }
        std::fwrite(q.data(), 1, bytes, f);
        clipped += clip; total += (long long)len * 2;
        allClipped += clip; allTotal += (long long)len * 2;
        if (++cycle % 100 == 0) {
            double r = (double)clipped / total;
            if (r > 0.01)                    { agc *= 0.95; clipped = 0; total = 0; }
            else if (r < 0.001 && agc < 1.0) { agc = std::min(1.0, agc * 1.02); clipped = 0; total = 0; }
        }
    }
    void Close() {
        if (!f) return;
        std::fclose(f); f = nullptr;
        if (verbose && allTotal > 0)
            std::fprintf(stderr, "[IfGen] bin closed: clipped %.4f%%, final AGC gain %.3f\n",
                         100.0 * allClipped / allTotal, agc);
    }
    ~BinWriter() { Close(); }
private:
    FILE *f = nullptr;
    int fmt = 0, n = 0, cycle = 0;
    bool verbose = false;
    double agc = 1.0;
    long long clipped = 0, total = 0, allClipped = 0, allTotal = 0;
    std::vector<unsigned char> q;
};

void WriteTagFile(const std::string &path, const OUTPUT_PARAM &o) {
    FILE *t = std::fopen(path.c_str(), "w");
    if (!t) { std::fprintf(stderr, "[IfGen] could not create tag file: %s\n", path.c_str()); return; }
    const char *fmtStr = "INT8X2", *bits = "8";
    if (o.Format == OutputFormatIQ2)       { fmtStr = "INT2X2";  bits = "2"; }
    else if (o.Format == OutputFormatIQ4)  { fmtStr = "INT4X2";  bits = "4"; }
    else if (o.Format == OutputFormatIQ16) { fmtStr = "INT16X2"; bits = "16"; }

    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    int ms = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
    std::tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &tt);
#else
    gmtime_r(&tt, &tmv);
#endif
    std::fprintf(t, "PROG = IFDataGen\n");
    std::fprintf(t, "TIME = %04d/%02d/%02d %02d:%02d:%02d.%03d\n", tmv.tm_year + 1900, tmv.tm_mon + 1,
                 tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms);
    std::fprintf(t, "FMT  = %s\n", fmtStr);
    std::fprintf(t, "F_S  = %.6f\n", o.SampleFreq / 1e3);
    std::fprintf(t, "F_LO = %.6f\n", o.CenterFreq / 1e3);
    std::fprintf(t, "IQ   = 1\n");
    std::fprintf(t, "BITS = %s\n", bits);
    std::fprintf(t, "SCALE = 1.0\n");
    std::fclose(t);
}

// ============================================================================
// RNG: gyors, chunkonkent (seed, blokk, chunk) alapu, szalfuggetlenul reprodukalhato
// ============================================================================

struct SplitMix64 {
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uni() { return double(next() >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0; }
};

complex_number PolarNoise(SplitMix64 &g) {
    double a, b, m;
    do { a = g.uni(); b = g.uni(); m = a * a + b * b; } while (m >= 1.0 || m == 0.0);
    m = std::sqrt(-2.0 * std::log(m) / m);
    return complex_number(a * m, b * m);
}

} // namespace

// ============================================================================
// ValidateIfGenParams: a motor felepitese nelkuli, olcso ellenorzes.
// ============================================================================

bool ValidateIfGenParams(const IfGenParams &p, std::vector<std::string> &errors) {
    errors.clear();

    if (p.time.month < 1 || p.time.month > 12) errors.push_back("time.month out of range [1,12]");
    if (p.time.day < 1 || p.time.day > 31)      errors.push_back("time.day out of range [1,31]");
    if (p.time.hour < 0 || p.time.hour > 23)    errors.push_back("time.hour out of range [0,23]");
    if (p.time.minute < 0 || p.time.minute > 59) errors.push_back("time.minute out of range [0,59]");
    if (p.time.second < 0.0 || p.time.second >= 60.0) errors.push_back("time.second out of range [0,60)");

    const bool hasSeg = !p.trajectory.empty();
    const bool hasWp  = !p.waypoints.empty();

    if (hasSeg && hasWp)
        errors.push_back("specify either trajectory[] or waypoints[], not both");
    else if (!hasSeg && !hasWp)
        errors.push_back("trajectory[] or waypoints[] must be given");

    if (hasWp) {
        ValidateWaypoints(p.waypoints, p.waypointDefaultSpeedMps, errors);
    } else {
        // A position / velocity csak szegmens-modban szamit.
        if (p.position.latitudeDeg < -90.0 || p.position.latitudeDeg > 90.0)
            errors.push_back("position.latitudeDeg out of range [-90,90]");
        if (p.position.longitudeDeg < -180.0 || p.position.longitudeDeg > 180.0)
            errors.push_back("position.longitudeDeg out of range [-180,180]");

        for (std::size_t i = 0; i < p.trajectory.size(); i++) {
            const auto &s = p.trajectory[i];
            const std::string id = "trajectory[" + std::to_string(i) + "]";
            if (s.durationS <= 0.0)
                errors.push_back(id + ".durationS must be > 0");
            if (s.type == IfGenTrajectoryType::HorizontalTurn && s.radiusM == 0.0 && s.turnRateDegS == 0.0)
                errors.push_back(id + ": HorizontalTurn needs radiusM or turnRateDegS");
        }
    }

    if (p.ephemerisFile.empty()) {
        errors.push_back("ephemerisFile is empty");
    } else {
        std::error_code ec;
        if (!std::filesystem::exists(p.ephemerisFile, ec) || ec)
            errors.push_back("ephemerisFile does not exist: " + p.ephemerisFile);
    }

    bool anyEnabled = false;
    for (auto &s : p.signals) if (s.enable) { anyEnabled = true; break; }
    if (!anyEnabled) errors.push_back("no enabled signal in signals[]");

    // GLO_G3 (GLONASS CDMA L3) nem tamogatott ebben a buildben.
    for (auto &s : p.signals) {
        if (s.enable && s.signal == IfGenSignal::GLO_G3) {
            errors.push_back("GLO_G3 signal is not supported by this SignalSim build "
                             "(missing SIGNAL_INDEX_G3 / FREQ_GLO_G3)");
            break;
        }
    }

    if (p.output.sampleRateHz <= 0.0) errors.push_back("output.sampleRateHz must be > 0");
    if (p.output.centerFreqHz <= 0.0) errors.push_back("output.centerFreqHz must be > 0");

    if (p.output.format == IfGenFormat::IQ2) {
        long long spms = std::llround(p.output.sampleRateHz / 1000.0);
        if (spms % 2 != 0) errors.push_back("IQ2 format needs an even number of samples per ms (check sampleRateHz)");
    }

    return errors.empty();
}

// ============================================================================
// Impl
// ============================================================================

struct IfGenEngine::Impl {
    IfGenOptions opt;
    IfGenInfo info;

    CTrajectory Trajectory;
    CPowerControl PowerControl;
    CNavData NavData;
    OUTPUT_PARAM OutputParam{};

    // Waypoint-mod
    WaypointPath Path;
    bool useWaypoints = false;
    long long pathMs = 0;

    UTC_TIME UtcTime{};
    LLA_POSITION StartPos{};
    LOCAL_SPEED StartVel{};

    GNSS_TIME CurTime{};

    PGPS_EPHEMERIS GpsEph[TOTAL_GPS_SAT] = {}, GpsEphVisible[TOTAL_GPS_SAT] = {};
    PGPS_EPHEMERIS BdsEph[TOTAL_BDS_SAT] = {}, BdsEphVisible[TOTAL_BDS_SAT] = {};
    PGPS_EPHEMERIS GalEph[TOTAL_GAL_SAT] = {}, GalEphVisible[TOTAL_GAL_SAT] = {};
    PGLONASS_EPHEMERIS GloEph[TOTAL_GLO_SAT] = {}, GloEphVisible[TOTAL_GLO_SAT] = {};

    std::unique_ptr<CIonoKlobuchar8> IonoModel;

    CSatelliteParam GpsSatParam[TOTAL_GPS_SAT], BdsSatParam[TOTAL_BDS_SAT],
                    GalSatParam[TOTAL_GAL_SAT], GloSatParam[TOTAL_GLO_SAT];

    int GpsSatNumber = 0, BdsSatNumber = 0, GalSatNumber = 0, GloSatNumber = 0;

    JsonStream JsonTree;
    NavBit *NavBitArray[NAVBIT_COUNT] = {};
    CSatIfSignal *chan[TOTAL_SAT_CHANNEL] = {};
    int nChannels = 0;

    std::vector<complex_number> raw;
    std::vector<std::complex<float>> blk;
    std::size_t pos = 0, len = 0;
    bool finished = false;
    std::uint64_t blockIndex = 0;
    std::unique_ptr<BinWriter> writer;
    int nThreads = 1;

    ~Impl() {
        writer.reset();
        for (int i = 0; i < TOTAL_SAT_CHANNEL; i++) delete chan[i];
        for (int i = 0; i < NAVBIT_COUNT; i++) delete NavBitArray[i];
    }

    // ========================================================================
    // Utvonal-absztrakcio: szegmens-mod vagy waypoint-mod
    // ========================================================================

    double TrajDuration() {
        return useWaypoints ? Path.Duration() : Trajectory.GetTimeLength();
    }

    void TrajReset() {
        pathMs = 0;
        if (!useWaypoints) Trajectory.ResetTrajectoryTime();
    }

    bool TrajNext(double dt, KINEMATIC_INFO &k) {
        if (!useWaypoints) return Trajectory.GetNextPosVelECEF(dt, k);
        ++pathMs;                                           // egesz ms-ben szamolunk, nincs csuszas
        if (pathMs > std::llround(Path.Duration() * 1000.0)) return false;
        k = Path.At(pathMs * 0.001);
        return true;
    }

    // ========================================================================
    // 1) JSON alapu setup
    // ========================================================================

    void Setup(const IfGenOptions &o) {
        opt = o;
        SetOutputFile(stderr);

        if (JsonTree.ReadFile(opt.configFile.c_str()) != 0)
            throw std::runtime_error("unable to read JSON file: " + opt.configFile);

        SetJsonFilePath(opt.configFile.c_str());
        JsonObject *Object = JsonTree.GetRootObject();

        // Waypoint-mod: a "trajectory" -> "waypointList" kulon kiolvasva.
        {
            std::vector<IfGenWaypoint> wps;
            IfGenWaypointInterp interp = IfGenWaypointInterp::Spline;
            double defSpeed = 0.0;
            if (ParseWaypointsJson(Object, wps, interp, defSpeed)) {
                std::vector<std::string> errs;
                ValidateWaypoints(wps, defSpeed, errs);
                if (!errs.empty()) {
                    std::string msg = "invalid waypointList:";
                    for (auto &e : errs) msg += "\n  - " + e;
                    throw std::runtime_error(msg);
                }
                useWaypoints = true;
                Path.Build(wps, interp, defSpeed);
            }
        }

        // Waypoint-modban is lefut (ido, output, power, ephemeris miatt kell);
        // a trajectory-bol csak a "name" hasznosul, a SetTrajectory FALSE-t ad, de ez artalmatlan.
        if (!AssignParameters(Object, &UtcTime, &StartPos, &StartVel, &Trajectory, &NavData,
                              &OutputParam, &PowerControl, NULL) && !useWaypoints) {
            std::fprintf(stderr, "[IfGen] WARNING: AssignParameters reported problems in '%s'\n",
                         opt.configFile.c_str());
        }

        if (opt.actualizeRinex)
            ActualizeRinex(UtcTime, &NavData, OutputParam);

        if (opt.writeBin && !opt.binFile.empty()) {
            std::strncpy(OutputParam.filename, opt.binFile.c_str(), 255);
            OutputParam.filename[255] = '\0';
        }

        if (OutputParam.Format == OutputFormatIQ2 && (OutputParam.SampleFreq & 1))
            throw std::runtime_error("IQ2 format needs an even number of samples per ms");

        FinishSetup();
    }

    // ========================================================================
    // 2) Strukturalt setup
    // ========================================================================

    void SetupFromParams(const IfGenParams &p) {
        opt = IfGenOptions{};
        opt.writeBin       = p.writeBin;
        opt.writeTag       = p.writeTag;
        opt.multiThread    = p.multiThread;
        opt.nThreads       = p.nThreads;
        opt.verbose        = p.verbose;
        opt.validateOnly   = p.validateOnly;
        opt.actualizeRinex = p.actualizeRinex;
        opt.noiseSeed      = p.noiseSeed;
        opt.gain           = p.gain;
        opt.binFile        = p.output.binFile;

        SetOutputFile(stderr);

        // ---- ido ----
        UtcTime.Year = p.time.year;
        UtcTime.Month = p.time.month;
        UtcTime.Day = p.time.day;
        UtcTime.Hour = p.time.hour;
        UtcTime.Minute = p.time.minute;
        UtcTime.Second = p.time.second;

        // ---- trajektoria ----
        if (!p.waypoints.empty()) {
            // Waypoint-mod: a kezdopont az elso waypoint (FinishSetup allitja be).
            useWaypoints = true;
            Path.Build(p.waypoints, p.waypointInterp, p.waypointDefaultSpeedMps);
        } else {
            StartPos.lat = p.position.latitudeDeg * PI / 180.0;
            StartPos.lon = p.position.longitudeDeg * PI / 180.0;
            StartPos.alt = p.position.altitudeM;

            StartVel.speed = p.velocity.speedMps;
            StartVel.course = p.velocity.courseDeg * PI / 180.0;
            SpeedCourseToEnu(StartVel);

            Trajectory.SetInitPosVel(StartPos, StartVel, false);
            for (std::size_t i = 0; i < p.trajectory.size(); i++) {
                const auto &seg = p.trajectory[i];
                TrajectoryDataType dataType2 = TrajDataSpeed;
                TrajectoryType trajType = TrajTypeConstSpeed;
                double data2 = seg.speedMps;

                switch (seg.type) {
                case IfGenTrajectoryType::ConstSpeed:
                    // A SignalSim ConstSpeed szegmense a meglevo sebesseget tartja;
                    // a 2. parametert nem hasznalja.
                    trajType = TrajTypeConstSpeed;
                    dataType2 = TrajDataSpeed;
                    data2 = seg.speedMps;
                    break;

                case IfGenTrajectoryType::ConstAcc:
                    trajType = TrajTypeConstAcc;
                    dataType2 = TrajDataAcceleration;
                    data2 = seg.accelMps2;
                    break;

                case IfGenTrajectoryType::VerticalAcc:
                    trajType = TrajTypeVerticalAcc;
                    dataType2 = TrajDataAcceleration;
                    data2 = seg.accelMps2;
                    break;

                case IfGenTrajectoryType::Jerk:
                    trajType = TrajTypeJerk;
                    dataType2 = TrajDataAccRate;        // m/s^3, NEM kell radianba konvertalni
                    data2 = seg.jerkMps3;
                    break;

                case IfGenTrajectoryType::HorizontalTurn:
                    trajType = TrajTypeHorizontalCircular;
                    if (seg.radiusM != 0.0) {
                        dataType2 = TrajDataRadius;
                        data2 = seg.radiusM;
                    } else {
                        dataType2 = TrajDataAngularRate;
                        data2 = seg.turnRateDegS * PI / 180.0;
                    }
                    break;
                }

                int rc = Trajectory.AppendTrajectory(trajType, TrajDataTimeSpan, seg.durationS, dataType2, data2);
                if (rc != TRAJECTORY_NO_ERR)
                    throw std::runtime_error("trajectory[" + std::to_string(i) +
                                             "] rejected by SignalSim (error code " + std::to_string(rc) + ")");
            }
        }

        // ---- kimenet ----
        OutputParam = OUTPUT_PARAM{};
        OutputParam.Type       = OutputTypeIFdata;
        OutputParam.SampleFreq = (int)std::llround(p.output.sampleRateHz / 1000.0);
        OutputParam.CenterFreq = (int)std::llround(p.output.centerFreqHz / 1000.0);
        OutputParam.ElevationMask = p.output.elevationMaskDeg * PI / 180.0;
        switch (p.output.format) {
        case IfGenFormat::IQ2:  OutputParam.Format = OutputFormatIQ2;  break;
        case IfGenFormat::IQ4:  OutputParam.Format = OutputFormatIQ4;  break;
        case IfGenFormat::IQ16: OutputParam.Format = OutputFormatIQ16; break;
        default:                OutputParam.Format = OutputFormatIQ8; break;
        }
        {
            std::string bin = p.output.binFile.empty() ? std::string("output.bin") : p.output.binFile;
            std::strncpy(OutputParam.filename, bin.c_str(), 255);
            OutputParam.filename[255] = '\0';
        }

        for (const auto &sel : p.signals) {
            if (!sel.enable) continue;
            GnssSystem sys; int idx;
            if (!MapSignal(sel.signal, sys, idx)) {
                if (opt.verbose)
                    std::fprintf(stderr, "[IfGen] WARNING: unsupported signal skipped (GLO_G3?)\n");
                continue;
            }
            OutputParam.FreqSelect[sys] |= (1u << idx);
        }

        // ---- teljesitmeny ----
        PowerControl.NoiseFloor = p.power.noiseFloorDbm;
        PowerControl.InitCN0 = p.power.initCN0DbHz;
        PowerControl.Adjust =
            p.power.elevationAdjust ? ElevationAdjustSinSqrtFade : ElevationAdjustNone;

        // ---- ephemeris ----
        NavData.ReadNavFile(p.ephemerisFile.c_str(), "");

        if (opt.actualizeRinex)
            ActualizeRinex(UtcTime, &NavData, OutputParam);

        if (OutputParam.Format == OutputFormatIQ2 && (OutputParam.SampleFreq & 1))
            throw std::runtime_error("IQ2 format needs an even number of samples per ms");

        FinishSetup();
    }

    static bool MapSignal(IfGenSignal sig, GnssSystem &sys, int &idx) {
        switch (sig) {
        case IfGenSignal::GPS_L1CA: sys = GpsSystem; idx = SIGNAL_INDEX_L1CA; return true;
        case IfGenSignal::GPS_L1C:  sys = GpsSystem; idx = SIGNAL_INDEX_L1C;  return true;
        case IfGenSignal::GPS_L2C:  sys = GpsSystem; idx = SIGNAL_INDEX_L2C;  return true;
        case IfGenSignal::GPS_L2P:  sys = GpsSystem; idx = SIGNAL_INDEX_L2P;  return true;
        case IfGenSignal::GPS_L5:   sys = GpsSystem; idx = SIGNAL_INDEX_L5;   return true;
        case IfGenSignal::BDS_B1C:  sys = BdsSystem; idx = SIGNAL_INDEX_B1C;  return true;
        case IfGenSignal::BDS_B1I:  sys = BdsSystem; idx = SIGNAL_INDEX_B1I;  return true;
        case IfGenSignal::BDS_B2I:  sys = BdsSystem; idx = SIGNAL_INDEX_B2I;  return true;
        case IfGenSignal::BDS_B3I:  sys = BdsSystem; idx = SIGNAL_INDEX_B3I;  return true;
        case IfGenSignal::BDS_B2a:  sys = BdsSystem; idx = SIGNAL_INDEX_B2a;  return true;
        case IfGenSignal::BDS_B2b:  sys = BdsSystem; idx = SIGNAL_INDEX_B2b;  return true;
        case IfGenSignal::GAL_E1:   sys = GalileoSystem; idx = SIGNAL_INDEX_E1;  return true;
        case IfGenSignal::GAL_E5a:  sys = GalileoSystem; idx = SIGNAL_INDEX_E5a; return true;
        case IfGenSignal::GAL_E5b:  sys = GalileoSystem; idx = SIGNAL_INDEX_E5b; return true;
        case IfGenSignal::GAL_E5:   sys = GalileoSystem; idx = SIGNAL_INDEX_E5;  return true;
        case IfGenSignal::GAL_E6:   sys = GalileoSystem; idx = SIGNAL_INDEX_E6;  return true;
        case IfGenSignal::GLO_G1:   sys = GlonassSystem; idx = SIGNAL_INDEX_G1;  return true;
        case IfGenSignal::GLO_G2:   sys = GlonassSystem; idx = SIGNAL_INDEX_G2;  return true;
        case IfGenSignal::GLO_G3:   return false;   // nem tamogatott ebben a buildben
        }
        return false;
    }

    // ========================================================================
    // Kozos vegso lepesek mindket setup-uthoz
    // ========================================================================

    void FinishSetup() {
#ifdef _OPENMP
        nThreads = opt.multiThread ? (opt.nThreads > 0 ? opt.nThreads : omp_get_max_threads()) : 1;
#else
        nThreads = 1;
#endif
        blockIndex = 0;

        TrajReset();
        CurTime = UtcToGpsTime(UtcTime);
        GLONASS_TIME GlonassTime = UtcToGlonassTime(UtcTime);
        GNSS_TIME BdsTime = UtcToBdsTime(UtcTime);

        KINEMATIC_INFO CurPos;
        if (useWaypoints) {
            CurPos   = Path.At(0.0);
            StartPos = EcefToLla(CurPos);
        } else {
            CurPos = LlaToEcef(StartPos);
            SpeedLocalToEcef(StartPos, StartVel, CurPos);
        }

        if (opt.writeBin && !opt.validateOnly) {
            writer = std::make_unique<BinWriter>();
            if (!writer->Open(OutputParam.filename, OutputParam.Format, OutputParam.SampleFreq, opt.verbose))
                throw std::runtime_error(std::string("cannot open output file: ") + OutputParam.filename);
            if (opt.writeTag)
                WriteTagFile(std::string(OutputParam.filename) + ".tag", OutputParam);
        }

        int cn0 = (int)(PowerControl.InitCN0 * 100 + 0.5);
        for (auto &p : GpsSatParam) p.CN0 = cn0;
        for (auto &p : BdsSatParam) p.CN0 = cn0;
        for (auto &p : GalSatParam) p.CN0 = cn0;
        for (auto &p : GloSatParam) p.CN0 = cn0;

        for (int i = 0; i < NAVBIT_COUNT; i++) {
            switch (i) {
            case DataBitLNav:   NavBitArray[i] = new LNavBit; break;
            case DataBitCNav:   NavBitArray[i] = new CNavBit; break;
            case DataBitCNav2:  NavBitArray[i] = new CNav2Bit; break;
            case DataBitGNav:   NavBitArray[i] = new GNavBit; break;
            case DataBitD1D2:   NavBitArray[i] = new D1D2NavBit; break;
            case DataBitBCNav1: NavBitArray[i] = new BCNav1Bit; break;
            case DataBitBCNav2: NavBitArray[i] = new BCNav2Bit; break;
            case DataBitBCNav3: NavBitArray[i] = new BCNav3Bit; break;
            case DataBitINav:   NavBitArray[i] = new INavBit; break;
            case DataBitFNav:   NavBitArray[i] = new FNavBit; break;
            default:            NavBitArray[i] = (NavBit*)0; break;
            }
        }

        FilterSignalsToBand();

        NavBitArray[DataBitLNav]->SetIonoUtc(NavData.GetGpsIono(), NavData.GetGpsUtcParam());
        NavBitArray[DataBitCNav]->SetIonoUtc(NavData.GetGpsIono(), NavData.GetGpsUtcParam());
        NavBitArray[DataBitCNav2]->SetIonoUtc(NavData.GetGpsIono(), NavData.GetGpsUtcParam());
        NavBitArray[DataBitD1D2]->SetIonoUtc(NavData.GetBdsIono(), NavData.GetBdsUtcParam());
        NavBitArray[DataBitINav]->SetIonoUtc(NavData.GetGalileoIono(), NavData.GetGalileoUtcParam());
        NavBitArray[DataBitFNav]->SetIonoUtc(NavData.GetGalileoIono(), NavData.GetGalileoUtcParam());

        for (int i = 1; i <= TOTAL_GPS_SAT; i++) {
            GpsEph[i - 1] = NavData.FindEphemeris(GpsSystem, CurTime, i);
            NavBitArray[DataBitLNav]->SetEphemeris(i, GpsEph[i - 1]);
            NavBitArray[DataBitCNav]->SetEphemeris(i, GpsEph[i - 1]);
            NavBitArray[DataBitCNav2]->SetEphemeris(i, GpsEph[i - 1]);
        }
        for (int i = 1; i <= TOTAL_BDS_SAT; i++) {
            BdsEph[i - 1] = NavData.FindEphemeris(BdsSystem, BdsTime, i);
            NavBitArray[DataBitD1D2]->SetEphemeris(i, BdsEph[i - 1]);
            NavBitArray[DataBitBCNav1]->SetEphemeris(i, BdsEph[i - 1]);
            NavBitArray[DataBitBCNav2]->SetEphemeris(i, BdsEph[i - 1]);
            NavBitArray[DataBitBCNav3]->SetEphemeris(i, BdsEph[i - 1]);
        }
        for (int i = 1; i <= TOTAL_GAL_SAT; i++) {
            GalEph[i - 1] = NavData.FindEphemeris(GalileoSystem, CurTime, i);
            NavBitArray[DataBitINav]->SetEphemeris(i, GalEph[i - 1]);
            NavBitArray[DataBitFNav]->SetEphemeris(i, GalEph[i - 1]);
        }
        for (int i = 1; i <= TOTAL_GLO_SAT; i++) {
            GloEph[i - 1] = NavData.FindGloEphemeris(GlonassTime, i);
            NavBitArray[DataBitGNav]->SetEphemeris(i, (PGPS_EPHEMERIS)GloEph[i - 1]);
        }

        NavData.CompleteAlmanac(BdsSystem, UtcTime);
        NavData.CompleteAlmanac(GalileoSystem, UtcTime);
        NavBitArray[DataBitLNav]->SetAlmanac(NavData.GetGpsAlmanac());
        NavBitArray[DataBitCNav]->SetAlmanac(NavData.GetGpsAlmanac());
        NavBitArray[DataBitCNav2]->SetAlmanac(NavData.GetGpsAlmanac());
        NavBitArray[DataBitD1D2]->SetAlmanac(NavData.GetBdsAlmanac());
        NavBitArray[DataBitBCNav1]->SetAlmanac(NavData.GetBdsAlmanac());
        NavBitArray[DataBitBCNav2]->SetAlmanac(NavData.GetBdsAlmanac());
        NavBitArray[DataBitBCNav3]->SetAlmanac(NavData.GetBdsAlmanac());
        NavBitArray[DataBitINav]->SetAlmanac(NavData.GetGalileoAlmanac());
        NavBitArray[DataBitFNav]->SetAlmanac(NavData.GetGalileoAlmanac());
        NavBitArray[DataBitGNav]->SetAlmanac((PGPS_ALMANAC)NavData.GetGlonassAlmanac());

        GpsSatNumber = OutputParam.FreqSelect[GpsSystem] ? GetVisibleSatellite(CurPos, CurTime, OutputParam, GpsSystem, GpsEph, TOTAL_GPS_SAT, GpsEphVisible) : 0;
        BdsSatNumber = OutputParam.FreqSelect[BdsSystem] ? GetVisibleSatellite(CurPos, CurTime, OutputParam, BdsSystem, BdsEph, TOTAL_BDS_SAT, BdsEphVisible) : 0;
        GalSatNumber = OutputParam.FreqSelect[GalileoSystem] ? GetVisibleSatellite(CurPos, CurTime, OutputParam, GalileoSystem, GalEph, TOTAL_GAL_SAT, GalEphVisible) : 0;
        GloSatNumber = OutputParam.FreqSelect[GlonassSystem] ? GetGlonassVisibleSatellite(CurPos, GlonassTime, OutputParam, GloEph, TOTAL_GLO_SAT, GloEphVisible) : 0;

        IonoModel = std::make_unique<CIonoKlobuchar8>(NavData.GetGpsIono());
        for (int i = 0; i < TOTAL_GPS_SAT; i++) GpsSatParam[i].Initialize(GpsSystem, GpsEph[i], IonoModel.get(), PowerControl.InitCN0, PowerControl.Adjust);
        for (int i = 0; i < TOTAL_BDS_SAT; i++) BdsSatParam[i].Initialize(BdsSystem, BdsEph[i], IonoModel.get(), PowerControl.InitCN0, PowerControl.Adjust);
        for (int i = 0; i < TOTAL_GAL_SAT; i++) GalSatParam[i].Initialize(GalileoSystem, GalEph[i], IonoModel.get(), PowerControl.InitCN0, PowerControl.Adjust);
        for (int i = 0; i < TOTAL_GLO_SAT; i++) GloSatParam[i].Initialize(GlonassSystem, (PGPS_EPHEMERIS)GloEph[i], IonoModel.get(), PowerControl.InitCN0, PowerControl.Adjust);

        PSIGNAL_POWER PowerList = NULL;
        int ListCount = PowerControl.GetPowerControlList(0, PowerList);
        UpdateSatParamList(CurPos, ListCount, PowerList);

        AddChannels(GpsSystem, 0, SIGNAL_INDEX_L1CA, SIGNAL_INDEX_L5, GpsSatNumber,
            [&](int i) { return (int)GpsEphVisible[i]->svid; },
            [&](int, int) { return 0; },
            [&](int svid) { return &GpsSatParam[svid - 1]; });
        AddChannels(BdsSystem, 1, SIGNAL_INDEX_B1C, SIGNAL_INDEX_B2b, BdsSatNumber,
            [&](int i) { return (int)BdsEphVisible[i]->svid; },
            [&](int, int) { return 0; },
            [&](int svid) { return &BdsSatParam[svid - 1]; });
        AddChannels(GalileoSystem, 2, SIGNAL_INDEX_E1, SIGNAL_INDEX_E6, GalSatNumber,
            [&](int i) { return (int)GalEphVisible[i]->svid; },
            [&](int, int) { return 0; },
            [&](int svid) { return &GalSatParam[svid - 1]; });
        AddChannels(GlonassSystem, 3, SIGNAL_INDEX_G1, SIGNAL_INDEX_G2, GloSatNumber,
            [&](int i) { return (int)GloEphVisible[i]->n; },
            [&](int i, int sig) {
                return (sig == SIGNAL_INDEX_G1) ? GloEphVisible[i]->freq * 562500
                     : (sig == SIGNAL_INDEX_G2) ? GloEphVisible[i]->freq * 437500
                     : 0;
            },
            [&](int svid) { return &GloSatParam[svid - 1]; });

        info.sampleRateHz    = OutputParam.SampleFreq * 1000.0;
        info.centerFreqHz    = OutputParam.CenterFreq * 1000.0;
        info.samplesPerBlock = (std::size_t)OutputParam.SampleFreq;
        info.totalBlocks     = (std::size_t)std::llround(TrajDuration() * 1000.0);
        info.nChannels       = nChannels;
        info.outputFile      = OutputParam.filename;
        info.formatBits      = (OutputParam.Format == OutputFormatIQ2)  ? 2
                             : (OutputParam.Format == OutputFormatIQ4)  ? 4
                             : (OutputParam.Format == OutputFormatIQ16) ? 16 : 8;

        raw.resize(info.samplesPerBlock);
        blk.resize(info.samplesPerBlock);

        if (opt.verbose) {
            std::fprintf(stderr, "[IfGen] fs=%.4f MHz fc=%.4f MHz dur=%.2f s  SV: GPS %d BDS %d GAL %d GLO %d  channels: %d  threads: %d  trajectory: %s\n",
                         info.sampleRateHz / 1e6, info.centerFreqHz / 1e6, info.totalBlocks / 1000.0,
                         GpsSatNumber, BdsSatNumber, GalSatNumber, GloSatNumber, nChannels, nThreads,
                         useWaypoints ? "waypoints" : "segments");
        }
    }

    // ========================================================================
    void FilterSignalsToBand() {
        int lo = (OutputParam.CenterFreq - OutputParam.SampleFreq / 2) * 1000;
        int hi = (OutputParam.CenterFreq + OutputParam.SampleFreq / 2) * 1000;
        auto drop = [&](GnssSystem s, int sig, int freq) {
            if ((OutputParam.FreqSelect[s] & (1 << sig)) && (freq < lo || freq > hi))
                OutputParam.FreqSelect[s] &= ~(1 << sig);
        };
        drop(GpsSystem, SIGNAL_INDEX_L1CA, FREQ_GPS_L1);  drop(GpsSystem, SIGNAL_INDEX_L1C, FREQ_GPS_L1);
        drop(GpsSystem, SIGNAL_INDEX_L2C,  FREQ_GPS_L2);  drop(GpsSystem, SIGNAL_INDEX_L2P, FREQ_GPS_L2);
        drop(GpsSystem, SIGNAL_INDEX_L5,   FREQ_GPS_L5);
        drop(BdsSystem, SIGNAL_INDEX_B1C, FREQ_BDS_B1C);  drop(BdsSystem, SIGNAL_INDEX_B1I, FREQ_BDS_B1I);
        drop(BdsSystem, SIGNAL_INDEX_B2I, FREQ_BDS_B2I);  drop(BdsSystem, SIGNAL_INDEX_B3I, FREQ_BDS_B3I);
        drop(BdsSystem, SIGNAL_INDEX_B2a, FREQ_BDS_B2a);  drop(BdsSystem, SIGNAL_INDEX_B2b, FREQ_BDS_B2b);
        drop(GalileoSystem, SIGNAL_INDEX_E1,  FREQ_GAL_E1);  drop(GalileoSystem, SIGNAL_INDEX_E5a, FREQ_GAL_E5a);
        drop(GalileoSystem, SIGNAL_INDEX_E5b, FREQ_GAL_E5b); drop(GalileoSystem, SIGNAL_INDEX_E6,  FREQ_GAL_E6);
        drop(GlonassSystem, SIGNAL_INDEX_G1, FREQ_GLO_G1);
        drop(GlonassSystem, SIGNAL_INDEX_G2, FREQ_GLO_G2);
    }

    template <class IdFn, class OffFn, class ParamFn>
    void AddChannels(GnssSystem sys, int sysIdx, int sigFirst, int sigLast, int satNum,
                     IdFn id, OffFn off, ParamFn param) {
        for (int sig = sigFirst; sig <= sigLast; sig++) {
            if (!(OutputParam.FreqSelect[sys] & (1 << sig))) continue;
            int ifFreq = (int)(SignalCenterFreq[sysIdx][sig] - OutputParam.CenterFreq * 1000);
            for (int i = 0; i < satNum; i++) {
                if (nChannels >= TOTAL_SAT_CHANNEL) return;
                int svid = id(i);
                if (!opt.validateOnly) {
                    auto *c = new CSatIfSignal(OutputParam.SampleFreq, ifFreq + off(i, sig), sys, sig, svid);
                    c->InitState(CurTime, param(svid), GetNavData(sys, sig));
                    chan[nChannels] = c;
                }
                nChannels++;
            }
        }
    }

    NavBit *GetNavData(GnssSystem sys, int sig) {
        switch (sys) {
        case GpsSystem:
            switch (sig) {
            case SIGNAL_INDEX_L1CA: return NavBitArray[DataBitLNav];
            case SIGNAL_INDEX_L1C:  return NavBitArray[DataBitCNav2];
            case SIGNAL_INDEX_L2C:  return NavBitArray[DataBitCNav];
            case SIGNAL_INDEX_L2P:  return NavBitArray[DataBitLNav];
            case SIGNAL_INDEX_L5:   return NavBitArray[DataBitCNav];
            default:                return NavBitArray[DataBitLNav];
            }
        case BdsSystem:
            switch (sig) {
            case SIGNAL_INDEX_B1C: return NavBitArray[DataBitBCNav1];
            case SIGNAL_INDEX_B1I: case SIGNAL_INDEX_B2I: case SIGNAL_INDEX_B3I: return NavBitArray[DataBitD1D2];
            case SIGNAL_INDEX_B2a: return NavBitArray[DataBitBCNav2];
            case SIGNAL_INDEX_B2b: return NavBitArray[DataBitBCNav3];
            default:               return NavBitArray[DataBitD1D2];
            }
        case GalileoSystem:
            switch (sig) {
            case SIGNAL_INDEX_E1:  return NavBitArray[DataBitINav];
            case SIGNAL_INDEX_E5a: return NavBitArray[DataBitFNav];
            case SIGNAL_INDEX_E5b: return NavBitArray[DataBitINav];
            case SIGNAL_INDEX_E5:  return NavBitArray[DataBitFNav];
            case SIGNAL_INDEX_E6:  return NavBitArray[DataBitECNav];   // FIGYELEM: nullptr (nincs ECNavBit peldanyositva)
            default:               return NavBitArray[DataBitINav];
            }
        case GlonassSystem:
            switch (sig) {
            case SIGNAL_INDEX_G1:
            case SIGNAL_INDEX_G2: return NavBitArray[DataBitGNav];
            default:              return NavBitArray[DataBitINav];
            }
        default: return NavBitArray[DataBitLNav];
        }
    }

    void UpdateSatParamList(KINEMATIC_INFO &CurPos, int ListCount, PSIGNAL_POWER PowerList) {
        LLA_POSITION PosLLA = EcefToLla(CurPos);
        for (int i = 0; i < GpsSatNumber; i++) {
            if (!GpsEphVisible[i]) continue;
            int svid = GpsEphVisible[i]->svid;
            if (svid < 1 || svid > TOTAL_GPS_SAT) continue;
            int k = svid - 1;
            GpsSatParam[k].CalculateParam(CurPos, PosLLA, CurTime);
            GpsSatParam[k].UpdateCN0(ListCount, PowerList);
        }
        for (int i = 0; i < BdsSatNumber; i++) {
            if (!BdsEphVisible[i]) continue;
            int k = BdsEphVisible[i]->svid - 1;
            if (k < 0 || k >= TOTAL_BDS_SAT) continue;
            BdsSatParam[k].CalculateParam(CurPos, PosLLA, CurTime);
            BdsSatParam[k].UpdateCN0(ListCount, PowerList);
        }
        for (int i = 0; i < GalSatNumber; i++) {
            if (!GalEphVisible[i]) continue;
            int k = GalEphVisible[i]->svid - 1;
            if (k < 0 || k >= TOTAL_GAL_SAT) continue;
            GalSatParam[k].CalculateParam(CurPos, PosLLA, CurTime);
            GalSatParam[k].UpdateCN0(ListCount, PowerList);
        }
        for (int i = 0; i < GloSatNumber; i++) {
            if (!GloEphVisible[i]) continue;
            int k = GloEphVisible[i]->n - 1;
            if (k < 0 || k >= TOTAL_GLO_SAT) continue;
            GloSatParam[k].CalculateParam(CurPos, PosLLA, CurTime);
            GloSatParam[k].UpdateCN0(ListCount, PowerList);
        }
    }

    bool StepToNextMs() {
        KINEMATIC_INFO CurPos;
        PSIGNAL_POWER PowerList = NULL;
        if (!TrajNext(0.001, CurPos)) return false;
        int ListCount = PowerControl.GetPowerControlList(1, PowerList);
        if (++CurTime.MilliSeconds > 604800000) { CurTime.Week++; CurTime.MilliSeconds -= 604800000; }
        UpdateSatParamList(CurPos, ListCount, PowerList);
        return true;
    }

    bool Refill() {
        if (finished || opt.validateOnly) return false;
        if (!StepToNextMs()) { finished = true; return false; }

        const int N = (int)info.samplesPerBlock;

        {
            const int CH = 1024;
            const int nCh = (N + CH - 1) / CH;
            const std::uint64_t base = opt.noiseSeed * 0x9E3779B97F4A7C15ULL + blockIndex * 0xD1B54A32D192ED03ULL;
#ifdef _OPENMP
            #pragma omp parallel for num_threads(nThreads)
#endif
            for (int c = 0; c < nCh; c++) {
                SplitMix64 g{ base + (std::uint64_t)c * 0x94D049BB133111EBULL };
                const int j1 = (c + 1) * CH < N ? (c + 1) * CH : N;
                for (int j = c * CH; j < j1; j++) raw[j] = PolarNoise(g);
            }
        }
        blockIndex++;

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic) num_threads(nThreads)
#endif
        for (int c = 0; c < nChannels; c++) chan[c]->GetIfSample(CurTime);

#ifdef _OPENMP
        #pragma omp parallel for num_threads(nThreads)
#endif
        for (int j = 0; j < N; j++) {
            complex_number acc = raw[j];
            for (int c = 0; c < nChannels; c++) acc += chan[c]->SampleArray[j];
            raw[j] = acc;
        }

        if (writer) writer->Write(raw.data(), N);

        const float g = (float)opt.gain;
        for (int j = 0; j < N; j++)
            blk[j] = std::complex<float>(float(raw[j].real) * g, float(raw[j].imag) * g);
        len = (std::size_t)N;
        pos = 0;
        return true;
    }
};

// ============================================================================
// Publikus API
// ============================================================================

IfGenEngine::IfGenEngine() = default;
IfGenEngine::~IfGenEngine() { Close(); }

void IfGenEngine::Init(const IfGenOptions &opt) {
    Close();
    impl = std::make_unique<Impl>();
    try { impl->Setup(opt); }
    catch (...) { impl.reset(); throw; }
}

void IfGenEngine::Init(const IfGenParams &params) {
    std::vector<std::string> errors;
    if (!ValidateIfGenParams(params, errors)) {
        std::string msg = "invalid IfGenParams:";
        for (auto &e : errors) msg += "\n  - " + e;
        throw std::runtime_error(msg);
    }
    Close();
    impl = std::make_unique<Impl>();
    try { impl->SetupFromParams(params); }
    catch (...) { impl.reset(); throw; }
}

const IfGenInfo& IfGenEngine::Info() const {
    if (!impl) throw std::runtime_error("IfGenEngine: not initialized");
    return impl->info;
}

bool IfGenEngine::NextBlock(const std::complex<float> *&samples, std::size_t &n) {
    if (!impl) throw std::runtime_error("IfGenEngine: not initialized");
    if (impl->pos < impl->len) {
        samples = impl->blk.data() + impl->pos;
        n = impl->len - impl->pos;
        impl->pos = impl->len;
        return true;
    }
    if (!impl->Refill()) return false;
    samples = impl->blk.data();
    n = impl->len;
    impl->pos = impl->len;
    return true;
}

std::size_t IfGenEngine::Read(std::complex<float> *out, std::size_t n) {
    if (!impl) throw std::runtime_error("IfGenEngine: not initialized");
    std::size_t done = 0;
    while (done < n) {
        if (impl->pos == impl->len && !impl->Refill()) break;
        std::size_t c = std::min(n - done, impl->len - impl->pos);
        std::copy_n(impl->blk.begin() + impl->pos, c, out + done);
        impl->pos += c;
        done += c;
    }
    return done;
}

std::size_t IfGenEngine::Stream(const SampleCallback &cb, std::size_t chunk) {
    if (!impl) throw std::runtime_error("IfGenEngine: not initialized");
    std::size_t total = 0;
    if (chunk == 0) {
        const std::complex<float> *p; std::size_t n;
        while (NextBlock(p, n)) {
            total += n;
            if (!cb(p, n)) break;
        }
        return total;
    }
    std::vector<std::complex<float>> buf(chunk);
    for (;;) {
        std::size_t n = Read(buf.data(), chunk);
        if (n == 0) break;
        total += n;
        if (!cb(buf.data(), n)) break;
        if (n < chunk) break;
    }
    return total;
}

void IfGenEngine::Close() {
    impl.reset();
}