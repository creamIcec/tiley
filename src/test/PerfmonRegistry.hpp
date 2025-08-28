#pragma once

#include <string>
class PerformanceMonitor;

namespace tiley {


// Get perf monitor instances by tag
PerformanceMonitor& perfmon(const std::string& tag);

// Overriding perf log path
void setPerfmonPath(const std::string& tag, const std::string& path);

bool hasPerfmon(const std::string& tag);

// No need to shutdown before process exits
void shutdownPerfmons();

} 
