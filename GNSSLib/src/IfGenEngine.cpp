// Sisi/GNSSLib/src/IfGenEngine.cpp
// IfGenEngine.cpp -- az IFDataGen main()-jenek es globaljainak konyvtari valtozata.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "SignalSim.h"
#include "ActualizeRinex.h"
#include "gnss/IfGenEngine.h"

#define TOTAL_GPS_SAT 32
#define TOTAL_BDS_SAT 63
#define TOTAL_GAL_SAT 36
#define TOTAL_GLO_SAT 24
#define TOTAL_SAT_CHANNEL 128
#define NAVBIT_COUNT 12

namespace {

enum DataBitType {
    DataBitLNav, DataBitCNav, DataBitCNav2,                      // GPS
    DataBitGNav, DataBitGNav2,                                   // GLONASS
    DataBitD1D2, DataBitBCNav1, DataBitBCNav2, DataBitBCNav3,    // BDS
    DataBitINav, DataBitFNav, DataBitECNav,                      // Galileo
    DataBitSbas,
};

const int SignalCenterFreq[][8] = {
    { FREQ_GPS_L1, FREQ_GPS_L1, FREQ_GPS_L2, FREQ_GPS_L2, FREQ_GPS_L5 },
    { FREQ_BDS_B1C, FREQ_BDS_B1I, FREQ_BDS_B2I, FREQ_BDS_B3I, FREQ_BDS_B2a, FREQ_BDS_B2b, FREQ_BDS_B2ab },
    { FREQ_GAL_E1, FREQ_GAL_E5a, FREQ_GAL_E5b, FREQ_GAL_E5, FREQ_GAL_E6 },
    { FREQ_GLO_G1, FREQ_GLO_G2 },
};

// ------------------------------------------------------------------------------------------
// Kvantalok a .bin fajlhoz (a regi main-bol, javitva)
// ------------------------------------------------------------------------------------------

// PocketSDR kompatibilis 2 bites. Javitva: a 2. minta |=-vel kerul a bajtba, es a bajt tenylegesen kiirodik.
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

// Javitva: int a kozbenso ertek (unsigned char-nal a >7 vizsgalat tulcsordulas miatt nem mukodott).
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

// Javitva: a minden ms-ban kiirt printf("gain...") torolve.
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

// .bin iro: kvantalas + a regi AGC logika. A stream skalazastol fuggetlen.
class BinWriter {
public:
    bool Open(const char *path, int format, int samplesPerMs, bool verb) {
        f = std::fopen(path, "wb");
        fmt = format; n = samplesPerMs; verbose = verb;
        q.assign((size_t)n * 4, 0);
        return f != nullptr;
    }
    void Write(const complex_number *s, int len) {
        int clip; size_t bytes;
        switch (fmt) {
        case OutputFormatIQ2:  clip = Quant2 (s, len, q.data(), agc); bytes = len / 2; break;
        case OutputFormatIQ4:  clip = Quant4 (s, len, q.data(), agc); bytes = len;     break;
        case OutputFormatIQ16: clip = Quant16(s, len, q.data(), agc); bytes = (size_t)len * 4; break;
        default:               clip = Quant8 (s, len, q.data(), agc); bytes = (size_t)len * 2; break;
        }
        std::fwrite(q.data(), 1, bytes, f);
        clipped += clip; total += (long long)len * 2; allClipped += clip; allTotal += (long long)len * 2;
        if (++cycle % 100 == 0) {                        // AGC 100 ms-onkent
            double r = (double)clipped / total;
            if (r > 0.01)                    { agc *= 0.95; clipped = total = 0; }
            else if (r < 0.001 && agc < 1.0) { agc = std::min(1.0, agc * 1.02); clipped = total = 0; }
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
    gmtime_r(&tt, &tmv);                                 // szalbiztos gmtime
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

} // namespace

namespace {

// Kicsi, gyors RNG a zajhoz. (seed, blokk, chunk) alapjan inicializalva -> a kimenet
// a szalszamtol fuggetlenul reprodukalhato, es nincs megosztott allapot.
struct SplitMix64 {
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uni() { return double(next() >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0; }  // [-1, 1)
};

// Marsaglia polar, sigma = 1 komponensenkent
complex_number PolarNoise(SplitMix64 &g) {
    double a, b, m;
    do { a = g.uni(); b = g.uni(); m = a * a + b * b; } while (m >= 1.0 || m == 0.0);
    m = std::sqrt(-2.0 * std::log(m) / m);
    return complex_number(a * m, b * m);
}

} // namespace

// ==========================================================================================
// Impl: a regi globalok + main() lokalis valtozoi
// ==========================================================================================
struct IfGenEngine::Impl {
    IfGenOptions opt;
    IfGenInfo info;

    // regi globalok
    CTrajectory Trajectory;
    CPowerControl PowerControl;
    CNavData NavData;
    OUTPUT_PARAM OutputParam;
    GNSS_TIME CurTime;
    PGPS_EPHEMERIS GpsEph[TOTAL_GPS_SAT] = {}, GpsEphVisible[TOTAL_GPS_SAT] = {};
    PGPS_EPHEMERIS BdsEph[TOTAL_BDS_SAT] = {}, BdsEphVisible[TOTAL_BDS_SAT] = {};
    PGPS_EPHEMERIS GalEph[TOTAL_GAL_SAT] = {}, GalEphVisible[TOTAL_GAL_SAT] = {};
    PGLONASS_EPHEMERIS GloEph[TOTAL_GLO_SAT] = {}, GloEphVisible[TOTAL_GLO_SAT] = {};
    CSatelliteParam GpsSatParam[TOTAL_GPS_SAT], BdsSatParam[TOTAL_BDS_SAT],
                    GalSatParam[TOTAL_GAL_SAT], GloSatParam[TOTAL_GLO_SAT];
    int GpsSatNumber = 0, BdsSatNumber = 0, GalSatNumber = 0, GloSatNumber = 0;

    // regi main() lokalisai
    JsonStream JsonTree;                                 // eletben marad, mint a mainben
    NavBit *NavBitArray[NAVBIT_COUNT] = {};
    CSatIfSignal *chan[TOTAL_SAT_CHANNEL] = {};
    int nChannels = 0;

    // futasi allapot
    std::vector<complex_number> raw;                     // zaj + osszes muhold, 1 ms, nyers (sigma=1)
    std::vector<std::complex<float>> blk;                // ugyanez float-ra konvertalva, gain-nel
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

    // -------------------------------------------------------------------- setup
    void Setup(const IfGenOptions &o) {
        opt = o;
        SetOutputFile(stderr);       // a SignalSim sajat uzenetei ne szennyezzek az stdout-ot

        if (JsonTree.ReadFile(opt.configFile.c_str()) != 0)
            throw std::runtime_error("unable to read JSON file: " + opt.configFile);
        SetJsonFilePath(opt.configFile.c_str());
        JsonObject *Object = JsonTree.GetRootObject();

        UTC_TIME UtcTime;
        LLA_POSITION StartPos;
        LOCAL_SPEED StartVel;
        if (!AssignParameters(Object, &UtcTime, &StartPos, &StartVel, &Trajectory, &NavData, &OutputParam, &PowerControl, NULL))
            std::fprintf(stderr, "[IfGen] WARNING: AssignParameters reported problems in '%s'\n", opt.configFile.c_str());
        if (opt.actualizeRinex) ActualizeRinex(UtcTime, &NavData, OutputParam);

        if (opt.writeBin && !opt.binFile.empty()) {
            std::strncpy(OutputParam.filename, opt.binFile.c_str(), 255);
            OutputParam.filename[255] = '\0';
        }
        if (OutputParam.Format == OutputFormatIQ2 && (OutputParam.SampleFreq & 1))
            throw std::runtime_error("IQ2 format needs an even number of samples per ms");

        // szalak
#ifdef _OPENMP
        nThreads = opt.multiThread ? (opt.nThreads > 0 ? opt.nThreads : omp_get_max_threads()) : 1;
#else
        nThreads = 1;
#endif
        blockIndex = 0;

        // kezdo idok / pozicio
        Trajectory.ResetTrajectoryTime();
        CurTime = UtcToGpsTime(UtcTime);
        GLONASS_TIME GlonassTime = UtcToGlonassTime(UtcTime);
        GNSS_TIME BdsTime = UtcToBdsTime(UtcTime);
        KINEMATIC_INFO CurPos = LlaToEcef(StartPos);
        SpeedLocalToEcef(StartPos, StartVel, CurPos);

        // kimeneti fajl (opcionalis)
        if (opt.writeBin && !opt.validateOnly) {
            writer = std::make_unique<BinWriter>();
            if (!writer->Open(OutputParam.filename, OutputParam.Format, OutputParam.SampleFreq, opt.verbose))
                throw std::runtime_error(std::string("cannot open output file: ") + OutputParam.filename);
            if (opt.writeTag) WriteTagFile(std::string(OutputParam.filename) + ".tag", OutputParam);
        }

        // CN0 kezdoertek
        int cn0 = (int)(PowerControl.InitCN0 * 100 + 0.5);
        for (auto &p : GpsSatParam) p.CN0 = cn0;
        for (auto &p : BdsSatParam) p.CN0 = cn0;
        for (auto &p : GalSatParam) p.CN0 = cn0;
        for (auto &p : GloSatParam) p.CN0 = cn0;

        // nav bit peldanyok
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

        // iono/UTC, ephemeris, almanac
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

        // lathato muholdak
        GpsSatNumber = OutputParam.FreqSelect[GpsSystem] ? GetVisibleSatellite(CurPos, CurTime, OutputParam, GpsSystem, GpsEph, TOTAL_GPS_SAT, GpsEphVisible) : 0;
        BdsSatNumber = OutputParam.FreqSelect[BdsSystem] ? GetVisibleSatellite(CurPos, CurTime, OutputParam, BdsSystem, BdsEph, TOTAL_BDS_SAT, BdsEphVisible) : 0;
        GalSatNumber = OutputParam.FreqSelect[GalileoSystem] ? GetVisibleSatellite(CurPos, CurTime, OutputParam, GalileoSystem, GalEph, TOTAL_GAL_SAT, GalEphVisible) : 0;
        GloSatNumber = OutputParam.FreqSelect[GlonassSystem] ? GetGlonassVisibleSatellite(CurPos, GlonassTime, OutputParam, GloEph, TOTAL_GLO_SAT, GloEphVisible) : 0;

        CIonoKlobuchar8 IonoModel(NavData.GetGpsIono());
        for (int i = 0; i < TOTAL_GPS_SAT; i++) GpsSatParam[i].Initialize(GpsSystem, GpsEph[i], &IonoModel, PowerControl.InitCN0, PowerControl.Adjust);
        for (int i = 0; i < TOTAL_BDS_SAT; i++) BdsSatParam[i].Initialize(BdsSystem, BdsEph[i], &IonoModel, PowerControl.InitCN0, PowerControl.Adjust);
        for (int i = 0; i < TOTAL_GAL_SAT; i++) GalSatParam[i].Initialize(GalileoSystem, GalEph[i], &IonoModel, PowerControl.InitCN0, PowerControl.Adjust);
        for (int i = 0; i < TOTAL_GLO_SAT; i++) GloSatParam[i].Initialize(GlonassSystem, (PGPS_EPHEMERIS)GloEph[i], &IonoModel, PowerControl.InitCN0, PowerControl.Adjust);

        PSIGNAL_POWER PowerList = NULL;
        int ListCount = PowerControl.GetPowerControlList(0, PowerList);
        UpdateSatParamList(CurPos, ListCount, PowerList);

        // csatornak
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
                     : (sig == SIGNAL_INDEX_G2) ? GloEphVisible[i]->freq * 437500 : 0; },
            [&](int svid) { return &GloSatParam[svid - 1]; });

        // info
        info.sampleRateHz    = OutputParam.SampleFreq * 1000.0;
        info.centerFreqHz    = OutputParam.CenterFreq * 1000.0;
        info.samplesPerBlock = (std::size_t)OutputParam.SampleFreq;
        info.totalBlocks     = (std::size_t)(Trajectory.GetTimeLength() * 1000);
        info.nChannels       = nChannels;
        info.outputFile      = OutputParam.filename;
        info.formatBits      = (OutputParam.Format == OutputFormatIQ2)  ? 2
                             : (OutputParam.Format == OutputFormatIQ4)  ? 4
                             : (OutputParam.Format == OutputFormatIQ16) ? 16 : 8;

        raw.resize(info.samplesPerBlock);
        blk.resize(info.samplesPerBlock);

        if (opt.verbose) {
            std::fprintf(stderr, "[IfGen] fs=%.4f MHz fc=%.4f MHz dur=%.2f s  SV: GPS %d BDS %d GAL %d GLO %d  channels: %d  threads: %d\n",
                         info.sampleRateHz / 1e6, info.centerFreqHz / 1e6, info.totalBlocks / 1000.0,
                         GpsSatNumber, BdsSatNumber, GalSatNumber, GloSatNumber, nChannels, nThreads);
        }
    }

    // csak azok a jelek maradnak, amik a [fc-fs/2, fc+fs/2] savba esnek
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
        drop(GlonassSystem, SIGNAL_INDEX_G1, FREQ_GLO_G1);   drop(GlonassSystem, SIGNAL_INDEX_G2, FREQ_GLO_G2);
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
            case SIGNAL_INDEX_E6:  return NavBitArray[DataBitECNav];
            default:               return NavBitArray[DataBitINav];
            }
        case GlonassSystem:
            switch (sig) {
            case SIGNAL_INDEX_G1: case SIGNAL_INDEX_G2: return NavBitArray[DataBitGNav];
            default:                                    return NavBitArray[DataBitINav];
            }
        default: return NavBitArray[DataBitLNav];
        }
    }

    void UpdateSatParamList(KINEMATIC_INFO &CurPos, int ListCount, PSIGNAL_POWER PowerList) {
        LLA_POSITION PosLLA = EcefToLla(CurPos);
        for (int i = 0; i < GpsSatNumber; i++) { int k = GpsEphVisible[i]->svid - 1; GpsSatParam[k].CalculateParam(CurPos, PosLLA, CurTime); GpsSatParam[k].UpdateCN0(ListCount, PowerList); }
        for (int i = 0; i < BdsSatNumber; i++) { int k = BdsEphVisible[i]->svid - 1; BdsSatParam[k].CalculateParam(CurPos, PosLLA, CurTime); BdsSatParam[k].UpdateCN0(ListCount, PowerList); }
        for (int i = 0; i < GalSatNumber; i++) { int k = GalEphVisible[i]->svid - 1; GalSatParam[k].CalculateParam(CurPos, PosLLA, CurTime); GalSatParam[k].UpdateCN0(ListCount, PowerList); }
        for (int i = 0; i < GloSatNumber; i++) { int k = GloEphVisible[i]->n - 1;    GloSatParam[k].CalculateParam(CurPos, PosLLA, CurTime); GloSatParam[k].UpdateCN0(ListCount, PowerList); }
    }

    // -------------------------------------------------------------------- futas
    bool StepToNextMs() {
        KINEMATIC_INFO CurPos;
        PSIGNAL_POWER PowerList = NULL;
        if (!Trajectory.GetNextPosVelECEF(0.001, CurPos)) return false;
        int ListCount = PowerControl.GetPowerControlList(1, PowerList);
        if (++CurTime.MilliSeconds > 604800000) { CurTime.Week++; CurTime.MilliSeconds -= 604800000; }
        UpdateSatParamList(CurPos, ListCount, PowerList);
        return true;
    }

    // Egy 1 ms-os blokk elokeszitese: zaj + osszes csatorna, .bin iras, konvertalas float-ra.
    bool Refill() {
        if (finished || opt.validateOnly) return false;
        if (!StepToNextMs()) { finished = true; return false; }

        const int N = (int)info.samplesPerBlock;
        {   // zaj: parhuzamos, chunkonkent (seed, blokk, chunk) alapu RNG-vel
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

        // mintaindex a kulso ciklus -> race-mentes, OpenMP-vel is
#ifdef _OPENMP
        #pragma omp parallel for num_threads(nThreads)
#endif
        for (int j = 0; j < N; j++) {
            complex_number acc = raw[j];
            for (int c = 0; c < nChannels; c++) acc += chan[c]->SampleArray[j];
            raw[j] = acc;
        }

        if (writer) writer->Write(raw.data(), N);       // tiszta GNSS + zaj, sajat AGC-vel

        const float g = opt.gain;
        for (int j = 0; j < N; j++) blk[j] = std::complex<float>(float(raw[j].real) * g, float(raw[j].imag) * g);
        len = (std::size_t)N;
        pos = 0;
        return true;
    }
};

// ==========================================================================================
// Publikus API
// ==========================================================================================
IfGenEngine::IfGenEngine() = default;
IfGenEngine::~IfGenEngine() { Close(); }

void IfGenEngine::Init(const IfGenOptions &opt) {
    Close();
    impl = std::make_unique<Impl>();
    try {
        impl->Setup(opt);
    } catch (...) {
        impl.reset();
        throw;
    }
}

const IfGenInfo& IfGenEngine::Info() const {
    if (!impl) throw std::runtime_error("IfGenEngine: not initialized");
    return impl->info;
}

bool IfGenEngine::NextBlock(const std::complex<float> *&samples, std::size_t &n) {
    if (!impl) throw std::runtime_error("IfGenEngine: not initialized");
    if (impl->pos < impl->len) {                         // Read() altal felig olvasott blokk maradeka
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
    } else {
        std::vector<std::complex<float>> buf(chunk);
        for (;;) {
            std::size_t n = Read(buf.data(), chunk);
            if (n == 0) break;
            total += n;
            if (!cb(buf.data(), n) || n < chunk) break;
        }
    }
    return total;
}

void IfGenEngine::Close() {
    impl.reset();                                        // ~Impl: .bin flush + lezaras, csatornak torlese
}