
#include "SurfaceView.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/interact/interact.hpp"
#include "src/lib/types.hpp"

#include <LPointerButtonEvent.h>
#include <LLog.h>
#include <LNamespaces.h>

using namespace tiley;

// 必须在SurfaceView的构造函数中就初始化对应的父亲层级关系
// 由于我们不知道surface何时提交到合成器, 如果在mappingChanged中才现场setParent的话太晚, 因为orderChanged比mappingChanged更早触发
// 在orderChanged里面又要对view进行排序。所以我们必须在orderChanged前(甚至所有surface相关提交操作之前)就把父级关系设置好

SurfaceView::SurfaceView(Surface* surface) noexcept : 
LSurfaceView((LSurface*) surface, &TileyServer::getInstance().layers()[APPLICATION_LAYER]){}

void SurfaceView::pointerButtonEvent(const LPointerButtonEvent &event){

    LLog::log("鼠标点击");

    L_UNUSED(event);

    Surface* surface = static_cast<Surface*>(this->surface());

    if(surface->toplevel()){
        LLog::log("点击窗口");
        focusWindow(surface);
    }
}

void SurfaceView::pointerEnterEvent (const LPointerEnterEvent &event){
    LLog::log("鼠标进入");
};