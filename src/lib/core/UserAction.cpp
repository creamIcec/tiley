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
                // for normal window not stacked
                if(window->type == NORMAL && !manager.isStackedWindow(window)){
                    bool attached = manager.attachTile(window);
                    // TODO: Allow insert to another monitor
                    if(attached){
                        manager.reapplyWindowState(window);
                        manager.recalculate();
                    }
                }
            }
            it = (*it)->stop();
        } else {
            it++;
        }
    }
}