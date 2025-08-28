#include "Utils.hpp"
#include "config.h"
#include <filesystem>

using namespace Louvre;

std::string tiley::getShaderPath(const std::string &shaderName){
    std::filesystem::path dev_path = SHADER_SOURCE_DIR;
    dev_path /= shaderName;

    if (std::filesystem::exists(dev_path)) {
        return dev_path.string();
    }

    // fallback to system path
    std::filesystem::path install_path = SHADER_INSTALL_DIR;
    install_path /= shaderName;

    if (std::filesystem::exists(install_path)) {
        return install_path.string();
    }

    LLog::error("Error: Unable finding shader files: '%s' in dev path '[%s]' or installation path '[%s]'",
        shaderName.c_str(),
        dev_path.c_str(),
        install_path.c_str()
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
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        LLog::warning("[ShortcutManager] Unable to locate HOME path, skipping user configs");
    }
    
    std::filesystem::path userConfigPath;
    if (homeDir) {
        userConfigPath = homeDir;
        userConfigPath /= ".config/tiley/hotkey.json";
    }


    std::filesystem::path defaultConfigPath = DEFAULT_HOTKEY_INSTALL_PATH;
    if (!std::filesystem::exists(defaultConfigPath)) {
        defaultConfigPath = DEFAULT_HOTKEY_SOURCE_PATH;
    }


    if (homeDir && std::filesystem::exists(userConfigPath)) {
        LLog::log("[ShortcutManager] Find user shortcut config: %s", userConfigPath.c_str());
        return userConfigPath;
    } else {
        LLog::log("[ShortcutManager] Cannot find user config, using default : %s", defaultConfigPath.c_str());
        if (homeDir && std::filesystem::exists(defaultConfigPath)) {
            try {
                std::filesystem::create_directories(userConfigPath.parent_path());
                std::filesystem::copy_file(defaultConfigPath, userConfigPath);
                LLog::log("[ShortcutManager] Copied default config to path: %s", userConfigPath.c_str());
            } catch (const std::filesystem::filesystem_error& e) {
                LLog::error("[ShortcutManager] Failed to copy default config: %s", e.what());
            }
        }
        return defaultConfigPath;
    }
}