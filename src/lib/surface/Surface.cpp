#include "Surface.hpp"
#include "LNamespaces.h"
#include "LSurface.h"
#include "LToplevelRole.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/interact/interact.hpp"

#include "src/lib/surface/SurfaceView.hpp"
#include "src/lib/types.hpp"

#include <LLog.h>
#include <LCursor.h>
#include <LSubsurfaceRole.h>
#include <LClient.h>
namespace tiley{
    struct NodeContainer;
}

using namespace tiley;

Surface::Surface(const void* params) noexcept : LSurface(params){
    // 不做任何事情。此时我们的Surface还没有分配任何属性。
}

// 调用路径: roleChanged() -> orderChanged -> ((如果是窗口) -> configureRequest() -> atomsChanged()) -> mappingChanged 
// 首先分配一个角色, 再组装顺序(窗口和他的子Surface之间), 之后判断: 如果一个Surface是窗口, 则触发配置过程。配置的结果可以由atomsChanged接收。配置完成后, 客户端开始显示: 调用MappingChanged, 随后就是显示之后自己的工作了。 
// 如果不是窗口, 则在组装顺序后直接MappingChanged.

void Surface::mappingChanged(){
    LLog::log("%d: mappingChanged", this);
    LSurface::mappingChanged();
}

void Surface::roleChanged(LBaseSurfaceRole *prevRole){
    LLog::log("%d: roleChanged", this);
    LSurface::roleChanged(prevRole);
    // 刚分配时prev是null
    if(!prevRole){
        LLog::log("刚分配role");
    }

    // 如果新的角色是鼠标指针的话, 则隐藏掉, 因为我们本来就有硬件指针。(防御性编程)
    if(cursorRole()){
        view.setVisible(false);
    }
}

void Surface::layerChanged(){
    LLog::log("%d: layerChanged", this);
    TileyServer& server = TileyServer::getInstance();
    view.setParent(&server.layers()[layer()]);
}

void Surface::orderChanged(){
    LLog::log("%d: orderChanged", this);
    LSurface::orderChanged();
    if(toplevel()){
        LLog::log("是窗口");
        TileyServer& server = TileyServer::getInstance();
        view.setParent(&server.layers()[TILED_LAYER]);
    }else{
        LLog::log("是窗口中的子surface");
        Surface* prev = static_cast<Surface*>(prevSurface());
        view.insertAfter(prev && (prev->layer() == layer()) ? &prev->view : nullptr);
    }
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}


/*

Surface::Surface(const void* params) noexcept : LSurface(params){
    
}



void Surface::mappingChanged(){

    compositor()->repaintAllOutputs();
 
    LOutput *activeOutput { cursor()->output() };
 
    if (!activeOutput)
        return;
 
    // TODO: 暂时放在屏幕中心
    if (mapped() && toplevel())
    {
        const LSize size {
            toplevel()->windowGeometry().size()
        };  //总共(包括标题栏等的一共的大小)
 
        // 显示屏在布局中的位置 + 可用区域相对该显示屏左上角的位置 = 可以用来显示的矩形区域左上角
        const LSize availGeoPos { activeOutput->pos() + activeOutput->availableGeometry().pos() };
 
        // 设置到中间
        setPos(availGeoPos + (activeOutput->availableGeometry().size() - size) / 2);
        
        // 为什么可能被不可用区域挡住?
        if (pos().y() < availGeoPos.y())
            setY(availGeoPos.y());

        if(toplevel()->decorationMode() != LToplevelRole::ServerSide){
            LLog::log("不是服务端装饰");
            toplevel()->configureSize(size);
            toplevel()->configureDecorationMode(LToplevelRole::ServerSide);
            toplevel()->configureRequest();
        }

        LLog::log("Window geometry: %dx%d", 
              toplevel()->windowGeometry().w(), 
              toplevel()->windowGeometry().h());
        LLog::log("Extra geometry (top, left, bottom, right): %d, %d, %d, %d",
              toplevel()->extraGeometry().top, toplevel()->extraGeometry().left,
              toplevel()->extraGeometry().bottom, toplevel()->extraGeometry().right);
    }

    // 干下面的事:
    // 1. 确保是一个toplevel(窗口)
    // 2. 使用户聚焦到该窗口
    // 3. 显示窗口

    // 1.
    if(toplevel()){
        if(mapped()){
            // 修改默认行为: 从默认相对于父view位置分离成绝对位置。对于窗口而言相当有用。
            view.enableParentOffset(false);
            // 2.
            // 默认插入平铺层
            TileyServer& server = TileyServer::getInstance();
            view.insertAfter(&server.layers()[TILED_LAYER]);
            focusWindow(this);

            // 3.
            LLog::log("显示窗口");
            view.setVisible(true);

        }else{
            LLog::log("隐藏窗口");
            view.setVisible(false);
        }
    }
}

void Surface::mappingChanged(){
    LLog::log("Surface映射变化:");
    LLog::log("  - 是toplevel: %d", toplevel() != nullptr);
    LLog::log("  - 是subsurface: %d", subsurface() != nullptr);
    LLog::log("  - 有parent: %d", parent() != nullptr);
    LLog::log("  - mapped: %d", mapped());
    
    if (subsurface()) {
        LLog::log("  - subsurface parent: %d", subsurface()->client());
    }
    LSurface::mappingChanged();

    TileyServer& server = TileyServer::getInstance();
    view.setParent(&server.layers()[TILED_LAYER]);  //放到平铺层
}

void Surface::roleChanged(LBaseSurfaceRole *prevRole){

    LLog::log("surface的身份发生改变, 是窗口? %d", toplevel() != nullptr);
    LSurface::roleChanged(prevRole);

    if(toplevel()){
        LLog::log("前驱surface是否存在? %d", prevSurface() != nullptr); // 窗口有前驱??
        LLog::log("是否属于某一层? %d", parent() != nullptr);  // 初始时没有父视图
    }

    if (cursorRole())
        view.setVisible(false);
}

// 这个函数只在创建窗口时调用了一次，并且不是toplevel那个surface调用的
// 需要研究
void Surface::layerChanged(){
    LLog::log("surface显示层次发生改变, 是toplevel? %d", toplevel() != nullptr);
    TileyServer& server = TileyServer::getInstance();
    view.(&server.layers()[TILED_LAYER]);
    repaintOutputs();
}

void Surface::orderChanged(){
    LLog::log("surface组内显示顺序发生改变");
    Surface* prev = (Surface*)prevSurface();
    view.insertAfter((prev && prev->layer() == layer()) ? &prev->view : nullptr);
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}

*/