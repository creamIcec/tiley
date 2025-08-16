#include <LLayout.h>
#include <LNamespaces.h>
#include <LWeak.h>
#include <LAnimation.h>
#include <LOutput.h>
#include <LSessionLockRole.h>
#include <LScreenshotRequest.h>
#include <LOutputMode.h>
#include <LPainter.h>
#include <LRegion.h>
#include <LOpenGL.h>
#include <LCursor.h>
#include <LContentType.h>
#include <LLog.h>

#include "Output.hpp"

#include "src/lib/TileyServer.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/WallpaperManager.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

using namespace Louvre;
using namespace tiley;

Output::Output(const void* params) noexcept : LOutput(params){
    // 初始化一个显示屏时, 初始化壁纸:
    // LRegion是一个工具类, 用于提供一块"画布区域", 在这个区域中可以添加很多个矩形, 并可对它们进行布尔运算。
    const LRegion region;  //为壁纸分配一块区域, 使得壁纸在渲染中被考虑进去
    m_wallpaperView.setTranslucentRegion(&region);
} 

void Output::initializeGL(){

    LLog::debug("新的屏幕插入, id: %u 。", this->id());

    TileyServer& server = TileyServer::getInstance();

    LLog::debug("[屏幕id: %u] 初始化要在该屏幕上渲染的窗口。", this->id());

    // 0. BUG修复: 防止在嵌套模式下不断出现无响应的问题, 暂时的一个workaround
    // 在嵌套模式下, 禁用垂直同步
    if(compositor()->graphicBackendId() == LGraphicBackendWayland){
        enableVSync(false);
    }

    // 1. 交给scene, 计算需要显示在这块显示屏上的部分
    server.scene().handleInitializeGL(this);

    // 2. 物理模式指定显示参数: (长度, 宽度, 刷新率)
    const LOutputMode* mode = preferredMode();
    if(mode != nullptr){
        this->setMode(mode);
    }

    // 3. 来自官方: 淡入示例
    LWeak<Output> weakRef {this};
    fadeInView.insertAfter(&server.layers()[OVERLAY_LAYER]);
    fadeInView.setOpacity(0.f);

    // 渲染一个长度为1s的动画, 每帧调用onUpdate这个Lambda
    LAnimation::oneShot(1000, [weakRef](LAnimation* anim){
        if(!weakRef){  //如果中途因为任何问题导致weakRef这个弱指针包装器变空了(比如, 屏幕被拔出), 则立刻停止动画
            anim->stop();
            return;
        }

        // 否则正常执行
        weakRef->fadeInView.setPos(weakRef->pos());
        weakRef->fadeInView.setSize(weakRef->size());
        weakRef->fadeInView.setOpacity(1.f - powf(anim->value(), 5.f));
        weakRef->repaint();
    },
    // 结束时, 将这个特殊的淡入动画view从场景中移出(unmap, 通过设置父节点为空指针)
    [weakRef](LAnimation*){
        if(weakRef){
            weakRef->fadeInView.setParent(nullptr);
        }
    });

    updateWallpaper();

    //TODO: 5.缩放

}

void Output::paintGL(){
    // 查找全屏窗口
    Surface* fullscreenSurface{ searchFullscreenSurface() };

    // 如果现在有全屏窗口, 则进入全屏渲染的逻辑
    // 使用Direct Scanout对于性能提升有很大帮助, 因为在全屏模式下,
    // 我们无需渲染其他的surface, 只需要渲染全屏的那个窗口即可。 
    if (fullscreenSurface){
        setContentType(fullscreenSurface->contentType());
        enableVSync(fullscreenSurface->preferVSync());
        enableFractionalOversampling(false);
        if(tryDirectScanout(fullscreenSurface))
            return;
    }else{
        setContentType(Louvre::LContentTypeNone);
        enableFractionalOversampling(true);
    }

    // 如果没有全屏窗口, 则进入正常渲染的流程
    TileyServer& server = TileyServer::getInstance();

    // 和wlroots的最大不同: 我们可以自己编写handlePaintGL函数来自己控制渲染过程。
    // 注意: 在这里传入this, 是指当前屏幕。在Louvre中, 每个屏幕都是一个对象, 因此我们需要让scene知道现在的屏幕是哪个
    // TODO: 在scene中判断屏幕, 分配不同的容器树根节点
    server.scene().handlePaintGL(this);

    for(LScreenshotRequest * req : screenshotRequests()){
        req->accept(true);
    }

    // 判断壁纸是否需要更新
    if (WallpaperManager::getInstance().wallpaperChanged()) {
        // 重置标志，避免重复更新
        // 在正确的渲染线程中执行更新操作
        updateWallpaper();
    }
};

void Output::moveGL(){
   TileyServer& server = TileyServer::getInstance();
   server.scene().handleMoveGL(this);

   updateWallpaper();
};

// resizeGL被设计成会在屏幕刚插入的时候也调用一次。
// 因此, 真实尺寸只能从这里面获取, 而不是initializeGL中.
void Output::resizeGL(){
    LLog::debug("[屏幕id: %u]屏幕状态发生改变", this->id());
    LLog::debug("[屏幕id: %u]接收到状态修改指令, 包含新的缩放: %f, 新的尺寸: %dx%d", 
                this->id(), this->scale(), this->size().w(), this->size().h());

    TileyServer& server = TileyServer::getInstance();
    
    server.scene().handleResizeGL(this);

    updateWallpaper();
};

void Output::uninitializeGL(){
    TileyServer& server = TileyServer::getInstance();
    server.scene().handleUninitializeGL(this);
};

void Output::setGammaRequest(LClient* client, const LGammaTable* gamma){
    L_UNUSED(client);   //只是一个标记, 表示某个参数未使用, 具体查看宏定义, 其实就是(void)client, 相比之前使用_作为未使用标记更加通用
    setGamma(gamma);
};

void Output::availableGeometryChanged(){
    LOutput::availableGeometryChanged();
};


bool Output::tryDirectScanout(Surface* surface) noexcept{
    return false;
}

// 查找全屏的surface是否存在
Surface* Output::searchFullscreenSurface() const noexcept{
    // 锁屏是最高优先级的
    if(sessionLockRole() && sessionLockRole()->surface()->mapped()){
        return static_cast<Surface*>(sessionLockRole()->surface());
    }

    // 如果没有锁屏, 则遍历toplevel, 同时满足:
    // 1. 在顶层这个Layer
    // 2. 已经mapped
    // 3. 申请过全屏并已经处于全屏
    // 4. 独占该屏幕(包括其他的, 顶栏之类的都被遮住), 可能处于全屏但不独占的情况是伪全屏。Hyprland实现了伪全屏。
    // 满足上述四个条件则判定为存在全屏的Surface
    for(auto it = compositor() -> layer(Louvre::LLayerTop).rbegin();it != compositor()->layer(LLayerTop).rend(); it++){
        if ((*it)->mapped() && (*it)->toplevel() && (*it)->toplevel()->fullscreen() && (*it)->toplevel()->exclusiveOutput() == this)
            return static_cast<Surface*>(*it);
    }

    return nullptr;
}

void Output::updateWallpaper(){
    WallpaperManager::getInstance().applyToOutput(this);
}