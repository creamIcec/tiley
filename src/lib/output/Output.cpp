#include "Output.hpp"
#include <LLayout.h>
#include <LNamespaces.h>
#include <LWeak.h>
#include <LAnimation.h>
#include <cmath>

#include "LLog.h"
#include "LOutput.h"
#include "src/lib/TileyServer.hpp"

using namespace Louvre;
using namespace tiley;

void Output::initializeGL(){

    LLog::log("新的屏幕插入。"); 

    TileyServer& server = TileyServer::getInstance();

    server.scene().handleInitializeGL(this);

    // 来自官方: 淡入示例
    LWeak<Output> weakRef {this};
    fadeInView.insertAfter(&server.layers()[LLayerOverlay]);
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

}

void Output::paintGL(){
    LOutput::paintGL();
};

void Output::moveGL(){
   LOutput::moveGL();
};

void Output::resizeGL(){
    LOutput::resizeGL();
};

void Output::uninitializeGL(){
    LOutput::uninitializeGL();
};

void Output::setGammaRequest(LClient* client, const LGammaTable* gamma){
    LOutput::setGammaRequest(client, gamma);
};

void Output::availableGeometryChanged(){
    LOutput::availableGeometryChanged();
};