#include "PerformanceMonitor.hpp"

#include <fstream>
#include <sys/resource.h>
#include <unistd.h>

PerformanceMonitor::PerformanceMonitor(const std::string& file_path)
    : file_path_(file_path),
      frames_(0),
      start_time_(std::chrono::steady_clock::now()) // steady_clock: 单调时钟
{}

// Set monitor output path
void PerformanceMonitor::setPath(const std::string& p) {
    file_path_ = p;
}

// Render start
void PerformanceMonitor::renderStart() {
    render_start_time_= std::chrono::steady_clock::now();
}

// Save single frame render time
void PerformanceMonitor::renderEnd() {
    auto render_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> render_duration = render_end_time - render_start_time_;
    render_durations_.push_back(render_duration.count());
}


// Accumulating performance data per frame and output once enough
void PerformanceMonitor::recordFrame() {
    frames_++;
    auto frame_time = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed = frame_time - start_time_;
    double elapsed_seconds = elapsed.count();

    // Idle frames ( > 0.5s idling time) are not counted
    constexpr double idle_threshold_seconds = 0.5;
    if (elapsed_seconds <= idle_threshold_seconds) {
        frame_times_.push_back(elapsed_seconds);
    }

    // Update timestamp
    start_time_ = frame_time;

    if (frames_ % 60 == 0) {
        logMetrics();
    }
}

// Log performance data to file
void PerformanceMonitor::logMetrics() {
    double cpu_usage       = getCpuUsage();
    double memory_usage_mb = getMemoryUsage();
    double fps             = calculateFps();
    double avg_render_time = calculateAvgRenderTime();

    double cpu_fps_ratio   = fps != 0 ? cpu_usage / fps : 0;

    std::ofstream file(file_path_, std::ios::app);
    if (file.is_open()) {
        file << "FPS: " << fps
             << ", CPU(s): " << cpu_usage
             << ", CPU/FPS Ratio: " << cpu_fps_ratio
             << ", Memory(MB): " << memory_usage_mb
             << ", Avg Render Time (ms): " << avg_render_time * 1000
             << "\n";
    }
}

// Process CPU time
double PerformanceMonitor::getCpuUsage() {
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
}

// Process static memory usage
double PerformanceMonitor::getMemoryUsage() {
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0; // KB → MB
}

// Average FPS
double PerformanceMonitor::calculateFps() {
    double total_time = 0.0;
    for (double t : frame_times_) total_time += t;
    return frame_times_.empty() ? 0.0 : frame_times_.size() / total_time;
}

// Average Render time (seconds)
double PerformanceMonitor::calculateAvgRenderTime() {
    double total = 0.0;
    for (double rt : render_durations_) total += rt;
    return render_durations_.empty() ? 0.0 : total / render_durations_.size();
}
