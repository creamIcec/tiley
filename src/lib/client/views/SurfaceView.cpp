
#include "SurfaceView.hpp"
#include "LSurfaceView.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/types.hpp"

#include <GLES2/gl2.h>
#define GLM_FORCE_RADIANS // 确保 glm 使用弧度，与 OpenGL 标准一致
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // 包含 glm::translate, glm::rotate, glm::scale
#include <glm/gtc/type_ptr.hpp>         // 包含 glm::value_ptr

#include <LPointerButtonEvent.h>
#include <LLog.h>
#include <LNamespaces.h>
#include <LPainter.h>
#include <private/LPainterPrivate.h>

using namespace tiley;

// 必须在SurfaceView的构造函数中就初始化对应的父亲层级关系
// 由于我们不知道surface何时提交到合成器, 如果在mappingChanged中才现场setParent的话太晚, 因为orderChanged比mappingChanged更早触发
// 在orderChanged里面又要对view进行排序。所以我们必须在orderChanged前(甚至所有surface相关提交操作之前)就把父级关系设置好

SurfaceView::SurfaceView(Surface* surface) noexcept : 
LSurfaceView((LSurface*) surface, &TileyServer::getInstance().layers()[APPLICATION_LAYER]){

    this->setColorFactor({1.0,1.0,1.0,0.9});

}

void SurfaceView::pointerButtonEvent(const LPointerButtonEvent &event){

    LLog::log("鼠标点击");

    L_UNUSED(event);
}

void SurfaceView::pointerEnterEvent (const LPointerEnterEvent &event){
    L_UNUSED(event);
    LLog::log("鼠标进入");
};


void SurfaceView::paintEvent(const PaintEventParams& params) noexcept{
    TileyServer &server = TileyServer::getInstance();
    Shader *shader = server.roundedCornerShader();

    // 如果不是窗口, 使用默认绘制方法
    if(surface() && !surface()->toplevel()){
        LSurfaceView::paintEvent(params);
        return;
    }

    // 如果没有着色器或者窗口没有纹理, 也使用默认绘制方法
    if (!shader || !surface() || !surface()->texture()) {
        LLog::warning("找不到shader着色器脚本/编译失败, 使用窗口默认绘制方法");
        LSurfaceView::paintEvent(params);
        return;
    }

    // --- 准备 OpenGL 状态 ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- 激活着色器和几何体 ---
    shader->use();
    // --- 绑定纹理 ---
    glActiveTexture(GL_TEXTURE0);
    // TODO: 由于Louvre的多线程特性, 一个texture的buffer不是线程共享的, 而是每个屏幕一个对象。这里需要用更好的方式获取当前屏幕
    // 我们暂时传入nullptr, 默认指定为主线程(main thread)的buffer
    glBindTexture(GL_TEXTURE_2D, surface()->texture()->id(nullptr));

    glBindBuffer(GL_ARRAY_BUFFER, server.quadVBO());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, server.quadEBO());

    // 万能的GPU啊, 你要这样解释数据格式...
    // 这个是属性0: 是我要渲染的坐标
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // 这个是属性1: 是纹理坐标
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    // ...麻烦您把这两个之间映射起来, 谢谢

    shader->setUniform("u_texture", 0);
    shader->setUniform("u_resolution", LSizeF(size()));
    shader->setUniform("u_radius", 12.f); // TODO: 圆角半径, 可以从配置文件读取

    // TODO: 将当前该surface可见的屏幕都执行一遍
    LOutput *out = surface()->outputs()[0];

    if(!out){
        // 这个surface没有被映射, 不用绘制了
        return;
    }

    glm::mat4 projection_matrix = glm::ortho(0.0f, (float)out->size().w(), (float)out->size().h(), 0.0f, -1.0f, 1.0f);

    // 2. 创建模型矩阵
    // 从一个单位矩阵开始
    glm::mat4 model_matrix = glm::mat4(1.0f);
    // a. 平移：将我们的单位矩形的原点移动到视图的左上角位置
    model_matrix = glm::translate(model_matrix, glm::vec3(pos().x(), pos().y(), 0.0f));
    // b. 缩放：将我们的单位矩形(1x1)缩放到视图的实际大小
    model_matrix = glm::scale(model_matrix, glm::vec3(size().w(), size().h(), 1.0f));

    // 3. 计算最终变换
    glm::mat4 final_transform = projection_matrix * model_matrix;

    // 4. 将矩阵传递给着色器
    GLuint transformLoc = glGetUniformLocation(shader->id(), "u_transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(final_transform));

    // --- 执行绘制 ---
    // 因为使用了 EBO，我们用 glDrawElements
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // --- 清理 ---
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

const LRegion * SurfaceView::translucentRegion() const noexcept{
    if(surface() && surface()->toplevel()){
        return nullptr;
    }else{
        return LSurfaceView::translucentRegion();
    }
}