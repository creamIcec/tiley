#include <LLog.h>
#include <LCursor.h>
#include <LSurface.h>
#include <LOutput.h>
#include <LNamespaces.h>

#include "ToplevelRole.hpp"
#include "LToplevelRole.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/core/UserAction.hpp"

using namespace tiley;


ToplevelRole::ToplevelRole(const void *params) noexcept : LToplevelRole(params)
{
    moveSession().setOnBeforeUpdateCallback([](LToplevelMoveSession *session)
    {
        LMargins constraints { session->toplevel()->calculateConstraintsFromOutput(cursor()->output()) };

        if (constraints.bottom != LEdgeDisabled)
        {
            constraints.bottom += session->toplevel()->windowGeometry().size().h()
                + session->toplevel()->extraGeometry().top
                + session->toplevel()->extraGeometry().bottom
                - 0;   //- 50;  // 官方代码用的50, 这个地方是因为他的dock栏高度是50像素, 我们之后可以根据我们的dock栏具体情况来修改。最好从IPC/外部文件读取
        }

        // 移动范围限制 = 屏幕范围 - 底部50像素
        session->setConstraints(constraints);

        // 新增: 阻止客户端的默认只用按下左键就可以移动的行为, 这个函数是最合适的位置? 
        if(!TileyServer::getInstance().is_compositor_modifier_down){
            stopMoveSession(false);
        }
    });

    resizeSession().setOnBeforeUpdateCallback([](LToplevelResizeSession *session)
    {
        // 更改大小也限制在屏幕范围内
        LMargins constraints { session->toplevel()->calculateConstraintsFromOutput(cursor()->output()) };
        // 更改大小范围限制 = 屏幕范围
        session->setConstraints(constraints);

        // 新增: 阻止客户端的默认只用按下左键就可以移动的行为, 这个函数是最合适的位置? 
        if(!TileyServer::getInstance().is_compositor_modifier_down){
            stopResizeSession(false);
        }
    });
}

void ToplevelRole::printWindowAreaInfo(LToplevelRole* toplevel){

    if(!toplevel){
        LLog::log("目标窗口不存在, 无法显示信息。");
        return;
    }

    LLog::log("windowGeometry发生改变");
    LLog::log("额外的geometry: left: %d, top: %d, right: %d, bottom: %d", 
        toplevel->extraGeometry().left,
        toplevel->extraGeometry().top,
        toplevel->extraGeometry().right,
        toplevel->extraGeometry().bottom
    );
    LLog::log("window geometry: width: %d, height: %d, x: %d, y: %d, bottomRight: (%dx%d)",
        toplevel->windowGeometry().width(),
        toplevel->windowGeometry().height(),
        toplevel->windowGeometry().x(),
        toplevel->windowGeometry().y(),
        toplevel->windowGeometry().bottomRight().width(),
        toplevel->windowGeometry().bottomRight().height()
    );
}

void ToplevelRole::atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev){

    //LLog::log("窗口状态改变");
    LToplevelRole::atomsChanged(changes, prev);
    
    // 为了确保鼠标下面的窗口已经更新, 在接收到状态改变信号之后再设置活动容器
    Surface* surface = static_cast<Surface*>(seat()->pointer()->surfaceAt(cursor()->pos()));

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();

    if(!surface){
        LLog::log("鼠标位置没有窗口, 更新活动容器为空");
        manager.setActiveContainer(nullptr);
        return;
    }

    if(surface->toplevel()){

        if(changes.check(WindowGeometryChanged)){
            // 调试: 打印窗口大小信息
            // printWindowAreaInfo(surface->toplevel());
        }   

        ToplevelRole* window = surface->tl();

        if(!manager.isTiledWindow(window)){
            // 如果现在鼠标所在位置不是平铺窗口, 则不改变状态
            LLog::log("鼠标位置不是平铺窗口, 不更新活动容器");
            return;
        }

        if(window && window->container){
            manager.setActiveContainer(window->container);
            //LLog::log("已设置活动容器为状态改变后窗口下的容器");
        }
    }
};

void ToplevelRole::configureRequest(){
    LLog::log("接收到配置请求");

    LOutput *output { cursor()->output() };
 
    if (output)
    {
        surface()->sendOutputEnterEvent(output);
        configureBounds(
            output->availableGeometry().size()
            - LSize(extraGeometry().left + extraGeometry().right, extraGeometry().top + extraGeometry().bottom));
    }
    else
        configureBounds(0, 0);
 
    configureSize(0,0);
    configureState(pendingConfiguration().state | Activated);

    // 判断客户端是否支持服务端装饰模式
    //if (supportServerSideDecorations()){
        configureDecorationMode(ServerSide);
    //}

    configureCapabilities(WindowMenuCap | FullscreenCap | MaximizeCap | MinimizeCap);

    /*
        ToplevelRole* gimpWindow = nullptr;

        // 测试最大化CSD窗口, 以GIMP为例
        for(LSurface* lSurface : compositor()->surfaces()){
            if(lSurface->toplevel()){
                if(lSurface->toplevel()->appId().find("gimp") != std::variant_npos){
                    gimpWindow = static_cast<Surface*>(lSurface)->tl();
                    LLog::log("发现GIMP窗口");
                    break;
                }
            }
        }

        if(gimpWindow){
            const LRect& tileGeometry = gimpWindow->container->getGeometry();
            const LSize& actualSurfaceSize = gimpWindow->surface()->size();
            
        }
    */

    LLog::log("客户端偏好: %d", preferredDecorationMode());

}


// from https://gitlab.com/fakinasa/polowm/-/blob/master/src/roles/ToplevelRole.cpp
// 检查一个窗口有无大小限制, 如果有, 则加入浮动层
bool ToplevelRole::hasSizeRestrictions()
{
    return ( (minSize() != LSize{0, 0}) &&
             ((minSize().w() == maxSize().w()) || (minSize().h() == maxSize().h()))
           ) || (surface()->sizeB() == LSize{1, 1});
}

void ToplevelRole::assignToplevelType(){

    // TODO: 添加获取用户制定浮动的机制
    bool userFloat = false;

    // 如果有尺寸限制
    if(hasSizeRestrictions()){
        this->type = RESTRICTED_SIZE;
    // 如果是子窗口或者用户指定
    }else if(surface()->parent() || userFloat){
        this->type = FLOATING;
    // 否则就是普通窗口
    }else{
        this->type = NORMAL;
    }
}

void ToplevelRole::startMoveRequest(const LEvent& triggeringEvent){
    // TODO: 要不要在移动开始时关闭所有popup?
    LToplevelRole::startMoveRequest(triggeringEvent);
}

void ToplevelRole::startResizeRequest(const LEvent& triggeringEvent, LBitset<LEdge> edge){
    // TODO: 要不要在移动开始时关闭所有popup?
    LToplevelRole::startResizeRequest(triggeringEvent, edge);    
}