#pragma once

#include <string>
class PerformanceMonitor;

namespace tiley {

// 获取指定“标签”的监视器实例（不存在则按创建），这里方便一个文件存储对应的性能
PerformanceMonitor& perfmon(const std::string& tag);

// 覆盖某个标签的输出文件路径
void setPerfmonPath(const std::string& tag, const std::string& path);


bool hasPerfmon(const std::string& tag);

//：释放全部实例（进程退出前不必调用）
void shutdownPerfmons();

} 
