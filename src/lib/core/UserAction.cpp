#include "UserAction.hpp"

#include "LNamespaces.h"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/TileyWindowStateManager.hpp"

#include <LEvent.h>

void stopResizeSession(bool recalculateLayout){

    L_UNUSED(recalculateLayout);

    for (auto it = seat()->toplevelResizeSessions().begin(); it != seat()->toplevelResizeSessions().end();){
        if ((*it)->triggeringEvent().type() != LEvent::Type::Touch){
            it = (*it)->stop();
        }else{
            it++;
        }
    }
}

void stopMoveSession(bool recalculateLayout){
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    for (auto it = seat()->toplevelMoveSessions().begin(); it != seat()->toplevelMoveSessions().end();){
        if ((*it)->triggeringEvent().type() != LEvent::Type::Touch){
            ToplevelRole* window = static_cast<ToplevelRole*>((*it)->toplevel());
            if(recalculateLayout && window){
                // 下面是平铺层的处理: 如果是正常窗口, 并且没有被堆叠
                if(window->type == NORMAL && !manager.isStackedWindow(window)){
                    // 插入管理器
                    bool attached = manager.attachTile(window);
                    // TODO: 允许跨屏幕插入
                    if(attached){
                        // 如果插入成功, 重新组织并重新布局
                        manager.reapplyWindowState(window);
                        manager.reca
                        lculate();
                    }
                }
            }
            it = (*it)->stop();
        } else {
            it++;
        }
    }
}