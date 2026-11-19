#include <gnss/gnsslib.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <deque>

namespace fs = std::filesystem;
using json = nlohmann::json;
namespace {
std::atomic<bool> g_stop{false};
void OnSigint(int) { g_stop = true; }
using Block = std::vector<std::complex<float>>;

class BlockQueue {
public:
    explicit BlockQueue(std::size_t capacity) : capacity_(capacity) {}
    bool push(Block&& b) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [&] { return queue_.size() < capacity_ || closed_; });
        if (closed_) return false;
        queue_.push_back(std::move(b));
        notEmpty_.notify_one();
        return true;
    }
    bool pop(Block& out, bool& starved) {
        std::unique_lock<std::mutex> lock(mutex_);
        starved = queue_.empty() && !closed_;
        notEmpty_.wait(lock, [&] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return true;
    }
    void waitFilled(std::size_t n) {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [&] { return queue_.size() >= n || closed_; });
    }
    void close() {
        { std::lock_guard<std::mutex> lock(mutex_); closed_ = true; }
        notFull_.notify_all();
        notEmpty_.notify_all();
    }
private:
    std::mutex mutex_;
    std::condition_variable notFull_, notEmpty_;
    std::deque<Block> queue_;
    std::size_t capacity_;
    bool closed_ = false;
};

struct Options {
    std::string configFile;
    std::string uhdArgs;
    std::string ref;
    double txGain = 0.0;
    float scale = 0.25f;
    double benchSeconds = 3.0;
    bool benchOnly = false;
    bool noBench = false;
};

bool ParseOptions(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](std::string& dst) {
            if (i + 1 >= argc) { std::cerr << a << " requires a value\n"; return false; }
            dst = argv[++i]; return true;
        };
        std::string v;
        if (a == "--config") { if (!next(o.configFile)) return false; }
        else if (a == "--args") { if (!next(o.uhdArgs)) return false; }
        else if (a == "--ref") { if (!next(o.ref)) return false; }
        else if (a == "--txgain") { if (!next(v)) return false; o.txGain = std::stod(v); }
        else if (a == "--scale") { if (!next(v)) return false; o.scale = std::stof(v); }
        else if (a == "--bench-seconds") { if (!next(v)) return false; o.benchSeconds = std::stod(v); }
        else if (a == "--bench-only") o.benchOnly = true;
        else if (a == "--no-bench") o.noBench = true;
        else {
            std::cerr << "Unknown option: " << a << "\n"
                      << "Usage: " << argv[0] << " --config scenario.json [--args \"serial=...\"]"
                      << " [--txgain dB] [--scale value] [--ref internal|external|gpsdo]"
                      << " [--bench-only|--no-bench]\n";
            return false;
        }
    }
    if (o.configFile.empty()) { std::cerr << "Missing required --config JSON file\n"; return false; }
    return true;
}

// SignalSim-style JSON stores frequencies in MHz (e.g. 1575.42) and sample
// rate in MS/s (e.g. 4.0). Values already expressed in Hz are also accepted.
double ToHz(double value, const char* field) {
    if (!std::isfinite(value) || value <= 0.0)
        throw std::runtime_error(std::string("Invalid or non-positive JSON field: ") + field);
    return value < 100000.0 ? value * 1.0e6 : value;
}

struct JsonConfigInfo {
    double sampleRateHz = 0.0;
    double centerFreqHz = 0.0;
    double routeDurationS = 0.0;
    fs::path normalizedPath;
};

JsonConfigInfo PrepareJson(const fs::path& inputPath) {
    std::ifstream in(inputPath);
    if (!in) throw std::runtime_error("Cannot open JSON config: " + inputPath.string());
    json root;
    in >> root;
    if (!root.is_object()) throw std::runtime_error("The JSON root must be an object");
    if (!root.contains("trajectory") || !root["trajectory"].is_object())
        throw std::runtime_error("JSON must contain an object named 'trajectory'");
    if (!root.contains("output") || !root["output"].is_object())
        throw std::runtime_error("JSON must contain an object named 'output'");

    auto& trajectory = root["trajectory"];
    // GNSSLib's internal JSON reader currently expects trajectory.waypointList.
    // Accept the user's trajectory.waypoints spelling and normalize it here.
    if (!trajectory.contains("waypointList")) {
        if (trajectory.contains("waypoints")) {
            trajectory["waypointList"] = trajectory["waypoints"];
            trajectory.erase("waypoints");
        } else {
            throw std::runtime_error("trajectory must contain 'waypoints' or 'waypointList'");
        }
    }
    // Waypoint mode and segment mode must not compete; the waypoint path wins.
    trajectory.erase("trajectoryList");
    if (!trajectory["waypointList"].is_array() || trajectory["waypointList"].empty())
        throw std::runtime_error("trajectory.waypointList must be a non-empty array");

    double lastTime = -1.0;
    for (std::size_t i = 0; i < trajectory["waypointList"].size(); ++i) {
        auto& wp = trajectory["waypointList"][i];
        for (const char* key : {"latitude", "longitude", "altitude", "time"}) {
            if (!wp.contains(key) || !wp[key].is_number())
                throw std::runtime_error("Each waypoint needs numeric latitude, longitude, altitude, and time fields");
        }
        const double lat = wp["latitude"].get<double>();
        const double lon = wp["longitude"].get<double>();
        const double t = wp["time"].get<double>();
        if (lat < -90 || lat > 90 || lon < -180 || lon > 180 || t < 0 || t <= lastTime)
            throw std::runtime_error("Waypoint coordinates are out of range or times are not strictly increasing");
        lastTime = t;
    }
    if (lastTime <= 0.0) throw std::runtime_error("The final waypoint time must be greater than zero");

    const auto& output = root["output"];
    if (!output.contains("sampleFreq") || !output["sampleFreq"].is_number())
        throw std::runtime_error("output.sampleFreq is missing or not numeric");
    if (!output.contains("centerFreq") || !output["centerFreq"].is_number())
        throw std::runtime_error("output.centerFreq is missing or not numeric");

    JsonConfigInfo info;
    info.sampleRateHz = ToHz(output["sampleFreq"].get<double>(), "output.sampleFreq");
    info.centerFreqHz = ToHz(output["centerFreq"].get<double>(), "output.centerFreq");
    info.routeDurationS = lastTime;

    // Keep the normalized file next to the source JSON so any relative RINEX
    // path continues to resolve from the same directory.
    fs::path absolute = fs::absolute(inputPath);
    info.normalizedPath = absolute.parent_path() / (absolute.stem().string() + ".b210_normalized.json");
    std::ofstream out(info.normalizedPath);
    if (!out) throw std::runtime_error("Cannot write normalized JSON: " + info.normalizedPath.string());
    out << root.dump(2) << '\n';
    return info;
}

double MeasureRealtimeFactor(gnss::GNSSGenerator& generator, double fs, double seconds) {
    const std::size_t chunk = std::max<std::size_t>(1, static_cast<std::size_t>(fs / 100.0));
    const std::size_t want = static_cast<std::size_t>(fs * seconds);
    Block buffer(chunk);
    std::size_t got = 0;
    const auto start = std::chrono::steady_clock::now();
    while (got < want) {
        const std::size_t n = generator.read(buffer.data(), std::min(chunk, want - got));
        if (n == 0) break;
        got += n;
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return elapsed > 0.0 ? (static_cast<double>(got) / fs) / elapsed : 0.0;
}
} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!ParseOptions(argc, argv, opt)) return 2;
    std::signal(SIGINT, OnSigint);

    JsonConfigInfo jsonInfo;
    try {
        jsonInfo = PrepareJson(opt.configFile);
    } catch (const std::exception& e) {
        std::cerr << "JSON config error: " << e.what() << '\n';
        return 2;
    }

    std::cout << "JSON: " << fs::absolute(opt.configFile) << '\n'
              << "Sample rate: " << jsonInfo.sampleRateHz / 1.0e6 << " MS/s\n"
              << "Center frequency: " << jsonInfo.centerFreqHz / 1.0e6 << " MHz\n"
              << "Waypoint duration: " << jsonInfo.routeDurationS << " s\n";

    gnss::GNSSGenerator generator;
    if (!generator.initializeFromJSON(jsonInfo.normalizedPath.string())) {
        std::cerr << "initializeFromJSON() failed\n";
        std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
        return 1;
    }
    if (!generator.openStream(false, "", opt.scale)) {
        std::cerr << "openStream() failed\n";
        std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
        return 1;
    }

    if (!opt.noBench) {
        std::cout << "Benchmark: " << opt.benchSeconds << " s of generated signal...\n";
        const double rtf = MeasureRealtimeFactor(generator, jsonInfo.sampleRateHz, opt.benchSeconds);
        std::cout << "Real-time factor: " << rtf << "x (>= 1.2 recommended)\n";
        if (opt.benchOnly) {
            generator.closeStream();
            std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
            return 0;
        }
        if (rtf < 1.0) {
            std::cerr << "Generator did not keep up with real time. Reduce sample rate/signals or use --no-bench to override.\n";
            generator.closeStream();
            std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
            return 1;
        }
        generator.closeStream();
        if (!generator.openStream(false, "", opt.scale)) {
            std::cerr << "openStream() failed when restarting after benchmark\n";
            std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
            return 1;
        }
    } else if (opt.benchOnly) {
        std::cout << "--bench-only has no effect together with --no-bench\n";
        generator.closeStream();
        std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
        return 0;
    }

    try {
        auto usrp = uhd::usrp::multi_usrp::make(opt.uhdArgs);
        if (!opt.ref.empty()) {
            usrp->set_clock_source(opt.ref);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        usrp->set_tx_rate(jsonInfo.sampleRateHz);
        usrp->set_tx_freq(jsonInfo.centerFreqHz);
        usrp->set_tx_gain(opt.txGain);
        const double actualRate = usrp->get_tx_rate();
        if (std::fabs(actualRate - jsonInfo.sampleRateHz) > std::max(1.0, jsonInfo.sampleRateHz * 1e-6)) {
            std::cerr << "Requested TX sample rate " << jsonInfo.sampleRateHz
                      << " Hz but UHD set " << actualRate << " Hz\n";
            generator.closeStream();
            std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
            return 1;
        }

        std::cout << usrp->get_pp_string() << '\n'
                  << "TX rate " << actualRate / 1e6 << " MS/s, center "
                  << usrp->get_tx_freq() / 1e6 << " MHz, gain " << usrp->get_tx_gain()
                  << " dB, generator scale " << opt.scale << '\n';

        uhd::stream_args_t streamArgs("fc32", "sc16");
        auto tx = usrp->get_tx_stream(streamArgs);
        const std::size_t chunk = std::max<std::size_t>(1, static_cast<std::size_t>(jsonInfo.sampleRateHz / 100.0));
        BlockQueue queue(100); // up to about 1 second of samples
        std::thread producer([&] {
            while (!g_stop.load()) {
                Block block(chunk);
                const std::size_t n = generator.read(block.data(), chunk);
                if (n == 0) break; // route end
                block.resize(n);
                if (!queue.push(std::move(block))) break;
            }
            queue.close();
        });

        std::cout << "Pre-buffering...\n";
        queue.waitFilled(50);

        uhd::tx_metadata_t md;
        md.start_of_burst = true;
        md.end_of_burst = false;
        md.has_time_spec = false;
        std::size_t starvedCount = 0, underflowCount = 0, blocks = 0;
        const auto t0 = std::chrono::steady_clock::now();
        Block block;
        bool starved = false;
        while (!g_stop.load() && queue.pop(block, starved)) {
            if (starved) ++starvedCount;
            std::size_t sent = 0;
            while (sent < block.size() && !g_stop.load()) {
                const std::size_t n = tx->send(block.data() + sent, block.size() - sent, md, 1.0);
                if (n == 0) continue;
                sent += n;
                md.start_of_burst = false;
            }
            ++blocks;
            uhd::async_metadata_t asyncMd;
            while (tx->recv_async_msg(asyncMd, 0.0)) {
                if (asyncMd.event_code == uhd::async_metadata_t::EVENT_CODE_UNDERFLOW ||
                    asyncMd.event_code == uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET)
                    ++underflowCount;
            }
            if (blocks % 100 == 0) {
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                std::cout << "\rt=" << static_cast<int>(elapsed) << " s  starved=" << starvedCount
                          << "  underflow=" << underflowCount << std::flush;
            }
        }

        md.end_of_burst = true;
        std::complex<float> dummy{};
        tx->send(&dummy, 0, md);
        g_stop = true;
        queue.close();
        producer.join();
        generator.closeStream();
        std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
        std::cout << "\nDone. blocks=" << blocks << " starved=" << starvedCount
                  << " underflow=" << underflowCount << '\n';
        if (starvedCount || underflowCount)
            std::cout << "WARNING: producer/host timing may have caused gaps in the stream.\n";
    } catch (const std::exception& e) {
        std::cerr << "UHD/stream error: " << e.what() << '\n';
        generator.closeStream();
        std::error_code ec; fs::remove(jsonInfo.normalizedPath, ec);
        return 1;
    }
    return 0;
}
