#ifndef PERFORMANCE_MONITOR_HPP
#define PERFORMANCE_MONITOR_HPP

#include <string>
#include <vector>
#include <chrono>

class PerformanceMonitor {
public:
    explicit PerformanceMonitor(const std::string& file_path = "performance_metrics.txt");

    // 渲染开始/结束
    void renderStart();
    void renderEnd();

    // 每帧调用
    void recordFrame();

    // 可手动触发一次写盘
    void logMetrics();

    // 运行时变更输出文件（配合“标签/路径”覆盖使用）
    void setPath(const std::string& p);

private:
    double getCpuUsage();            // 当前实现：累计 CPU 时间（秒）
    double getMemoryUsage();         // ru_maxrss（MB）
    double calculateFps();
    double calculateAvgRenderTime();

private:
    std::string file_path_;
    unsigned int frames_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point render_start_time_;
    std::vector<double> frame_times_;
    std::vector<double> render_durations_;
};

#endif // PERFORMANCE_MONITOR_HPP
