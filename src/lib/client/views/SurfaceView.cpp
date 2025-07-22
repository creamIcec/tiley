#include "SurfaceView.hpp"
#include "LNamespaces.h"

#include "src/lib/surface/Surface.hpp"
#include "src/lib/interact/interact.hpp"

#include <LPointerButtonEvent.h>
#include <LLog.h>

using namespace tiley;

void SurfaceView::pointerButtonEvent(const LPointerButtonEvent &event){

    LLog::log("鼠标点击");

    L_UNUSED(event);

    Surface* surface = static_cast<Surface*>(this->surface());

    if(surface->toplevel()){
        LLog::log("点击窗口");
        focusWindow(surface);
    }
}

void SurfaceView::pointerEnterEvent (const LPointerEnterEvent &event){
    LLog::log("鼠标进入");
};