#include "Utils.hpp"
#include "config.h"
#include <filesystem>

using namespace Louvre;

std::string tiley::getShaderPath(const std::string &shaderName){
    // 路径 1: 优先检查开发环境的路径
    // 使用 std::filesystem::path 来拼接,这是最安全、跨平台的方式
    std::filesystem::path dev_path = SHADER_SOURCE_DIR;
    dev_path /= shaderName;

    if (std::filesystem::exists(dev_path)) {
        return dev_path.string();
    }

    // 路径 2: 如果开发路径找不到,再检查安装后的系统路径
    std::filesystem::path install_path = SHADER_INSTALL_DIR;
    install_path /= shaderName;

    if (std::filesystem::exists(install_path)) {
        return install_path.string();
    }
    
    // 如果两个权威路径都找不到, 说明出了大问题
    LLog::error("错误: 无法在开发目录: [%s] 或 安装目录 [%s] 找到着色器文件 '%s'",
        dev_path.c_str(),
        install_path.c_str(),       
        shaderName.c_str()
    );

    return "";
}

std::string tiley::getDefaultWallpaperPath() {
    if (std::filesystem::exists(DEFAULT_WALLPAPER_INSTALL_PATH)) {
        return DEFAULT_WALLPAPER_INSTALL_PATH;
    }
    return DEFAULT_WALLPAPER_SOURCE_PATH;
}

std::string tiley::getHotkeyConfigPath() {
    // 1. 确定用户配置路径
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        LLog::warning("[ShortcutManager] 无法获取 HOME 目录,跳过用户配置。");
    }
    
    std::filesystem::path userConfigPath;
    if (homeDir) {
        userConfigPath = homeDir;
        userConfigPath /= ".config/tiley/hotkey.json";
    }

    // 2. 确定系统/开发默认配置路径
    std::filesystem::path defaultConfigPath = DEFAULT_HOTKEY_INSTALL_PATH;
    if (!std::filesystem::exists(defaultConfigPath)) {
        defaultConfigPath = DEFAULT_HOTKEY_SOURCE_PATH;
    }

    // 3. 应用“黄金法则”
    if (homeDir && std::filesystem::exists(userConfigPath)) {
        LLog::log("[ShortcutManager] 找到用户快捷键配置: %s", userConfigPath.c_str());
        return userConfigPath;
    } else {
        LLog::log("[ShortcutManager] 未找到用户配置,使用默认配置: %s", defaultConfigPath.c_str());
        
        // 【关键的用户体验提升】
        // 如果默认配置存在,而用户配置不存在,则自动为用户创建一份
        if (homeDir && std::filesystem::exists(defaultConfigPath)) {
            try {
                // 确保目标目录存在
                std::filesystem::create_directories(userConfigPath.parent_path());
                // 复制文件
                std::filesystem::copy_file(defaultConfigPath, userConfigPath);
                LLog::log("[ShortcutManager] 已将默认配置复制到 %s,方便用户修改。", userConfigPath.c_str());
            } catch (const std::filesystem::filesystem_error& e) {
                LLog::error("[ShortcutManager] 复制默认配置失败: %s", e.what());
            }
        }
        return defaultConfigPath;
    }
}