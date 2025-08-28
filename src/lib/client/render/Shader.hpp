#pragma once

#include <GLES2/gl2.h>
#include <LNamespaces.h>
#include <LPoint.h>
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
namespace tiley {
    class Shader {  
        public:
            Shader();
            ~Shader();
            bool link(GLuint vertexShader, GLuint fragmentShader);
            void use() const;
            void setUniform(const char* name, int value);
            void setUniform(const char* name, float value);
            void setUniform(const char* name, const Louvre::LPointF& value);
            void setUniform(const char* name, const glm::vec3& vec3);
            void setUniform(const char* name, const glm::mat4& mat4);

            GLuint id() const {return m_programID;}

        private:
            GLuint m_programID { 0 };
    };
}