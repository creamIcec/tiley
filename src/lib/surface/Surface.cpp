#include "Surface.hpp"

#include <cmath>

#include <LLog.h>
#include <LCursor.h>
#include <LSubsurfaceRole.h>
#include <LClient.h>
#include <LLayerView.h>
#include <LNamespaces.h>
#include <LSessionLockRole.h>
#include <LSurface.h>
#include <LToplevelRole.h>
#include <LView.h>
#include <LExclusiveZone.h>
#include <LMargins.h>
#include <LAnimation.h>
#include <LRegion.h>
#include <LSceneView.h>
#include <LTextureView.h>

#include "LTexture.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/views/SurfaceView.hpp"
#include "src/lib/types.hpp"
#include "src/lib/Utils.hpp"

using namespace Louvre;
using namespace tiley;

#define ENUM_CASE(e) case e: return #e;

std::string getEnumName(LSurface::Role value) {
    switch(value) {
        ENUM_CASE(LSurface::Role::Undefined)
        ENUM_CASE(LSurface::Role::Toplevel)
        ENUM_CASE(LSurface::Role::Popup)
        ENUM_CASE(LSurface::Role::Subsurface)
        ENUM_CASE(LSurface::Role::Cursor)
        ENUM_CASE(LSurface::Role::DNDIcon)
        ENUM_CASE(LSurface::Role::SessionLock)
        ENUM_CASE(LSurface::Role::Layer)
        default: return "UNKNOWN";
    }
}

LTexture* Surface::renderThumbnail(LRegion* transRegion){
    LBox box { getView()->boundingBox() };

    minimizeStartRect = LRect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);

    LSceneView tmpView(minimizeStartRect.size(), 1);
    tmpView.setPos(minimizeStartRect.pos());

    LView *prevParent { getView()->parent() };
    getView()->setParent(&tmpView);


    struct TMPList
    {
        LSurfaceView *view;
        LView *parent;
    };

    std::list<TMPList> tmpChildren;

    Surface *next { this };
    while ((next = (Surface*)next->nextSurface()))
    {
        if (next->parent() == this && next->subsurface())
        {
            tmpChildren.emplace_back(next->view.get(), next->view->parent());
            next->view->enableParentOffset(false);
            next->view->setParent(&tmpView);
        }
    }

    getView()->enableParentOffset(false);

    tmpView.render();

    if (transRegion)
    {
        *transRegion = *tmpView.translucentRegion();
        transRegion->offset(LPoint() - tmpView.pos());
    }

    LTexture *renderedThumbnail { tmpView.texture()->copy() };
    getView()->enableParentOffset(true);
    getView()->setParent(prevParent);

    while (!tmpChildren.empty())
    {
        tmpChildren.front().view->enableParentOffset(true);
        tmpChildren.front().view->setParent(tmpChildren.front().parent);
        tmpChildren.pop_front();
    }

    return renderedThumbnail;

}

void Surface::startUnmappedAnimation() noexcept
{
    if (isClosing) return;
    isClosing = true;

    view->enableAlwaysMapped(true);
    LTexture* thumbnail = renderThumbnail();

    if (!thumbnail) {
        view->enableAlwaysMapped(false);
        return;
    }

    auto& server = TileyServer::getInstance();
    LTextureView *fadeOutView = new LTextureView(thumbnail, &server.layers()[APPLICATION_LAYER]);

    const LPoint initialPos = pos();
    // 【新】我们将在这里定义动画的目标,让代码更清晰
    constexpr Float32 FINAL_SCALE_RATIO = 0.7f; // 可配置的目标缩放比例
    constexpr UInt32 ANIMATION_MS = 150;       // 动画可以更快一些,感觉更清脆

    fadeOutView->setPos(initialPos);
    fadeOutView->enableParentOffset(false);
    view->enableAlwaysMapped(false);
    
    fadeOutView->enableScaling(true);
    LWeak<Surface> weakSelf {this};

    LAnimation::oneShot(ANIMATION_MS, 
        [fadeOutView, initialPos, FINAL_SCALE_RATIO](LAnimation *animation) {
            
            // 使用更平滑的 "Ease-In-Out" 缓动函数
            // 这会让动画的开始和结束都感觉非常自然,没有突兀感
            const Float32 ease = (1.f - cosf(animation->value() * M_PI)) * 0.5f;

            // 重新计算缩放插值
            // 我们使用自己的 lerp 函数,将动画进程(ease: 0->1)映射到缩放范围(1.0f -> 0.8f)
            const float scaleFactor = tiley::math::lerp(1.f, FINAL_SCALE_RATIO, ease);
            fadeOutView->setScalingVector(scaleFactor);
            
            // 重新计算位置插值
            // 目标位置不再是中心点,而是根据缩放比例计算出的新位置
            // 这样能确保窗口的中心在动画过程中保持不动
            const LPoint currentPos = initialPos + (fadeOutView->size() * (1.f - scaleFactor)) / 2;
            fadeOutView->setPos(currentPos);
            
            // 透明度插值
            // 让窗口在缩小的同时, 完全淡出。这会产生一个非常干净的收尾效果。
            const float opacity = 1.f - ease;
            fadeOutView->setOpacity(opacity);

            compositor()->repaintAllOutputs();
        },
        [fadeOutView, weakSelf](LAnimation *) { // 在 onFinish 中捕获 weakSelf 以便调用 close

            if (fadeOutView) {
                delete fadeOutView->texture();
                delete fadeOutView;
            }

            compositor()->repaintAllOutputs();
        }
    );
}



Surface::Surface(const void *params) : LSurface(params){
    fadeInAnimation.setOnUpdateCallback([this](LAnimation* animation){
        const Float32 ease { 1.f - powf(1.f - animation->value(), 6.f) };
        getView()->setOpacity(ease);

        Surface *next = (Surface*)nextSurface();
        while (next)
        {
            if (next->isSubchildOf(this) && !next->minimized())
            {
                next->getView()->setOpacity(ease);
            }
            next = (Surface*)next->nextSurface();
        }
        repaintOutputs();
    });

    fadeInAnimation.setOnFinishCallback([this](LAnimation*){
        getView()->setOpacity(1.f);

        Surface *next = (Surface*)nextSurface();

        while (next)
        {
            if (next->isSubchildOf(this) && !next->minimized())
            {
                next->getView()->setOpacity(1.f);
            }

            next = (Surface*)next->nextSurface();
        }
        repaintOutputs();
    });
}

// 获取要操作的view的方法
// 对于正在平铺的窗口, 返回包装器; 对于目前没有平铺的窗口或其他任意角色, 返回view本身
LView* Surface::getView() noexcept{

    if(tl() && tl()->container && TileyWindowStateManager::getInstance().isTiledWindow(tl())){
        return tl()->container->getContainerView();
    }else{
        return view.get();
    }
}

// 用于需要直接访问的情况
LSurfaceView* Surface::getSurfaceView() noexcept{
    return view.get();
}

void Surface::printWindowGeometryDebugInfo(LOutput* activeOutput, const LRect& outputAvailable) noexcept{
    if(toplevel()){
        const LMargins& margin = toplevel()->extraGeometry();
        const LRect& acquiredGeometry = size();
        const LRect& windowGeometry = toplevel()->windowGeometry();

        LLog::log("**************新建窗口信息(内存地址: %d)**************", this);
        LLog::log("屏幕可用空间: %dx%d", outputAvailable.w(), outputAvailable.h());
        for(auto zone : activeOutput->exclusiveZones()){
            LLog::log("屏幕保留空间: %dx%d, 附着在边缘: %b", zone->rect().w(), zone->rect().h(), zone->edge());
        }
        LLog::log("额外的空间: top: %d, right: %d, bottom: %d, left: %d", margin.top, margin.right, margin.bottom, margin.left);
        LLog::log("请求大小: %dx%d", acquiredGeometry.w(), acquiredGeometry.h());
        LLog::log("实际占用大小: %dx%d", windowGeometry.w(), windowGeometry.h());
        LLog::log("窗口画面偏移: (%d, %d)", toplevel()->windowGeometry().pos().x(), toplevel()->windowGeometry().pos().y());

    }else{
        LLog::log("警告: 目标Surface不是窗口角色, 跳过信息打印。");
    }
}

// 调用路径: roleChanged() -> orderChanged -> ((如果是窗口) -> configureRequest() -> atomsChanged()) -> mappingChanged 
// 首先分配一个角色, 再组装顺序(窗口和他的子Surface之间), 之后判断: 如果一个Surface是窗口, 则触发配置过程。配置的结果可以由atomsChanged接收。配置完成后, 客户端开始显示: 调用MappingChanged, 随后就是显示之后自己的工作了。 
// 如果不是窗口, 则在组装顺序后直接MappingChanged.

// roleChanged: 角色发生变化。目前不影响渲染。
// 只需做一件事, 将软件光标隐藏即可, 我们有硬件鼠标。
void Surface::roleChanged(LBaseSurfaceRole *prevRole){
    // 执行父类的方法。父类方法只是刷新一下屏幕
    LSurface::roleChanged(prevRole);

    TileyServer& server = TileyServer::getInstance();

    if(cursorRole()){
        // 我们不能控制提交过来的surface, 但我们可以不渲染它。
        getView()->setVisible(false);
        return;
    }else if (roleId() == LSurface::SessionLock || roleId() == LSurface::Popup){
        LLog::debug("打开了一个弹出菜单");
        getView()->setParent(&server.layers()[LLayerOverlay]);
    }
}

// orderChanged: surface的顺序发生变化。我们需要据此对应调整view的顺序。
void Surface::orderChanged()
{   
    //LLog::debug("顺序改变, surface地址: %d, 层次: %d", this, layer());
    
    // 调试: 打印surface前后关系
    // TileyServer& server = TileyServer::getInstance();
    // server.compositor()->printToplevelSurfaceLinklist();

    // Louvre-views 官方写法

    if (toplevel() && toplevel()->fullscreen())
        return;

    Surface *prevSurface { static_cast<Surface*>(this->prevSurface()) };
    LView *view { getView() };

    while (prevSurface != nullptr)
    {
        if (subsurface() || prevSurface->getView()->parent() == view->parent())
            break;

        prevSurface = static_cast<Surface*>(prevSurface->prevSurface());
    }

    if (prevSurface)
    {
        if (subsurface() || prevSurface->getView()->parent() == getView()->parent())
            view->insertAfter(prevSurface->getView());
    }
    else
        view->insertAfter(nullptr);
}

// layerChanged: 层次发生变化。对于我们来讲最有用的就是平铺层<->浮动层之间的互相切换。
// 只需将toplevel的view移动到新的层即可, 它的弹出窗口和它的subsurface会自动跟随。
void Surface::layerChanged(){
    //LLog::debug("%d: 层次改变: ", this);
    TileyServer& server = TileyServer::getInstance();
    getView()->setParent(&server.layers()[layer()]);
}

// mappingChanged: 一个窗口的显示状态发生变化。常见于: 新的窗口要显示/窗口被关闭/窗口被最小化等
// 我们要做的: 为每个surface指定父亲(之后他就可以自动跟随(计算相对位置)了)
void Surface::mappingChanged(){

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    // 先刷新一次屏幕, 确保视觉更新
    compositor()->repaintAllOutputs();

    if(mapped()){
        // 如果不是窗口, 则无需额外的操作了(SurfaceView构造函数已经初始化过默认显示层级是APPLICATION_LAYER, 不用担心非窗口的Surface没有得到处理)
        if(!toplevel()){
            return;
        }
        // 否则, 为窗口分配一个类型
        tl()->assignToplevelType();
        // 准备插入
        Container* tiledContainer = nullptr;
        // 尝试插入窗口。如果窗口角色不满足插入平铺层的条件, 则不会插入
        manager.addWindow(tl(), tiledContainer);
        // 如果成功插入平铺层
        if(tiledContainer){
            // 如果是显示出了窗口
            // 设置到应用层(在SurfaceView的构造函数中已经初始化过, 这里调用是为了防止某些地方使其发生了变化)
            getView()->setParent(&server.layers()[APPLICATION_LAYER]);
            // 设置活动容器
            manager.setActiveContainer(tiledContainer);
            LLog::debug("设置上一个活动容器为新打开的窗口容器");
            // 重新计算布局
            manager.recalculate();
        }

        // 最后, 无论何种类型的窗口, 播放创建动画
        fadeInAnimation.setDuration(400 * 1.5f);
        fadeInAnimation.start();

    } else {
        // 如果是关闭(隐藏)了窗口
        if(toplevel()){
            // 如果不全屏, 则播放关闭窗口动画
            if(!toplevel()->fullscreen()){
                startUnmappedAnimation();
            }
            // 准备移除
            Container* siblingContainer = nullptr;
            // 移除窗口(包括平铺的和非平铺的都是这个方法)
            manager.removeWindow(tl(), siblingContainer);
            // 如果移除的是平铺层的窗口
            if(siblingContainer != nullptr){
                // 重新布局
                manager.recalculate();
            }
        }

        // 从该surface上移除焦点
        if(seat()->pointer()->focus() == this){
            seat()->pointer()->setFocus(nullptr);
        }

        // 最后, 无论是什么surface, 都将view从parent的列表中移除, 销毁view
        getView()->setParent(nullptr);
    }
    
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}