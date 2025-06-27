// src/shader_utils.cpp
#include "include/server.hpp"
#include <fstream>
#include <sstream>
#include <string>

// 从文件加载 GLSL 源码的辅助函数
static std::string load_shader_code(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        wlr_log(WLR_ERROR, "Could not open shader file: %s", path.c_str());
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// 辅助函数：编译单个着色器
static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        wlr_log(WLR_ERROR, "Failed to compile shader: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// 初始化函数 (原生 GLES2 版本)
bool tiley::init_shader(tiley::TileyShader& shader, const std::string& vert_path, const std::string& frag_path) {
    std::string vert_src_str = load_shader_code(vert_path);
    std::string frag_src_str = load_shader_code(frag_path);
    if (vert_src_str.empty() || frag_src_str.empty()) {
        return false;
    }

    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src_str.c_str());
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src_str.c_str());
    if (!vert || !frag) {
        return false;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    
    // 在链接前绑定顶点属性位置，这比 glGetAttribLocation 更稳健
    glBindAttribLocation(prog, 0, "pos");
    glBindAttribLocation(prog, 1, "texcoord");
    
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        wlr_log(WLR_ERROR, "Failed to link shader program: %s", log);
        glDeleteProgram(prog);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return false;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    shader.program = prog;
    shader.proj = glGetUniformLocation(prog, "proj");
    shader.tex = glGetUniformLocation(prog, "tex");
    shader.alpha = glGetUniformLocation(prog, "alpha");

    wlr_log(WLR_INFO, "Successfully created and linked GLES2 shader program from %s", frag_path.c_str());
    return true;
}

