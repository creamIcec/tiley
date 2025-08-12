#pragma once

#include <GLES2/gl2.h>
#include <LNamespaces.h>
#include <LPoint.h>
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>

// 代表一条渲染流水线。在这个类中, 我们组装流水线, 并使用里面的shader程序
namespace tiley {
    class Shader {  
        public:
            Shader();
            ~Shader();
            // 链接顶点着色器+片段着色器 = 完整的渲染脚本
            bool link(GLuint vertexShader, GLuint fragmentShader);
            // 使用该程序
            void use() const;
            // 设置程序传入参数的工具函数
            void setUniform(const char* name, int value);
            void setUniform(const char* name, float value);
            void setUniform(const char* name, const Louvre::LPointF& value);
            void setUniform(const char* name, const glm::vec3& vec3);
            void setUniform(const char* name, const glm::mat4& mat4);

            GLuint id() const {return m_programID;}

        private:
            GLuint m_programID { 0 };
    };
} // namespace tiley