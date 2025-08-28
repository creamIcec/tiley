#include <LLayout.h>
#include <LNamespaces.h>
#include <LWeak.h>
#include <LAnimation.h>
#include <LOutput.h>
#include <LSessionLockRole.h>
#include <LScreenshotRequest.h>
#include <LOutputMode.h>
#include <LPainter.h>
#include <LRegion.h>
#include <LOpenGL.h>
#include <LCursor.h>
#include <LContentType.h>
#include <LLog.h>

#include "Output.hpp"

#include "src/lib/TileyServer.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/WallpaperManager.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

using namespace Louvre;
using namespace tiley;

Output::Output(const void* params) noexcept : LOutput(params){
    const LRegion region;  // wallpaper occupies a region to be rendered
    m_wallpaperView.setTranslucentRegion(&region);
} 

void Output::initializeGL(){

    LLog::debug("[monitor id: %u] is plugged in", this->id());

    TileyServer& server = TileyServer::getInstance();

    LLog::debug("[monitor id: %u] initialize rendering context for monitor", this->id());

    // workaround: disable VSync when running in nested mode
    if(compositor()->graphicBackendId() == LGraphicBackendWayland){
        enableVSync(false);
    }

    // scene rendering
    server.scene().handleInitializeGL(this);

    // output mode selection
    const LOutputMode* mode = preferredMode();
    if(mode != nullptr){
        this->setMode(mode);
    }

    LWeak<Output> weakRef {this};
    fadeInView.insertAfter(&server.layers()[OVERLAY_LAYER]);
    fadeInView.setOpacity(0.f);

    LAnimation::oneShot(1000, [weakRef](LAnimation* anim){
        if(!weakRef){  //Stop animation once the output is no longer available
            anim->stop();
            return;
        }

        weakRef->fadeInView.setPos(weakRef->pos());
        weakRef->fadeInView.setSize(weakRef->size());
        weakRef->fadeInView.setOpacity(1.f - powf(anim->value(), 5.f));
        weakRef->repaint();
    },
    // remove the animation view once animation finished
    [weakRef](LAnimation*){
        if(weakRef){
            weakRef->fadeInView.setParent(nullptr);
        }
    });

    updateWallpaper();

    // Test settings
    perfTag_ = "test";
    // TODO: reformat hardcoded path
    tiley::setPerfmonPath(perfTag_, "/home/zero/tiley/src/lib/test/test_1.txt");
    // initialize testing manager
    perfMon_ = &tiley::perfmon(perfTag_);
    // End of Test settings

}

void Output::paintGL(){
  
    // Test settings
    tiley::setPerfmonPath("test", "/home/zero/tiley/src/lib/test/test_1.txt");
    // End of Test settings

    Surface* fullscreenSurface{ searchFullscreenSurface() };

    bool directScanout = false;

    // directly render the fullscreened window to screen without compositing for better performance 
    if (fullscreenSurface){
        setContentType(fullscreenSurface->contentType());
        enableVSync(fullscreenSurface->preferVSync());
        enableFractionalOversampling(false);
        if(tryDirectScanout(fullscreenSurface)){    
            directScanout = true;
            return;
        }
    }else{
        setContentType(Louvre::LContentTypeNone);
        enableFractionalOversampling(true);
    }

    TileyServer& server = TileyServer::getInstance();


    if (!fullscreenSurface || !directScanout) {
        perfMon_->renderStart();
        server.scene().handlePaintGL(this);
        //LLog::debug("testing paintGL");
        perfMon_->renderEnd();
    }

    for(LScreenshotRequest * req : screenshotRequests()){
        req->accept(true);
    }
  
    perfMon_->recordFrame();

    // check wallpaper pending update status
    if (WallpaperManager::getInstance().wallpaperChanged()) {
        // reset flag
        updateWallpaper();
    }
};

void Output::moveGL(){
   TileyServer& server = TileyServer::getInstance();
   server.scene().handleMoveGL(this);

   updateWallpaper();
};

// access real size of screen from here, not initializeGL
void Output::resizeGL(){
    LLog::debug("[monitor id: %u]screen state has been changed", this->id());
    LLog::debug("[monitor id: %u]received new monitor configuration request: scale: %f, size: %dx%d", 
                this->id(), this->scale(), this->size().w(), this->size().h());

    TileyServer& server = TileyServer::getInstance();
    
    server.scene().handleResizeGL(this);

    updateWallpaper();
};

void Output::uninitializeGL(){
    TileyServer& server = TileyServer::getInstance();
    server.scene().handleUninitializeGL(this);
};

void Output::setGammaRequest(LClient* client, const LGammaTable* gamma){
    L_UNUSED(client);
    setGamma(gamma);
};

void Output::availableGeometryChanged(){
    LOutput::availableGeometryChanged();
};


bool Output::tryDirectScanout(Surface* surface) noexcept{
    L_UNUSED(surface);
    return false;
}

Surface* Output::searchFullscreenSurface() const noexcept{
    // lockscreen
    if(sessionLockRole() && sessionLockRole()->surface()->mapped()){
        return static_cast<Surface*>(sessionLockRole()->surface());
    }

    // check 4 conditions for determining if a surface is in fullscreen 
    for(auto it = compositor() -> layer(Louvre::LLayerTop).rbegin();it != compositor()->layer(LLayerTop).rend(); it++){
        if ((*it)->mapped() && (*it)->toplevel() && (*it)->toplevel()->fullscreen() && (*it)->toplevel()->exclusiveOutput() == this)
            return static_cast<Surface*>(*it);
    }

    return nullptr;
}

void Output::updateWallpaper(){
    WallpaperManager::getInstance().applyToOutput(this);
}