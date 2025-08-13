#pragma once

#include <string>
#include <LLog.h>

namespace tiley{
    std::string getShaderPath(const std::string& shaderName);
    namespace math{
        // 模板函数: 对两个多维向量类对象进行线性插值
        template<typename T>
        T lerp(const T& start, const T& end, float a) {
            return start * (1.0f - a) + end * a;
        }
    }
}