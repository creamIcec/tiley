#include "config.h"

#include <string>
#include <filesystem>
#include <LLog.h>

inline std::string getShaderPath(const std::string& shaderName) {
    // 路径 1: 优先检查开发环境的路径
    // 使用 std::filesystem::path 来拼接，这是最安全、跨平台的方式
    std::filesystem::path dev_path = SHADER_SOURCE_DIR;
    dev_path /= shaderName;

    if (std::filesystem::exists(dev_path)) {
        return dev_path.string();
    }

    // 路径 2: 如果开发路径找不到，再检查安装后的系统路径
    std::filesystem::path install_path = SHADER_INSTALL_DIR;
    install_path /= shaderName;

    if (std::filesystem::exists(install_path)) {
        return install_path.string();
    }
    
    // 如果两个权威路径都找不到, 说明出了大问题
    Louvre::LLog::error("错误: 无法在开发目录: [%s] 或 安装目录 [%s] 找到着色器文件 '%s'",
        dev_path.c_str(),
        install_path.c_str(),       
        shaderName.c_str()
    );

    return "";
}