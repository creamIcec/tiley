#pragma once

#include <string>
#include <LLog.h>

namespace tiley{
    std::string getShaderPath(const std::string& shaderName);
    namespace math{
        template<typename T>
        T lerp(const T& start, const T& end, float a) {
            return start * (1.0f - a) + end * a;
        }
    }

    std::string getDefaultWallpaperPath();
    std::string getHotkeyConfigPath();
}