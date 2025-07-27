#include "Surface.hpp"

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

#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/views/SurfaceView.hpp"
#include "src/lib/types.hpp"


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

Surface::Surface(const void *params) : LSurface(params) {
    view = std::make_unique<SurfaceView>(this);
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
        LLog::log("打开了一个弹出菜单");
        getView()->setParent(&server.layers()[LLayerOverlay]);
    }
}

// orderChanged: surface的顺序发生变化。我们需要据此对应调整view的顺序。
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
void Surface::orderChanged()
{   
    //LLog::log("顺序改变, surface地址: %d, 层次: %d", this, layer());
    
    // 调试: 打印surface前后关系
    // TileyServer& server = TileyServer::getInstance();
    // server.compositor()->printToplevelSurfaceLinklist();
    
    // Previous surface in LCompositor::surfaces()
    Surface *prev { static_cast<Surface*>(prevSurface()) };

    // Re-insert the view only if there is a previous surface within the same layer
    getView()->insertAfter((prev && prev->layer() == layer()) ? prev->getView() : nullptr);

}

// layerChanged: 层次发生变化。对于我们来讲最有用的就是平铺层<->浮动层之间的互相切换。
// 只需将toplevel的view移动到新的层即可, 它的弹出窗口和它的subsurface会自动跟随。
void Surface::layerChanged(){
    //LLog::log("%d: layerChanged", this);
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
            LLog::log("设置上一个活动容器为新打开的窗口容器");
            // 重新计算布局
            manager.recalculate();
        }

    } else {
        // 如果是关闭(隐藏)了窗口
        if(toplevel()){
            // 准备移除
            Container* removedContainer = nullptr;
            // 移除窗口(包括平铺的和非平铺的都是这个方法)
            manager.removeWindow(tl(), removedContainer);
            // 如果移除成功
            if(removedContainer != nullptr){
                // 重新布局
                manager.recalculate();
            }
        }

        // 最后, 无论是什么surface, 都将view从parent的列表中移除, 销毁view
        getView()->setParent(nullptr);    
    }
    
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}