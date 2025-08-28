#include <LLog.h>
#include <LCursor.h>
#include <LSurface.h>
#include <LOutput.h>
#include <LNamespaces.h>

#include "ToplevelRole.hpp"
#include "LToplevelRole.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/core/UserAction.hpp"

using namespace tiley;

ToplevelRole::ToplevelRole(const void *params) noexcept : LToplevelRole(params)
{
    // from Louvre-views
    moveSession().setOnBeforeUpdateCallback([](LToplevelMoveSession *session)
    {
        LMargins constraints { session->toplevel()->calculateConstraintsFromOutput(cursor()->output()) };

        if (constraints.bottom != LEdgeDisabled)
        {
            constraints.bottom += session->toplevel()->windowGeometry().size().h()
                + session->toplevel()->extraGeometry().top
                + session->toplevel()->extraGeometry().bottom
                - 0;   //- 50;  // TODO: better read from IPC/config
        }

        session->setConstraints(constraints);

        // Any better place?
        if(!TileyServer::getInstance().is_compositor_modifier_down){
            stopMoveSession(false);
        }
    });

    resizeSession().setOnBeforeUpdateCallback([](LToplevelResizeSession *session)
    {
        LMargins constraints { session->toplevel()->calculateConstraintsFromOutput(cursor()->output()) };

        session->setConstraints(constraints);

        // Any better place?
        if(!TileyServer::getInstance().is_compositor_modifier_down){
            stopResizeSession(false);
        }
    });
}


void ToplevelRole::printWindowGeometryInfo(LToplevelRole* toplevel){

    if(!toplevel){
        LLog::log("[printWindowAreaInfo]: cannot print window geometry info of null");
        return;
    }
    
    LLog::log("[printWindowAreaInfo]: extra geometry: left: %d, top: %d, right: %d, bottom: %d", 
        toplevel->extraGeometry().left,
        toplevel->extraGeometry().top,
        toplevel->extraGeometry().right,
        toplevel->extraGeometry().bottom
    );
    LLog::log("[printWindowAreaInfo]: window geometry: width: %d, height: %d, x: %d, y: %d, bottomRight: (%dx%d)",
        toplevel->windowGeometry().width(),
        toplevel->windowGeometry().height(),
        toplevel->windowGeometry().x(),
        toplevel->windowGeometry().y(),
        toplevel->windowGeometry().bottomRight().width(),
        toplevel->windowGeometry().bottomRight().height()
    );
}

void ToplevelRole::atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev){
    //LLog::log("[atomsChanged]: window properties changed");
    LToplevelRole::atomsChanged(changes, prev);
};

// client triggers configuration and we need to respond with proper settings
void ToplevelRole::configureRequest(){
    LLog::debug("[configureRequest]: client requires configuration");

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
    
    // server-sided decorations are not well supported by GTK applications
    if (supportServerSideDecorations()){
        configureDecorationMode(ServerSide);
    }

    configureCapabilities(WindowMenuCap | FullscreenCap | MaximizeCap | MinimizeCap);

    LLog::debug("[configureRequest]: client preferred decoration mode: %d", preferredDecorationMode());

}

// from https://gitlab.com/fakinasa/polowm/-/blob/master/src/roles/ToplevelRole.cpp
// Check whether the size of a window is restricted
bool ToplevelRole::hasSizeRestrictions()
{
    return ( (minSize() != LSize{0, 0}) &&
             ((minSize().w() == maxSize().w()) || (minSize().h() == maxSize().h()))
           ) || (surface()->sizeB() == LSize{1, 1});
}

void ToplevelRole::assignToplevelType(){

    // TODO: read user prefer floating application ids from config
    bool userFloat = false;

    if(hasSizeRestrictions()){
        this->type = RESTRICTED_SIZE;
    }else if(surface()->parent() || userFloat){
        this->type = FLOATING;
    }else{
        this->type = NORMAL;
    }
}

void ToplevelRole::startMoveRequest(const LEvent& triggeringEvent){
    // TODO: close any popups before moving?
    LToplevelRole::startMoveRequest(triggeringEvent);
}

void ToplevelRole::startResizeRequest(const LEvent& triggeringEvent, LBitset<LEdge> edge){
    // TODO: close any popups before resizing?
    LToplevelRole::startResizeRequest(triggeringEvent, edge);    
}