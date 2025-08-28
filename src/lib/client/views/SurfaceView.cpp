#include "SurfaceView.hpp"

#include "src/lib/TileyServer.hpp"
#include "src/lib/types.hpp"

#include <GLES2/gl2.h>
#include <algorithm>
#include <glm/fwd.hpp>
#define GLM_FORCE_RADIANS // 确保 glm 使用弧度,与 OpenGL 标准一致
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // 包含 glm::translate, glm::rotate, glm::scale
#include <glm/gtc/type_ptr.hpp>         // 包含 glm::value_ptr

#include <LPointerButtonEvent.h>
#include <LLog.h>
#include <LNamespaces.h>
#include <LPainter.h>
#include <private/LPainterPrivate.h>

#include <LToplevelMoveSession.h>
#include <LSurfaceView.h>
#include <LSurface.h>

using namespace tiley;

// 必须在SurfaceView的构造函数中就初始化对应的父亲层级关系
// 由于我们不知道surface何时提交到合成器, 如果在mappingChanged中才现场setParent的话太晚, 因为orderChanged比mappingChanged更早触发
// 在orderChanged里面又要对view进行排序。所以我们必须在orderChanged前(甚至所有surface相关提交操作之前)就把父级关系设置好
SurfaceView::SurfaceView(Surface* surface) noexcept : 
LSurfaceView((LSurface*)surface, &TileyServer::getInstance().layers()[APPLICATION_LAYER]){}

SurfaceView::~SurfaceView() noexcept{}

void SurfaceView::paintEvent(const PaintEventParams& params) noexcept{
    
   // LSurfaceView::paintEvent(params);
   // return;
   tiley::setPerfmonPath("surfaceview", "/home/zero/tiley/src/lib/test/test_performance.txt");
   // 如果自己是正在移动的窗口的SurfaceView
   auto moveSessions = seat()->toplevelMoveSessions();
    auto iterator = std::find_if(moveSessions.begin(), moveSessions.end(), [this](auto session){
        auto surface = static_cast<Surface*>(session->toplevel()->surface());
        if(surface->getSurfaceView() == this){
            return true;   
        }
        return false;
    });

    if(iterator != moveSessions.end()){
        this->setColorFactor({1.0f,1.0f,1.0f,0.8f});
    }else{
        this->setColorFactor({1.0f,1.0f,1.0f,1.0f});
    }
    
    /*LSurfaceView::paintEvent(params);
    return;
    */
  
    // 如果不是窗口, 使用默认绘制方法
    if(surface() && !surface()->toplevel()){
        // performanceMonitor.renderStart();  // 开始记录渲染时间
        LSurfaceView::paintEvent(params);
       // performanceMonitor.renderEnd();  // 结束记录渲染时间
    //performanceMonitor.recordFrame(); // 记录当前帧的数据
        return;
    }

    TileyServer &server = TileyServer::getInstance();
    Shader *shader = server.roundedCornerShader();
    // 获取需要重绘的区域
    LRegion *region = params.region;
    LOutput* output = params.painter->imp()->output;

    // 如果没有着色器或者窗口没有纹理, 也使用默认绘制方法
    if (!shader || !surface() || !surface()->texture() || region->empty()) {
        LLog::warning("当前surface不满足自定义绘制条件, 使用窗口默认绘制方法");
        LSurfaceView::paintEvent(params);
        return;
    }

    // TODO: 将当前该surface可见的屏幕都执行一遍

    if(surface()->outputs().empty()){
        // 如果为空, 说明当前outputs可能处于被重置中的状态(例如, 屏幕物理属性发生改变, 期间没有outputs)
        return;
    }

    LOutput *out = surface()->outputs()[0];

    if(!out){
        // 这个surface没有被映射, 不用绘制了
        return;
    }

    // 保存先前视口
    GLint old_viewport[4];
    glGetIntegerv(GL_VIEWPORT, old_viewport);


    // --- 准备 OpenGL 状态 ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // --- 激活着色器和几何体 ---
    shader->use();
    // --- 绑定纹理 ---
    glActiveTexture(GL_TEXTURE0);
    // TODO: 由于Louvre的多线程特性, 一个texture的buffer不是线程共享的, 而是每个屏幕一个对象。这里需要用更好的方式获取当前屏幕
    // 我们暂时传入nullptr, 默认指定为主线程(main thread)的buffer
    glBindTexture(GL_TEXTURE_2D, surface()->texture()->id(output));
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

    const float scale = out->scale();              // 获取缩放比例, e.g., 2.0 for 200% scaling
    const LSize &logical_size = out->size();       // 逻辑尺寸, e.g., 1920x1080
    const LSize &physical_size = out->sizeB();     // 物理缓冲区尺寸, e.g., 3840x2160

    // 定义圆角半径(逻辑像素, 需要根据实际分辨率放大)
    const float cornerRadius = 8.f; // TODO: 圆角半径, 可以从配置文件读取
    const float borderWidth = 2.f;  // 边框粗细

    shader->setUniform("u_texture", 0);
    shader->setUniform("u_resolution", LSizeF(size()));
    // 传递给着色器的像素值,都需要乘以缩放比例
    shader->setUniform("u_radius", cornerRadius * scale);
    shader->setUniform("u_border_width", borderWidth * scale);
    // 设置边框颜色为白色 (R=1.0, G=1.0, B=1.0)
    shader->setUniform("u_border_color", glm::vec3(1.0f, 1.0f, 1.0f));

    glm::mat4 projection_matrix = glm::ortho(0.0f, (float)logical_size.w(), (float)logical_size.h(), 0.0f, -1.0f, 1.0f);

    // 2. 创建模型矩阵
    // 从一个单位矩阵开始
    glm::mat4 model_matrix = glm::mat4(1.0f);
    // a. 平移：将我们的单位矩形的原点移动到视图的左上角位置(偏移进行补偿)
    model_matrix = glm::translate(model_matrix, glm::vec3(pos().x() - cornerRadius, pos().y() - cornerRadius, 0.0f));
    // b. 缩放：将我们的单位矩形(1x1)缩放到视图的实际大小(扩大3倍进行裁剪补偿, 防止视觉边距过大)
    model_matrix = glm::scale(model_matrix, glm::vec3(size().w() + 3 * cornerRadius, size().h() + 3 * cornerRadius, 1.0f));
    // 3. 计算最终变换
    glm::mat4 final_transform = projection_matrix * model_matrix;
    // 4. 将矩阵传递给着色器
    GLuint transformLoc = glGetUniformLocation(shader->id(), "u_transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(final_transform));

    glViewport(0, 0, physical_size.w(), physical_size.h());
    
    glEnable(GL_SCISSOR_TEST); // 启用剪裁测试

    Int32 n;
    const LBox *boxes = region->boxes(&n);
    for (Int32 i = 0; i < n; i++) {
        const LBox &box = boxes[i];
        
        Int32 physical_x = box.x1 * scale;
        Int32 physical_y = box.y1 * scale;
        Int32 physical_w = (box.x2 - box.x1) * scale;
        Int32 physical_h = (box.y2 - box.y1) * scale;

        // gl坐标系是从左下角开始的(和笛卡尔平面坐标系一致), 而Louvre(或者说计算机坐标系)左上角是原点, 因此需要翻转y轴
        glScissor(physical_x, physical_size.h() - (physical_y + physical_h), physical_w, physical_h);
        
        // 在这个小小的剪裁区域内,执行我们的绘制命令
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }


    // 清理, 还原状态机绘制之前的状态
    glDisable(GL_SCISSOR_TEST);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);

    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);

    params.painter->bindProgram();

    
}

const LRegion * SurfaceView::translucentRegion() const noexcept{
    if(surface() && surface()->toplevel()){
        return nullptr;
    }else{
        return LSurfaceView::translucentRegion();
    }
}