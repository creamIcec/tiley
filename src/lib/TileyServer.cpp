#include <memory>
#include <mutex>

#include <LOpenGL.h>
#include <LLog.h>

#include "src/lib/TileyServer.hpp"
#include "src/lib/input/ShortcutManager.hpp"
#include "src/lib/scene/Scene.hpp"
#include "TileyCompositor.hpp"
#include "Utils.hpp"

using namespace tiley;

std::unique_ptr<TileyServer, TileyServer::ServerDeleter> TileyServer::INSTANCE = nullptr;
std::once_flag TileyServer::onceFlag;

TileyServer::TileyServer(){}
TileyServer::~TileyServer(){}

TileyServer& TileyServer::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyServer());
    });
    return *INSTANCE;
}

Scene& TileyServer::scene() noexcept{
    return compositor()->scene;
}

LayerView* TileyServer::layers() noexcept{
    return scene().layers;
}

void TileyServer::populateStartupCMD(std::string cmd){
    startUpCMD.push_back(cmd);   
}

void TileyServer::initOpenGLResources(){

    std::string vert_path = getShaderPath("rounded_corners.vert");
    std::string frag_path = getShaderPath("rounded_corners.frag");

    if (!vert_path.empty() && !frag_path.empty()) {

        char* vShaderSrc = LOpenGL::openShader(vert_path.c_str());
        char* fShaderSrc = LOpenGL::openShader(frag_path.c_str());

        if (!vShaderSrc || !fShaderSrc) {
            LLog::error("[initOpenGLResources]: unable to open shader path, please check. Stop compiling");
            if(vShaderSrc) free(vShaderSrc);
            if(fShaderSrc) free(fShaderSrc);
            return;
        }

        GLuint vShader = LOpenGL::compileShader(GL_VERTEX_SHADER, vShaderSrc);
        GLuint fShader = LOpenGL::compileShader(GL_FRAGMENT_SHADER, fShaderSrc);

        free(vShaderSrc);
        free(fShaderSrc);

        if (vShader == 0 || fShader == 0) {
            LLog::error("[initOpenGLResources]: unable to compile shaders, see output for details");
            return;
        }
        
        m_roundedCornerShader = std::make_unique<Shader>();
        if (!m_roundedCornerShader->link(vShader, fShader)) {
            LLog::error("[initOpenGLResources]: unable to link shaders, this may be a bug, please report");
            m_roundedCornerShader.reset();
        }

        glDeleteShader(vShader);
        glDeleteShader(fShader);

        // EBO & VBO
        // start from bottom-right and rotating clockwise
        float vertices[] = {
            // pos      // tex
            1.f, 1.f,   1.f, 1.f,
            0.f, 1.f,   0.f, 1.f,
            0.f, 0.f,   0.f, 0.f,
            1.f, 0.f,   1.f, 0.f 
        };

        unsigned int indices[] = {
            0, 1, 2,
            2, 3, 0
        };
        
        glGenBuffers(1, &m_quadVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glGenBuffers(1, &m_quadEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    } else {

        LLog::warning("[initOpenGLResources]: warning: unable to use custom shaders, fallback to default pipeline");
        
    }
}

void TileyServer::uninitOpenGLResources(){
    m_roundedCornerShader.reset();
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteBuffers(1, &m_quadEBO);
}

void TileyServer::initKeyEventHandlers(){
    auto& shortcutManager = ShortcutManager::getInstance();
    shortcutManager.initializeHandlers();
}