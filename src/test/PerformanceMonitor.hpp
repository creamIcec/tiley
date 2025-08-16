#pragma once

#include <chrono>
#include <string>
#include <vector>

class PerformanceMonitor {
public:
    explicit PerformanceMonitor(const std::string& file_path);
    void setPath(const std::string& p);
    void renderStart();
    void renderEnd();
    void recordFrame();

private:
    void logMetrics();
    double getCpuUsage();
    double getMemoryUsage();
    double calculateFps();
    double calculateAvgRenderTime();

    std::string file_path_;
    size_t frames_;


    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point render_start_time_;

    std::vector<double> frame_times_;
    std::vector<double> render_durations_;
};
