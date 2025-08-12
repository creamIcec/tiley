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

    // 1. 编译和链接着色器程序
    if (!vert_path.empty() && !frag_path.empty()) {

        char* vShaderSrc = LOpenGL::openShader(vert_path.c_str());
        char* fShaderSrc = LOpenGL::openShader(frag_path.c_str());

        if (!vShaderSrc || !fShaderSrc) {
            LLog::error("无法加载shader渲染脚本。请检查路径");
            if(vShaderSrc) free(vShaderSrc);
            if(fShaderSrc) free(fShaderSrc);
            return;
        }

        GLuint vShader = LOpenGL::compileShader(GL_VERTEX_SHADER, vShaderSrc);
        GLuint fShader = LOpenGL::compileShader(GL_FRAGMENT_SHADER, fShaderSrc);

        free(vShaderSrc);
        free(fShaderSrc);

        if (vShader == 0 || fShader == 0) {
            LLog::error("shader渲染脚本编译错误。请检查是否有语法错误");
            return;
        }
        
        m_roundedCornerShader = std::make_unique<Shader>();
        if (!m_roundedCornerShader->link(vShader, fShader)) {
            LLog::error("无法链接shader渲染脚本成渲染管线。");
            m_roundedCornerShader.reset();
        }

        glDeleteShader(vShader);
        glDeleteShader(fShader);


        // --- 2. 创建 VBO 和 EBO ---
        // 我们只创建和填充缓冲区，不做任何状态绑定
        float vertices[] = {
            // pos      // tex
            1.f, 1.f,   1.f, 1.f, // 右下
            0.f, 1.f,   0.f, 1.f, // 左下
            0.f, 0.f,   0.f, 0.f, // 左上
            1.f, 0.f,   1.f, 0.f  // 右上
        };

        // 三角索引缓冲(在OpenGL中, 所有的渲染单位都是三角形, 一个矩形由两个三角形组成, 因此我们需要6个索引)
        unsigned int indices[] = {
            0, 1, 2, // 第一个三角形(右下, 左下, 左上)
            2, 3, 0  // 第二个三角形(左上, 右上, 右下)
        };
        
        glGenBuffers(1, &m_quadVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glGenBuffers(1, &m_quadEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // 解绑，保持 OpenGL 状态干净
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    } else {

        LLog::warning("警告: 无法找到着色器程序, 将使用默认着色程序渲染");
        
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