#include "Shader.hpp"
#include "LNamespaces.h"
#include <GLES2/gl2.h>
#include <LLog.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

using namespace tiley;
using namespace Louvre;

Shader::Shader() {
    m_programID = glCreateProgram();
}

Shader::~Shader() {
    // destroy shader program object
    if (m_programID != 0) {
        glDeleteProgram(m_programID);
    }
}

bool Shader::link(GLuint vertexShader, GLuint fragmentShader) {
    if (m_programID == 0 || vertexShader == 0 || fragmentShader == 0) {
        return false;
    }

    // attach
    glAttachShader(m_programID, vertexShader);
    glAttachShader(m_programID, fragmentShader);

    // link
    glLinkProgram(m_programID);

    GLint success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_programID, 512, NULL, infoLog);
        Louvre::LLog::error("[link]: failed to link shader program, caused by: \n%s", infoLog);
        return false;
    }

    glDetachShader(m_programID, vertexShader);
    glDetachShader(m_programID, fragmentShader);

    return true;
}

void Shader::use() const {
    glUseProgram(m_programID);
}

// Uniform set toolkit
// TODO: STL
void Shader::setUniform(const char* name, int value) {
    glUniform1i(glGetUniformLocation(m_programID, name), value);
}

void Shader::setUniform(const char* name, float value) {
    glUniform1f(glGetUniformLocation(m_programID, name), value);
}

void Shader::setUniform(const char* name, const LPointF& value) {
    glUniform2f(glGetUniformLocation(m_programID, name), value.x(), value.y());
}

void Shader::setUniform(const char* name, const glm::vec3& vec3){
    glUniform3f(glGetUniformLocation(m_programID, name), vec3[0], vec3[1], vec3[2]);
}

void Shader::setUniform(const char* name, const glm::mat4& mat4) {
    glUniformMatrix4fv(glGetUniformLocation(m_programID, name), 1, GL_FALSE, glm::value_ptr(mat4));
}
