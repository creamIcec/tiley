#include "TileyCompositor.hpp"
#include "LCompositor.h"
#include "LNamespaces.h"
#include "TileyServer.hpp"
#include "src/lib/client/Client.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/input/Keyboard.hpp"
#include "src/lib/input/Pointer.hpp"
#include "src/lib/input/Seat.hpp"
#include "src/lib/output/Output.hpp"
#include "src/lib/surface/Surface.hpp"

#include <LTransform.h>
#include <LLog.h>
#include <LOutputMode.h>
#include <LLauncher.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

using namespace tiley;
using namespace Louvre;

// Util function for finding the best scaling factor of an monitor
static UInt32 getIdealScale(LOutput* output){
    
    // 96: from early Microsoft Windows
    float idealScale = output->dpi() / 96.f;

    float finalScale = roundf(idealScale);

    if (finalScale < 1.f) {
        finalScale = 1.f;
    }

    return finalScale;
}

// Util function for finding the best display mode of an monitor(especially refresh rate)
static LOutputMode* getBestDisplayMode(LOutput* output){
    LOutputMode *bestMode = nullptr;
    const LSize &physicalSize = output->size();
    LLog::debug("[output id: %u] size: %dx%d", output->id(), physicalSize.w(), physicalSize.h());

    for (LOutputMode *mode : output->modes()){
        LLog::debug("[output id: %u] screen mode: %dx%d @ %.2f Hz", output->id(), physicalSize.w(), physicalSize.h(), mode->refreshRate() / 1000.f);
        if (mode->sizeB() == physicalSize){
            if (!bestMode || mode->refreshRate() > bestMode->refreshRate()){
                bestMode = mode;
            }
        }
    }

    if (bestMode){
        LLog::debug("[output id: %u] use display mode: %dx%d @ %.2f Hz",
                   output->id(), bestMode->sizeB().w(), bestMode->sizeB().h(), bestMode->refreshRate() / 1000.f);
        return bestMode;
    }

    return nullptr;
}

void TileyCompositor::initialized(){
    setenv("WAYLAND_DISPLAY", getenv("LOUVRE_WAYLAND_DISPLAY"), 1);

    TileyServer& server = TileyServer::getInstance();

    server.seat()->configureInputDevices();

    int32_t totalWidth {0};

    // all outputs(both unconfigured and configured)
    for(LOutput* output : server.seat()->outputs()){
        LOutputMode* bestMode = getBestDisplayMode(output);
        /*
        if(bestMode){
            output->setMode(bestMode);
        }else{
            LLog::warning("[monitor id: %u] warning: unable to find best display mode, enable default mode");
        }
        */
        output->setScale(getIdealScale(output));
        output->setTransform(LTransform::Normal);
        // TODO: let users configure their preferred monitor layouts
        output->setPos(LPoint(totalWidth, 0));
        totalWidth += output->size().w();
        addOutput(output);
        // tell the monitor to unlock its rendering thread
        output->repaint();
    }

    // TODO: enable waybar when config requires
    // TODO: launch waybar with args
    bool waybar = true;
    if(waybar){
        //Louvre::LLauncher::launch("export SWAYSOCK=/tmp/tiley-ipc.sock; waybar");
    }

    const std::vector<std::string>& cmds = TileyServer::getInstance().getStartupCMD();
    for(auto cmd : cmds){
        LLog::debug("Launching command: %s ...", cmd.c_str());
        Louvre::LLauncher::launch(cmd);
    }
}

void TileyCompositor::uninitialized(){
    // Destroy all environmental objects here
}


// LObject -> LFactoryObject -> XXXObject(Output, Client, etc.)
LFactoryObject* TileyCompositor::createObjectRequest(LFactoryObject::Type objectType, const void* params){

    if (objectType == LFactoryObject::Type::LOutput){
        return new Output(params);
    }

    if (objectType == LFactoryObject::Type::LClient){
        return new Client(params);
    }

    if (objectType == LFactoryObject::Type::LSurface){
        return new Surface(params);
    }
    
    if (objectType == LFactoryObject::Type::LToplevelRole){
        return new ToplevelRole(params);
    }

    if (objectType == LFactoryObject::Type::LKeyboard){
        return new Keyboard(params);
    }

    if (objectType == LFactoryObject::Type::LPointer){
        return new Pointer(params);
    }

    if (objectType == LFactoryObject::Type::LSeat){
        return new Seat(params);
    }

    return LCompositor::createObjectRequest(objectType, params);  //move down gradually following the realization of objects

    if (objectType == LFactoryObject::Type::LSubsurfaceRole){
        // TODO
    }
    if (objectType == LFactoryObject::Type::LSessionLockRole){
        // TODO
    }
    if (objectType == LFactoryObject::Type::LCursorRole){
        // TODO
    }
    if (objectType == LFactoryObject::Type::LDNDIconRole){
        // TODO
    }


    if (objectType == LFactoryObject::Type::LTouch){
        // TODO
    }
    if (objectType == LFactoryObject::Type::LClipboard){
        // TODO
    }
    if (objectType == LFactoryObject::Type::LDND){
        // TODO
    }
    if(objectType == LFactoryObject::Type::LSessionLockManager){
        // TODO
    }

    // nullptr means default LFactoryObject
    return nullptr;
}

void TileyCompositor::onAnticipatedObjectDestruction(LFactoryObject* object){
    if(object->factoryObjectType() == LFactoryObject::Type::LClient){
        // TODO: destroy the client
    }
}

bool TileyCompositor::createGlobalsRequest(){
    return LCompositor::createGlobalsRequest();
}


bool TileyCompositor::globalsFilter(LClient* client, LGlobal* global){
    return true; //TODO
}

void TileyCompositor::printToplevelSurfaceLinklist(){
    LLog::log("***********Surfaces order(later means higher visual layer)***********");
    for(LSurface* surface : surfaces()){
        if(surface->toplevel()){
            if(surface->nextSurface()){
                LLog::log("%d<-", surface);
            }else{
                LLog::log("%d", surface);
            }
        }
    }
    LLog::log("************************************************");
};