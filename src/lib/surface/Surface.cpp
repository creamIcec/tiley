#include "Surface.hpp"

#include <cmath>

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
#include <LAnimation.h>
#include <LRegion.h>
#include <LSceneView.h>
#include <LTextureView.h>

#include "LTexture.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/views/SurfaceView.hpp"
#include "src/lib/types.hpp"
#include "src/lib/Utils.hpp"

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

// from Louvre
LTexture* Surface::renderThumbnail(LRegion* transRegion){
    LBox box { getView()->boundingBox() };

    minimizeStartRect = LRect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);

    LSceneView tmpView(minimizeStartRect.size(), 1);
    tmpView.setPos(minimizeStartRect.pos());

    LView *prevParent { getView()->parent() };
    getView()->setParent(&tmpView);


    struct TMPList
    {
        LSurfaceView *view;
        LView *parent;
    };

    std::list<TMPList> tmpChildren;

    Surface *next { this };
    while ((next = (Surface*)next->nextSurface()))
    {
        if (next->parent() == this && next->subsurface())
        {
            tmpChildren.emplace_back(next->view.get(), next->view->parent());
            next->view->enableParentOffset(false);
            next->view->setParent(&tmpView);
        }
    }

    getView()->enableParentOffset(false);

    tmpView.render();

    if (transRegion)
    {
        *transRegion = *tmpView.translucentRegion();
        transRegion->offset(LPoint() - tmpView.pos());
    }

    LTexture *renderedThumbnail { tmpView.texture()->copy() };
    getView()->enableParentOffset(true);
    getView()->setParent(prevParent);

    while (!tmpChildren.empty())
    {
        tmpChildren.front().view->enableParentOffset(true);
        tmpChildren.front().view->setParent(tmpChildren.front().parent);
        tmpChildren.pop_front();
    }

    return renderedThumbnail;

}

void Surface::startUnmappedAnimation() noexcept
{
    if (isClosing) return;
    isClosing = true;

    view->enableAlwaysMapped(true);
    LTexture* thumbnail = renderThumbnail();

    if (!thumbnail) {
        view->enableAlwaysMapped(false);
        return;
    }

    auto& server = TileyServer::getInstance();
    LTextureView *fadeOutView = new LTextureView(thumbnail, &server.layers()[APPLICATION_LAYER]);

    const LPoint initialPos = pos();

    // unmapped animation params setup
    constexpr Float32 FINAL_SCALE_RATIO = 0.7f;
    constexpr UInt32 ANIMATION_MS = 150;

    fadeOutView->setPos(initialPos);
    fadeOutView->enableParentOffset(false);
    view->enableAlwaysMapped(false);
    
    fadeOutView->enableScaling(true);
    LWeak<Surface> weakSelf {this};

    LAnimation::oneShot(ANIMATION_MS, 
        [fadeOutView, initialPos, FINAL_SCALE_RATIO](LAnimation *animation) {
            
            // scaling animation
            const Float32 ease = (1.f - cosf(animation->value() * M_PI)) * 0.5f;

            const float scaleFactor = tiley::math::lerp(1.f, FINAL_SCALE_RATIO, ease);
            fadeOutView->setScalingVector(scaleFactor);

            const LPoint currentPos = initialPos + (fadeOutView->size() * (1.f - scaleFactor)) / 2;
            fadeOutView->setPos(currentPos);
            
            // opacity animation
            const float opacity = 1.f - ease;
            fadeOutView->setOpacity(opacity);

            compositor()->repaintAllOutputs();
        },
        [fadeOutView, weakSelf](LAnimation *) {

            if (fadeOutView) {
                delete fadeOutView->texture();
                delete fadeOutView;
            }

            compositor()->repaintAllOutputs();
        }
    );
}



Surface::Surface(const void *params) : LSurface(params){
    fadeInAnimation.setOnUpdateCallback([this](LAnimation* animation){
        const Float32 ease { 1.f - powf(1.f - animation->value(), 6.f) };
        getView()->setOpacity(ease);

        Surface *next = (Surface*)nextSurface();
        while (next)
        {
            if (next->isSubchildOf(this) && !next->minimized())
            {
                next->getView()->setOpacity(ease);
            }
            next = (Surface*)next->nextSurface();
        }
        repaintOutputs();
    });

    fadeInAnimation.setOnFinishCallback([this](LAnimation*){
        getView()->setOpacity(1.f);

        Surface *next = (Surface*)nextSurface();

        while (next)
        {
            if (next->isSubchildOf(this) && !next->minimized())
            {
                next->getView()->setOpacity(1.f);
            }

            next = (Surface*)next->nextSurface();
        }
        repaintOutputs();
    });
}


LView* Surface::getView() noexcept{

    if(tl() && tl()->container && TileyWindowStateManager::getInstance().isTiledWindow(tl())){
        return tl()->container->getContainerView();  // window is tilable
    }else{
        return view.get(); // return raw view of window
    }
}

// TODO: deprecation
LSurfaceView* Surface::getSurfaceView() noexcept{
    return view.get();
}

void Surface::printWindowGeometryDebugInfo(LOutput* activeOutput, const LRect& outputAvailable) noexcept{
    if(toplevel()){
        const LMargins& margin = toplevel()->extraGeometry();
        const LRect& acquiredGeometry = size();
        const LRect& windowGeometry = toplevel()->windowGeometry();

        LLog::log("**************New Window information: %s**************", this);
        LLog::log("Display geometry available: %dx%d", outputAvailable.w(), outputAvailable.h());
        for(auto zone : activeOutput->exclusiveZones()){
            LLog::log("Display reserved geometry: %dx%d, attached edge: %b", zone->rect().w(), zone->rect().h(), zone->edge());
        }
        LLog::log("Extra zone for margin: top: %d, right: %d, bottom: %d, left: %d", margin.top, margin.right, margin.bottom, margin.left);
        LLog::log("client acquired geometry: %dx%d", acquiredGeometry.w(), acquiredGeometry.h());
        LLog::log("client real geometry: %dx%d", windowGeometry.w(), windowGeometry.h());
        LLog::log("window offect: (%d, %d)", toplevel()->windowGeometry().pos().x(), toplevel()->windowGeometry().pos().y());

    }else{
        LLog::log("[printWindowGeometryDebugInfo]: target surface is not toplevelrole, skip printing");
    }
}

// path: roleChanged() -> orderChanged -> ((if window) -> configureRequest() -> atomsChanged()) -> mappingChanged 
void Surface::roleChanged(LBaseSurfaceRole *prevRole){
    LSurface::roleChanged(prevRole);
    TileyServer& server = TileyServer::getInstance();

    if(cursorRole()){
        // display software cursor
        getView()->setVisible(false);
        return;
    }else if (roleId() == LSurface::SessionLock || roleId() == LSurface::Popup){
        LLog::debug("[roleChanged]: a popup is shown");
        getView()->setParent(&server.layers()[LLayerOverlay]);
    }
}

// orderChanged: adjust views order to align surfaces order
void Surface::orderChanged()
{   
    //LLog::debug("Order Changed, address of surface object: %d, layer: %d", this, layer());
    
    // debug: print order of surfaces
    // TileyServer& server = TileyServer::getInstance();
    // server.compositor()->printToplevelSurfaceLinklist();

    // from Louvre-views
    if (toplevel() && toplevel()->fullscreen())
        return;

    Surface *prevSurface { static_cast<Surface*>(this->prevSurface()) };
    LView *view { getView() };

    while (prevSurface != nullptr)
    {
        if (subsurface() || prevSurface->getView()->parent() == view->parent())
            break;

        prevSurface = static_cast<Surface*>(prevSurface->prevSurface());
    }

    if (prevSurface)
    {
        if (subsurface() || prevSurface->getView()->parent() == getView()->parent())
            view->insertAfter(prevSurface->getView());
    }
    else
        view->insertAfter(nullptr);
}

// move surface of toplevel is enough, subsurfaces will follow as Louvre handles this
void Surface::layerChanged(){
    //LLog::debug("%d: Layer changed: ", this);
    TileyServer& server = TileyServer::getInstance();
    getView()->setParent(&server.layers()[layer()]);
}

// A Surface changed its mapping state, we do layout management here
void Surface::mappingChanged(){

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    compositor()->repaintAllOutputs();

    if(mapped()){
        // if the surface is not a toplevel, stop processing as parent is defined in constructor
        if(!toplevel()){
            return;
        }
        // else we assign a type to the toplevel
        tl()->assignToplevelType();

        Container* tiledContainer = nullptr;
        manager.addWindow(tl(), tiledContainer);
        
        // if inserted successfully
        if(tiledContainer){
            // set parent again to APPLICATION_LAYER for unexpected layer change
            getView()->setParent(&server.layers()[APPLICATION_LAYER]);

            manager.setActiveContainer(tiledContainer);
            LLog::debug("[mappingChanged]: set active container to newly added window");

            manager.recalculate();
        }

        // play animation
        fadeInAnimation.setDuration(400 * 1.5f);
        fadeInAnimation.start();

    } else {

        if(toplevel()){
            
            if(!toplevel()->fullscreen()){
                startUnmappedAnimation();
            }
            
            // remove window(suitable for all type of windows, including floating ones)
            Container* siblingContainer = nullptr;
            manager.removeWindow(tl(), siblingContainer);
            if(siblingContainer != nullptr){
                manager.recalculate();
            }
        }

        // remove focus from the surface
        if(seat()->pointer()->focus() == this){
            seat()->pointer()->setFocus(nullptr);
        }

        // destroy view
        getView()->setParent(nullptr);
    }
    
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}