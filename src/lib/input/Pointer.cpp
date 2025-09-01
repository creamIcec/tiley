#include "Pointer.hpp"

#include "LEdge.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/UserAction.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"
#include "src/lib/core/Container.hpp"

#include <LNamespaces.h>
#include <LLog.h>
#include <LSeat.h>
#include <LPointer.h>
#include <LView.h>
#include <LKeyboard.h>
#include <LSessionLockManager.h>
#include <LDNDSession.h>
#include <LCursor.h>
#include <LClient.h>
#include <LCompositor.h>
#include <LPointerButtonEvent.h>
#include <LToplevelRole.h>
#include <xkbcommon/xkbcommon.h>

#include <LToplevelMoveSession.h>

#include <private/LCompositorPrivate.h>

using namespace tiley;

LSurface* Pointer::surfaceAtWithFilter(const LPoint& point, const std::function<bool (LSurface*)> &filter){
    retry:

    compositor()->imp()->surfacesListChanged = false;

    // check by wayland surface order
    for (auto it = compositor()->surfaces().rbegin(); it != compositor()->surfaces().rend(); ++it)
    {
        LSurface *s = *it;

        // check1: The surface must be visible by user
        if (s->mapped() && !s->minimized())
        {
            // check2: The point is inside target region of the surface
            if (s->inputRegion().containsPoint(point - s->rolePos()))
            {
                // check3: The filter function must return true
                if (filter(s))
                {
                    // viola
                    return s;
                }
                // if one of the above conditions not meet, loop continue to next surface
            }
        }

        // if surface list are mutated when looping through, try again
        if (compositor()->imp()->surfacesListChanged)
            goto retry;
    }

    // failed after all attempts
    return nullptr;
}

void Pointer::printPointerPressedSurfaceDebugInfo(){

    TileyServer& server = TileyServer::getInstance();
    Surface* surface = static_cast<Surface*>(surfaceAt(cursor()->pos()));

    LLog::log("*****************Surface Clicked Debug Info********************");
    LLog::log("Clicked surface: %d", surface);
    LLog::log("Surface layer: %d", surface->layer());
    LLog::log("Address of BACKGROUND_VIEW layer object: %d", &server.scene().layers[0]);
    LLog::log("Address of TILED_VIEW layer object: %d", &server.scene().layers[1]);
    LLog::log("Address of FLOATING_VIEW layer object: %d", &server.scene().layers[2]);
    LLog::log("Address of LOCKSCREEN_VIEW layer object: %d", &server.scene().layers[3]);
    LLog::log("Address of OVERLAY_VIEW layer object: %d", &server.scene().layers[4]);
    LLog::log("Address of parent SurfaceView: %d", surface->getView()->parent());

    if(surface->toplevel()){
        LLog::log("Role of Surface: toplevel");
    }else if(surface->subsurface()){
        LLog::log("Role of Surface: subsurface");
    }

    LLog::log("*****************/Surface Clicked Debug Info********************");
}

void Pointer::pointerButtonEvent(const LPointerButtonEvent& event){

    TileyServer& server = TileyServer::getInstance();
    Surface* surface = static_cast<Surface*>(surfaceAt(cursor()->pos()));

    // Debug: print clicked surface debug info
    // printPointerPressedSurfaceDebugInfo();

    bool compositorProceed = false;

    // if keyboard available
    if(seat()->keyboard()){
        compositorProceed = processCompositorKeybind(event);
    }

    // if tiley has already proceed the event, stop propagating
    if(!compositorProceed){
        processPointerButtonEvent(event);
    }

}

void Pointer::processPointerButtonEvent(const LPointerButtonEvent& event){

    // TODO: Event queue. Use a queue to cache all instant pointer events to avoid processing too frequently
    // TODO: Tweaking copied code(below) from default implementation

    const bool sessionLocked { sessionLockManager()->state() != LSessionLockManager::Unlocked };
    const bool activeDND { seat()->dnd()->dragging() && seat()->dnd()->triggeringEvent().type() != LEvent::Type::Touch };
 
    if (activeDND)
    {
        if (event.state() == LPointerButtonEvent::Released &&
            event.button() == LPointerButtonEvent::Left)

            seat()->dnd()->drop();
 
        seat()->keyboard()->setFocus(nullptr);
        // reset focus to nothing
        setFocus(nullptr);
        setDraggingSurface(nullptr);
        return;
    }
 
    // all keys should trigger a public processing logic
    if (!focus()){

        LSurface *surface { surfaceAt(cursor()->pos()) };
        if (surface){

            if (sessionLocked && surface->client() != sessionLockManager()->client())
                return;
 
            cursor()->setCursor(surface->client()->lastCursorRequest());

            seat()->keyboard()->setFocus(surface);
            setFocus(surface);
            // propagate the event to clients
            sendButtonEvent(event);

            if (!surface->popup() && !surface->isPopupSubchild())
                seat()->dismissPopups();
        } else {

            seat()->keyboard()->setFocus(nullptr);
            seat()->dismissPopups();
        
        }
 
        return;
    }
 
    // logic for particular button
    if (event.button() != LPointerButtonEvent::Left)
    {
        sendButtonEvent(event);
        return;
    }

    if (event.state() == LPointerButtonEvent::Pressed)
    {
        // Keep a ref to continue sending it events after the cursor
        // leaves, if the left button remains pressed
        setDraggingSurface(focus());
 
        // Most apps close popups when they get keyboard focus,
        // probably because the parent loses it

        if (!focus()->popup() && !focus()->isPopupSubchild())
        {
            
            seat()->keyboard()->setFocus(focus());
 
            // Pointer focus may have changed within LKeyboard::focusChanged()
            if (!focus())
                return;
        }
 
        sendButtonEvent(event);
 
        if (focus()->toplevel() && !focus()->toplevel()->activated()){
            LLog::debug("[Pointer::processPointerButtonEvent]: Address of activated toplevel: %d", focus()->toplevel());
            focus()->toplevel()->configureState(focus()->toplevel()->pendingConfiguration().state | LToplevelRole::Activated);
        }

        // exclude popups
        if (!focus()->popup() && !focus()->isPopupSubchild())
            seat()->dismissPopups();
        
        if (!focus() || focus() == compositor()->surfaces().back())
            return;
        
        // determine which one to be raised (self or parent)
        if (focus()->parent())
            focus()->topmostParent()->raise();
        else
            focus()->raise();
    }
    // Left button released
    else
    {
        sendButtonEvent(event);
 
        // Stop pointer toplevel resizing sessions
        for (auto it = seat()->toplevelResizeSessions().begin(); it != seat()->toplevelResizeSessions().end();)
        {
            if ((*it)->triggeringEvent().type() != LEvent::Type::Touch)
                it = (*it)->stop();
            else
                it++;
        }
 
        // Stop pointer toplevel moving sessions

        for (auto it = seat()->toplevelMoveSessions().begin(); it != seat()->toplevelMoveSessions().end();)
        {
            if ((*it)->triggeringEvent().type() != LEvent::Type::Touch)
                it = (*it)->stop();
            else
                it++;
        }
 
        // We stop sending events to the surface on which the left button was being held down
        setDraggingSurface(nullptr);
 
        // if focus is not locked (e.g. by fullscreen games) and outside of input region
        if (!focus()->pointerConstraintEnabled() && !focus()->inputRegion().containsPoint(cursor()->pos() - focus()->rolePos())){
            setFocus(nullptr);
            cursor()->useDefault();
            cursor()->setVisible(true);
        }
    }
}

bool Pointer::processCompositorKeybind(const LPointerButtonEvent& event){

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();

    // TODO: Tweaking copied code(below) from default implementation

    if(!focus() || !focus()->toplevel()){
        LLog::debug("[Pointer::processCompositorKeybind]: No pointer focus or focus is not a window, stop handling.");
        return false;
    }

    ToplevelRole* window = static_cast<ToplevelRole*>(focus()->toplevel());

    bool isResizingWindow = !seat()->toplevelResizeSessions().empty();
    // TODO: In nested mode use 'alt' as modifier and 'meta' in tty mode
    bool isModifierPressedDown = TileyServer::getInstance().is_compositor_modifier_down;

    // Right + Pressed + Not resizing before
    if(event.button() == Louvre::LPointerButtonEvent::Right &&
       event.state() == Louvre::LPointerButtonEvent::Pressed &&
       isModifierPressedDown &&
       !isResizingWindow){
        LLog::debug("[Pointer::processCompostiorKeybind]: Resizing window...");
        
        const LPoint &mousePos = cursor()->pos();
        const LPoint &winPos = window->surface()->pos();
        const LSize &winSize = window->surface()->size();

        // relative position of cursor to window top-left corner 
        float relativeX = (float)(mousePos.x() - winPos.x()) / (float)winSize.w();
        float relativeY = (float)(mousePos.y() - winPos.y()) / (float)winSize.h();

        // Bitset '0000' to storage resizing edge
        LBitset<LEdge> edge = LEdgeNone;

        if (relativeY < 0.5f) {
            edge |= LEdgeTop;
        } else if (relativeY >= 0.5f) {
            edge |= LEdgeBottom;
        }

        if (relativeX < 0.5f) {
            edge |= LEdgeLeft;
        } else if (relativeX >= 0.5f) {
            edge |= LEdgeRight;
        }

        if (edge != LEdgeNone){
            manager.setupResizeSession(window, edge, cursor()->pos());
            window->startResizeRequest(event, edge);
        }

        if(!window->container){
            return true;
        }

        return true;

    }

    // Right + Pressed + Already in a session
    if(event.button() == Louvre::LPointerButtonEvent::Right &&
       event.state() == Louvre::LPointerButtonEvent::Released &&
       isResizingWindow){
        LLog::debug("[Pointer::processCompositorKeybind]: Stop resizing window...");
        stopResizeSession(true);
        return true;
    }

    bool isMovingWindow = !seat()->toplevelMoveSessions().empty();

    // Left + Pressed + Not moving before
    if(event.button() == Louvre::LPointerButtonEvent::Left &&
        event.state() == Louvre::LPointerButtonEvent::Pressed &&
        isModifierPressedDown &&
        !isMovingWindow){
        
        LLog::debug("[Pointer::processCompositorKeybind]: Moving window");
        
        window->startMoveRequest(event);

        if(!window->container){
            return true;
        }

        // tiled windows and not in stacking mode
        if(window->type == NORMAL && !manager.isStackedWindow(window)){
            Container* detachedContainer = manager.detachTile(window, MOVING);
            if(detachedContainer){
                manager.reapplyWindowState(window);
                manager.recalculate();
            }
        }

        return true;
    }

    // Left + Released + Already in a moving session
    if(event.button() == Louvre::LPointerButtonEvent::Left &&
       event.state() == Louvre::LPointerButtonEvent::Released &&
       isMovingWindow){

        stopMoveSession(true);
        return true;
            
    }

    return false;
}

void Pointer::pointerMoveEvent(const LPointerMoveEvent& event){

    // Update the cursor position
    cursor()->move(event.delta().x(), event.delta().y());
 
    bool pointerConstrained { false };

    if (focus())
    {
        LPointF fpos { focus()->rolePos() };
        
        // Attempt to enable the pointer constraint mode if the cursor is within the constrained region.
        if (focus()->pointerConstraintMode() != LSurface::PointerConstraintMode::Free)
        {
            if (focus()->pointerConstraintRegion().containsPoint(cursor()->pos() - fpos))
                focus()->enablePointerConstraint(true);
        }
 
        if (focus()->pointerConstraintEnabled())
        {
            if (focus()->pointerConstraintMode() == LSurface::PointerConstraintMode::Lock)
            {
                if (focus()->lockedPointerPosHint().x() >= 0.f)
                    cursor()->setPos(fpos + focus()->lockedPointerPosHint());
                else
                {
                    // "jump" to starting point
                    cursor()->move(-event.delta().x(), -event.delta().y());
                    
                    const LPointF closestPoint {
                        focus()->pointerConstraintRegion().closestPointFrom(cursor()->pos() - fpos)
                    };
 
                    cursor()->setPos(fpos + closestPoint);
                }
            }
            else /* Confined */
            {
                const LPointF closestPoint {
                    focus()->pointerConstraintRegion().closestPointFrom(cursor()->pos() - fpos)
                };
 
                cursor()->setPos(fpos + closestPoint);
            }
            
            // mark the pointer is constrained
            pointerConstrained = true;
        }
    }
 
    // Schedule repaint on outputs that intersect with the cursor where hardware composition is not supported.
    cursor()->repaintOutputs(true);
 
    const bool sessionLocked { compositor()->sessionLockManager()->state() != LSessionLockManager::Unlocked };
    const bool activeDND { seat()->dnd()->dragging() && seat()->dnd()->triggeringEvent().type() != LEvent::Type::Touch };

    // is dragging an icon
    if (activeDND)
    {
        if (seat()->dnd()->icon())
        {
            seat()->dnd()->icon()->surface()->setPos(cursor()->pos());
            seat()->dnd()->icon()->surface()->repaintOutputs();
            cursor()->setCursor(seat()->dnd()->icon()->surface()->client()->lastCursorRequest());
        }
 
        // DND is prior to all other pointer activities
        // so keep the environment clear
        seat()->keyboard()->setFocus(nullptr);
        setDraggingSurface(nullptr);
        setFocus(nullptr);
    }

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    LSurface* targetInsertWindowSurface = surfaceAtWithFilter(cursor()->pos(), [&manager](LSurface* surface){
        
        // check1: must be window role
        if(!surface->toplevel()){
            return false;
        }

        ToplevelRole* window = static_cast<ToplevelRole*>(surface->toplevel());

        // check2 : must be tiled
        if(!manager.isTiledWindow(window)){
            return false;
        }

        return true;
    });

    Surface* _targetInsertWindowSurface = static_cast<Surface*>(targetInsertWindowSurface);
    if(_targetInsertWindowSurface && manager.activatedContainer() != _targetInsertWindowSurface->tl()->container){
        manager.setActiveContainer(_targetInsertWindowSurface->tl()->container);
        LLog::debug("[Pointer::pointerMoveEvent]: Update active container to pointer focused container");
    }

    bool activeResizing { false };
 
    for (LToplevelResizeSession *session : seat()->toplevelResizeSessions())
    {

        if (session->triggeringEvent().type() != LEvent::Type::Touch)
        {
            
            if(!TileyServer::getInstance().is_compositor_modifier_down){
                break;
            }

            activeResizing = true;

            ToplevelRole* targetWindow = static_cast<ToplevelRole*>(session->toplevel());

            bool isResizingTiledWindow = targetWindow && targetWindow->container && targetWindow->type == NORMAL && manager.isTiledWindow(targetWindow);
            
            if(isResizingTiledWindow){
                LLog::debug("[Pointer::pointerMoveEvent]: Resizing size of tiled window");
                if(manager.resizeTile(cursor()->pos())){
                    manager.reapplyWindowState(targetWindow);
                    manager.recalculate();
                }
            }else{
                // just update the position of non-tiled windows
                session->updateDragPoint(cursor()->pos());
            }
        }
        
    }
 
    // resizing activity is prior to others
    if (activeResizing)
        return;
 
    bool activeMoving { false };

    for (LToplevelMoveSession *session : seat()->toplevelMoveSessions())
    {
        // TODO: Why can't touch?
        if (session->triggeringEvent().type() != LEvent::Type::Touch){

            if(!TileyServer::getInstance().is_compositor_modifier_down){
                break;
            }

            activeMoving = true;
            session->updateDragPoint(cursor()->pos());

            session->toplevel()->surface()->repaintOutputs();
            
            if (session->toplevel()->maximized())
                session->toplevel()->configureState(session->toplevel()->pendingConfiguration().state &~ LToplevelRole::Maximized);
        }

    }
 
    // moving activity is prior to others (Moving and Resizing processing are interchangeable)
    if (activeMoving)
        return;
 
    // If a surface had the left pointer button held down
    if (draggingSurface())
    {
        // set dragging position
        event.localPos = cursor()->pos() - draggingSurface()->rolePos();
        sendMoveEvent(event);
        return;
    }


    // all special cases proceed, common logic starts here

    LSurface *surface { pointerConstrained ? focus() : surfaceAtWithFilter(cursor()->pos(), 
        [this](LSurface* s) -> bool{
            auto surface = static_cast<Surface*>(s);
            if(!surface || !surface->getView()){
                return false;
            }

            if(!surface->getView()->visible()){
                return false;
            }
            
            return true;
        })
    };
 
    if (surface)
    {
        if (sessionLocked && surface->client() != sessionLockManager()->client())
            return;
 
        event.localPos = cursor()->pos() - surface->rolePos();

        // DND surface is not included in compositor->surfaces()
        if (activeDND)
        {
            if (seat()->dnd()->focus() == surface)
                // To surface: attention please, a DND is crossing your airspace
                seat()->dnd()->sendMoveEvent(event.localPos, event.ms());
            else
                // To DND: focus the surface under you now
                seat()->dnd()->setFocus(surface, event.localPos);
        }
        else
        {
            if (focus() == surface){
                sendMoveEvent(event);
                // TODO: make focus-follow-mouse configurable
                bool focusFollowMouse = true;
                if(focusFollowMouse){
                    seat()->keyboard()->setFocus(surface);
                    if(surface->toplevel()){
                        surface->toplevel()->configureState(surface->toplevel()->pendingConfiguration().state | LToplevelRole::Activated);
                    }
                }
            } else {
                setFocus(surface, event.localPos);
            }
        }
 
        // client requested to change cursor
        cursor()->setCursor(surface->client()->lastCursorRequest());
    }
    else
    {
        if (activeDND)
            // TO DND: you are now at public airspace, no surfaces available
            seat()->dnd()->setFocus(nullptr, LPointF());
        else
        {
            setFocus(nullptr);
            cursor()->useDefault();
            cursor()->setVisible(true);
        }
    }
}

// pointer focus is just an interior design not of wayland protocol?

// TODO: Never lose focus until last window of a workspace closed
void Pointer::focusChanged(){

    LLog::debug("[Pointer::focusChanged]: pointer focus is changed");

    Surface* surface = static_cast<Surface*>(focus());
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();

    if(!surface){
        return;
    }

    if(surface->roleId() == LSurface::SessionLock || surface->roleId() == LSurface::Popup){
        return;   
    }

    if(!surface->toplevel() && !surface->topmostParent()){
        manager.setActiveContainer(nullptr);
        LLog::debug("[Pointer::focusChanged]: no surface available at cursor position, set activated container to null");
        return;
    }

    
    if(!manager.isTiledWindow(surface->tl())){
        LLog::debug("[Pointer::focusChanged]: target surface is not of a window role, stop processing");
        return;
    }

    manager.setActiveContainer(surface->tl()->container);
    LLog::debug("[Pointer::focusChanged]: set active container to target surface");

}