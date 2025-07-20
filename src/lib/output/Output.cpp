#include "Output.hpp"
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

#include "LContentType.h"
#include "LLog.h"

#include "src/lib/TileyServer.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

using namespace Louvre;
using namespace tiley;

Output::Output(const void* params) noexcept : LOutput(params){

    // 初始化一个显示屏时, 干下面的事:
    // 1. 初始化壁纸

    // LRegion是一个工具类, 用于提供一块"画布区域", 在这个区域中可以添加很多个矩形, 并可对它们进行布尔运算。
    const LRegion region;  //为壁纸分配一块区域, 使得壁纸在渲染中被考虑进去
    this->wallpaperView.setTranslucentRegion(&region);
} 

void Output::initializeGL(){

    LLog::log("新的屏幕插入, id: %u 。", this->id());

    TileyServer& server = TileyServer::getInstance();

    LLog::log("[屏幕id: %u] 初始化要在该屏幕上渲染的窗口。", this->id());

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
};

void Output::moveGL(){
   TileyServer& server = TileyServer::getInstance();
   server.scene().handleMoveGL(this);
};

// resizeGL被设计成会在屏幕刚插入的时候也调用一次。
// 因此, 真实尺寸只能从这里面获取, 而不是initializeGL中.
void Output::resizeGL(){
    LLog::log("[屏幕id: %u]屏幕状态发生改变", this->id());
    LLog::log("[屏幕id: %u]接收到状态修改指令, 包含新的缩放: %f, 新的尺寸: %dx%d", 
                this->id(), this->scale(), this->size().w(), this->size().h());

    TileyServer& server = TileyServer::getInstance();

    /*
    auto layers = server.layers();    
    
    LLog::log("[屏幕id: %u]更新")

    // TODO: 暂时假设只有一个屏幕, 所以这么设置, 之后需要改成所有屏幕加起来的大小。
    for(int i = 0; i <= BACKGROUND_LAYER; i++){
        layers[i].setSize(size());
        LLog::log("层[%d]的尺寸: %dx%d", i, layers[i].size().w(), layers[i].size().h());
    }
    */
    
    server.scene().handleResizeGL(this);

    LLog::log("[屏幕id: %u] 更新壁纸。", this->id());

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
    return false;  //TODO
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
    // 如果已经加载了纹理...
    if(wallpaperView.texture()){
        if(wallpaperView.texture()->sizeB() == this->sizeB()){  //..并且纹理绝对尺寸和屏幕尺寸一样...
            wallpaperView.setBufferScale(scale()); //...则设置相对尺寸缩放为屏幕缩放...
            wallpaperView.setPos(pos());  //...并且和屏幕左上角对齐.
            return;  //就结束了。
        }

        delete wallpaperView.texture();  //到了这里, 说明没有复用之前的纹理, 说明需要重新配置, 先释放内存。
    }
    // 假设尺寸等不一样, 就需要先将绝对尺寸转换成屏幕尺寸, 再设置缩放。而转换的算法有很多种, 是我们平时在裁剪图像(例如photoshop)时经常遇到的, 例如:
    // 平铺, 拉伸, 填充等等

    using std::filesystem::path;

    path wallpaperRootPath("/home/iriseplos/tiley-data/wallpaper");
    path wallpaperPath = wallpaperRootPath / "visualbg_station.png";

    LTexture* originalWallpaper {LOpenGL::loadTexture(wallpaperPath)};

    if(!originalWallpaper){
        LLog::error("[屏幕id: %u]无法加载壁纸。请检查路径是否存在。", this->id());
        return;
    }

    // 下面就是具体的计算逻辑
    // 注意: 所有计算都在buffer绝对坐标下, 最终再缩放到屏幕缩放大小。
    const LSize& originalSizeB {originalWallpaper->sizeB()};
    const LSize& outputSizeB {sizeB()};

    // 一块矩形"蒙版", 就是我们最后采用的区域
    LRect srcRect {0};

    // 优先满足屏幕高度, 然后计算在将图片高度拉伸到屏幕高度、并且保持比例的前提下, 对应的宽度是多少。
    const Float32 ratio = outputSizeB.h() / (Float32)originalSizeB.h();
    const Float32 scaledWidth = originalSizeB.w() * ratio;

    // 根据是否比原来的尺寸大决定
    // 如果高度适应后, 宽度比原来大, 说明图片在左右两侧会留下黑边, 则我们需要裁剪图片的上下部分, 让图片得以继续放大, 适应屏幕
    if(scaledWidth >= originalSizeB.w()){
        srcRect.setW(originalSizeB.w());  //满足宽度, 裁剪上下: 取中间的区域, 分为两步:
        // 第一步: 将蒙版框的高度按照宽度比例缩放
        srcRect.setH(outputSizeB.h() * originalSizeB.w() / (Float32)outputSizeB.w());
        // 第二步: 将蒙版框在图片中居中
        srcRect.setY((originalSizeB.h() - srcRect.h()) / 2.0f);
    }else{
        // 反之, 则说明会在上下留下黑边, 需裁剪左右部分。
        srcRect.setH(originalSizeB.h());
        srcRect.setW(outputSizeB.w() * originalSizeB.h()  / (Float32)outputSizeB.h());
        srcRect.setX((originalSizeB.w() - srcRect.w()) / 2.0f);
    }

    // 得到需要的区域之后, 设置buffer绝对大小
    wallpaperView.setTexture(originalWallpaper->copy(sizeB(), srcRect));  
    wallpaperView.setBufferScale(scale()); //按照屏幕缩放比例进行缩放

    delete originalWallpaper; // 由于copy创建了一个新的对象, 之前的可以释放
    wallpaperView.setPos(pos());  //和屏幕左上角位置对齐
    wallpaperView.setDstSize(outputSizeB);

    LLog::log("壁纸view的尺寸: %dx%d", wallpaperView.size().w(), wallpaperView.size().h());
    LLog::log("壁纸view的本地尺寸: %dx%d", wallpaperView.nativeSize().w(), wallpaperView.nativeSize().h());
    LLog::log("壁纸view的父View(BackgroundLayerView)尺寸: %dx%d",wallpaperView.parent()->size().w(), wallpaperView.parent()->size().h());
    LLog::log("壁纸view的父View(BackgroundLayerView)本地尺寸: %dx%d",wallpaperView.parent()->nativeSize().w(), wallpaperView.parent()->nativeSize().h());


    //到这里我们就完成了壁纸的加载

}