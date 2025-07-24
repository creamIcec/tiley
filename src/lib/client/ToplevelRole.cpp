#include <LLog.h>
#include <LCursor.h>
#include <LSurface.h>
#include <LOutput.h>
#include <LNamespaces.h>

#include "ToplevelRole.hpp"

using namespace tiley;

void ToplevelRole::atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev){
    LToplevelRole::atomsChanged(changes, prev);
    LLog::log("装饰模式: %u", decorationMode());
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

// 根据官方文档, 这个事件由客户端触发。我们可以自己触发嘛?
void ToplevelRole::startMoveRequest(const LEvent& triggeringEvent){
    LToplevelRole::startMoveRequest(triggeringEvent);
}