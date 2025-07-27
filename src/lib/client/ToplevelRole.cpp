#include <LLog.h>
#include <LCursor.h>
#include <LSurface.h>
#include <LOutput.h>
#include <LNamespaces.h>

#include "ToplevelRole.hpp"
#include "LToplevelRole.h"
#include "src/lib/TileyWindowStateManager.hpp"

using namespace tiley;

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
    configureDecorationMode(ServerSide);
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