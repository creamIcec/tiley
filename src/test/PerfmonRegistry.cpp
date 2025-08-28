#include "PerfmonRegistry.hpp"
#include "PerformanceMonitor.hpp"

#include <unordered_map>
#include <memory>
#include <mutex>

namespace tiley {

static std::unordered_map<std::string, std::unique_ptr<PerformanceMonitor>> g_registry;
static std::unordered_map<std::string, std::string> g_path_override;
static std::mutex g_mu;

static std::string default_path_for(const std::string& tag) {
    // TODO: hard-coded path
    return "/home/zero/tiley/src/lib/test_" + tag + ".txt";
}

PerformanceMonitor& perfmon(const std::string& tag) {
    std::lock_guard<std::mutex> lk(g_mu);

    auto it = g_registry.find(tag);
    if (it == g_registry.end()) {
        // Solve Path: Overriding first, followed by rule
        //TODO: Get dir by name
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

//TODO: Dynamically update path
void setPerfmonPath(const std::string& tag, const std::string& path) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_path_override[tag] = path;

    // Instance exists, update its path
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
