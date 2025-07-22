#include <LKeyboard.h>

#include "interact.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"

namespace tiley{
    bool focusWindow(Surface* surface){

        //对于我们的平铺式管理器而言, 需要做下面的事情:
        //1. 从上一个窗口失焦
        //2. 将该窗口置于顶层
        //3. 将键盘聚焦到该窗口
        //4. 平铺式特色: 将鼠标瞬移到该窗口(对于刚创建的窗口, 移动到中心)

        if (!surface || !surface->toplevel()) {
            return false; // 不处理非 toplevel 或还未创建 wrapperView 的 surface
        }

        TileyServer& server = TileyServer::getInstance();
        LSeat* seat = server.seat();

        // 1.
        Surface* prevSurface = static_cast<Surface*>(seat->keyboard()->focus());
        if(prevSurface == surface){
            // 如果就是之前那个(或者都为空), 不用再聚焦
            return false;
        }

        //2.3.
        seat->keyboard()->setFocus(surface);
        seat->pointer()->setFocus(surface);
        // 移动到当前它所在渲染层的最前端
        surface->raise();

        //4.
        //TODO: 如何瞬移鼠标?

        // 设置聚焦窗口
        //TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
        //manager.setFocusedContainer(surface->container);

        return true;
    }
}