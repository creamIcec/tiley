#include "Surface.hpp"

#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/interact/interact.hpp"
#include "src/lib/surface/SurfaceView.hpp"
#include "src/lib/types.hpp"

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

#include <memory>

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


LView* Surface::getView() noexcept{
    // TODO: 带服务端装饰的view
    return &view;
}

Surface::Surface(const void* params) noexcept : LSurface(params){
    // 不做任何事情。此时我们的Surface还没有分配任何属性。
}

// 调用路径: roleChanged() -> orderChanged -> ((如果是窗口) -> configureRequest() -> atomsChanged()) -> mappingChanged 
// 首先分配一个角色, 再组装顺序(窗口和他的子Surface之间), 之后判断: 如果一个Surface是窗口, 则触发配置过程。配置的结果可以由atomsChanged接收。配置完成后, 客户端开始显示: 调用MappingChanged, 随后就是显示之后自己的工作了。 
// 如果不是窗口, 则在组装顺序后直接MappingChanged.

// 在这里开始我们的平铺逻辑
// 当一个表面要显示时, 我们按照这个表面的view是否属于同一个窗口, 放进同一个Layer容器中, 代表整个窗口
void Surface::mappingChanged(){
    //LLog::log("%d: mappingChanged", this);
    compositor()->repaintAllOutputs();

    LOutput *activeOutput { cursor()->output() };

    TileyServer& server = TileyServer::getInstance();

    if (!activeOutput)
        return;

    if (mapped()){
        if(toplevel()){            
            if(!wrapperView){  //防御性: 不一定先执行, 需要判断是否已经存在
                // 创建包装器
                wrapperView = std::make_unique<LLayerView>();
                // TODO: 包装器的父容器先设置成这一层(方便调试)。后面需要设置成包装它的容器(还要包装一层用于布局)
                wrapperView->setParent(&server.layers()[TILED_LAYER]);
            }
            view.setParent(wrapperView.get());
            
            seat()->keyboard()->setFocus(this);
            toplevel()->configureState(LToplevelRole::Activated);

        }else{
            auto parentSurface = static_cast<Surface*>(topmostParent());
            if(parentSurface && parentSurface->toplevel()){
                // 设置成和他属于同一个窗口的toplevel一样的wrapperView, 确保一个窗口内的surface都在一个容器内。
                view.setParent(parentSurface->wrapperView.get());
            }
        }
    }
}

void Surface::roleChanged(LBaseSurfaceRole *prevRole){
    //LLog::log("%d: roleChanged", this);
    LSurface::roleChanged(prevRole);
    // 如果新的角色是鼠标指针的话, 则隐藏掉, 因为我们本来就有硬件指针。(防御性编程)
    if(cursorRole()){
        view.setVisible(false);
        return;
    }
}

// 层次发生变化
// 只需将toplevel的包装view移动到新的层即可
void Surface::layerChanged(){
    //LLog::log("%d: layerChanged", this);
    TileyServer& server = TileyServer::getInstance();
    if(toplevel()){
        wrapperView->setParent(&server.layers()[layer()]);
    }
}


// 设想我们在洗牌, 屏幕上显示的是现在应该是什么牌序, 我们需要将手上的牌洗成那个顺序
// 显示牌序: 5 1 2 4 3
// 手上牌序: 5 1 2 4 3
// 显示牌序变了: 1 2 3 4 5
// 洗牌
// 1. 对于每张牌, 找到它新的显示牌序中的离它最近前一张m(例如1在2之前, 3在4之前)
// 2. 将这张牌放到m之后即可
// 3. 最后洗出来的保证符合显示牌序
// 5 1 2 4 3 -> 选中5 -> 放到4之后 -> 1 2 4 5 3 -> 选中1 -> 放到nullptr之后 -> 1 2 4 5 3
// -> 选中2 -> 放到1之后 -> 1 2 4 5 3 -> 选中4 -> 放到3之后 -> 1 2 5 3 4 -> 选中5 -> 放到4之后 -> 1 2 3 4 5
// 虽然每次调整顺序都会出发一次全部牌的orderChanged(会再全部排一次), 不过算是面向对象方便人编写的一个副作用吧
// 直到最后排好序, 下一次发现没有变化了, 才会停止触发。
void Surface::orderChanged(){
    //LLog::log("%d: orderChanged", this);
    LSurface::orderChanged();
    // 找到一个在我前面，并且其视图和我视图拥有同一个父亲的兄弟 (针对窗口内)
    LView* prevBrotherView = nullptr;
    LSurface *prevSurf = prevSurface();

    while (prevSurf) {
        // prevSurf的视图的父亲, 必须和我的视图的父亲是同一个
        if (((Surface*)prevSurf)->view.parent() == view.parent()) {
            prevBrotherView = ((Surface*)prevSurf)->getView();
            break; // 找到了, 停止搜索
        }
        prevSurf = prevSurf->prevSurface();
    }

    // 将我的视图，插入到那个兄弟视图的后面。
    // 如果没找到兄弟(我是第一个), prevBrotherView 就是 nullptr,
    // insertAfter(nullptr) 会把我放到最前面
    view.insertAfter(prevBrotherView);
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}