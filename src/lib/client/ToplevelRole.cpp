#include <LToplevelRole.h>
#include <LLog.h>
#include <LCursor.h>
#include <LSurface.h>
#include <LOutput.h>

#include "ToplevelRole.hpp"
#include "LNamespaces.h"

using namespace tiley;

void ToplevelRole::atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev){
    LToplevelRole::atomsChanged(changes, prev);
    LLog::log("装饰模式: %u", decorationMode());
};

void ToplevelRole::configureRequest(){
    LLog::log("接收到配置请求");

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
    configureDecorationMode(ClientSide);
    configureCapabilities(WindowMenuCap | FullscreenCap | MaximizeCap | MinimizeCap);
}