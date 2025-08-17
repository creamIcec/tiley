#include "PerformanceMonitor.hpp"

#include <fstream>
#include <sys/resource.h>
#include <unistd.h>

//构造函数
PerformanceMonitor::PerformanceMonitor(const std::string& file_path)
    : file_path_(file_path),
      frames_(0),
      start_time_(std::chrono::steady_clock::now()) // steady_clock: 单调时钟
{}

// 设置输出文件路径
void PerformanceMonitor::setPath(const std::string& p) {
    file_path_ = p;
}

// 记录渲染开始时间
void PerformanceMonitor::renderStart() {
    render_start_time_= std::chrono::steady_clock::now();
}

// 记录渲染结束时间,并存储单帧渲染耗时
void PerformanceMonitor::renderEnd() {
    auto render_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> render_duration = render_end_time - render_start_time_;
    render_durations_.push_back(render_duration.count());
}

// 记录一帧,并在累积到一定数量后输出性能数据
void PerformanceMonitor::recordFrame() {
    frames_++;
    auto frame_time = std::chrono::steady_clock::now();

    // 两帧时间差
    std::chrono::duration<double> elapsed = frame_time - start_time_;
    double elapsed_seconds = elapsed.count();

    // ===== 空闲帧过滤 =====
    // 如果两帧间隔超过阈值(0.5秒),认为是 idle,不计入 FPS 统计
    constexpr double idle_threshold_seconds = 0.5;
    if (elapsed_seconds <= idle_threshold_seconds) {
        frame_times_.push_back(elapsed_seconds);
    }

    // 更新时间戳
    start_time_ = frame_time;

    // 每隔 60 帧输出一次性能数据
    if (frames_ % 60 == 0) {
        logMetrics();
    }
}

// 输出性能数据到文件
void PerformanceMonitor::logMetrics() {
    double cpu_usage       = getCpuUsage();        // 累计 CPU 时间(秒)
    double memory_usage_mb = getMemoryUsage();     // 内存使用量(MB)
    double fps             = calculateFps();       // 平均 FPS
    double avg_render_time = calculateAvgRenderTime(); // 平均渲染耗时(秒)

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

// 获取进程 CPU 使用时间（秒）
double PerformanceMonitor::getCpuUsage() {
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
}

// 获取进程最大常驻内存（MB）
double PerformanceMonitor::getMemoryUsage() {
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0; // KB → MB
}

// 计算平均 FPS
double PerformanceMonitor::calculateFps() {
    double total_time = 0.0;
    for (double t : frame_times_) total_time += t;
    return frame_times_.empty() ? 0.0 : frame_times_.size() / total_time;
}

// 计算平均渲染耗时（秒）
double PerformanceMonitor::calculateAvgRenderTime() {
    double total = 0.0;
    for (double rt : render_durations_) total += rt;
    return render_durations_.empty() ? 0.0 : total / render_durations_.size();
}
