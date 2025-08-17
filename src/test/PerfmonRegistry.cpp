#include "PerfmonRegistry.hpp"
#include "PerformanceMonitor.hpp"

#include <unordered_map>
#include <memory>
#include <mutex>

namespace tiley {

static std::unordered_map<std::string, std::unique_ptr<PerformanceMonitor>> g_registry;
static std::unordered_map<std::string, std::string> g_path_override;
//保证独立读取性能
static std::mutex g_mu;

// 默认路径
static std::string default_path_for(const std::string& tag) {
    // TODO：类似json哪里这里应该也要换成可自己配置的那种
    return "/home/zero/tiley/src/lib/test_" + tag + ".txt";
}

PerformanceMonitor& perfmon(const std::string& tag) {
    std::lock_guard<std::mutex> lk(g_mu);

    auto it = g_registry.find(tag);
    if (it == g_registry.end()) {
        // 解析路径（先看覆盖,其次用默认规则）
        //TODO:可以根据名字来自动创建文件目录
        std::string path;
        auto pit = g_path_override.find(tag);
        if (pit != g_path_override.end()) {
            path = pit->second;
        } else {
            path = default_path_for(tag);
        }

        auto ins = g_registry.emplace(tag, std::make_unique<PerformanceMonitor>(path));
        return *(ins.first->second);
    }
    return *(it->second);
}

//TODO:可动态更新路径。
void setPerfmonPath(const std::string& tag, const std::string& path) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_path_override[tag] = path;

    // 若实例已存在,立刻更新其路径
    auto it = g_registry.find(tag);
    if (it != g_registry.end() && it->second) {
        it->second->setPath(path);
    }
}

bool hasPerfmon(const std::string& tag) {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_registry.find(tag) != g_registry.end();
}

void shutdownPerfmons() {
    std::lock_guard<std::mutex> lk(g_mu);
    g_registry.clear();
    g_path_override.clear();
}

} 
