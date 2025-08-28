#pragma once

#include "LNamespaces.h"
#include <LBox.h>

using namespace Louvre;

namespace tiley{
    class Surface;
}

namespace tiley{

    struct LaunchArgs{
        bool enableDebug;  
        char* startupCMD;  // bash command
    };

    // Bottom to top
    enum TILEY_LAYERS{
        BACKGROUND_LAYER,  // wallpaper
        DESKTOP_LAYER,  // widget
        APPLICATION_LAYER,   // application windows and subsurfaces
        POPUP_LAYER,    // IME, system notifications
        OVERLAY_LAYER,  // Lock screen, monitor-wise animation
    };

    // Container split types
    enum SPLIT_TYPE{
        SPLIT_NONE,  // window
        SPLIT_H,
        SPLIT_V
    };

    // Why if a window is floating
    enum FLOATING_REASON{
        NONE,       // not float
        MOVING,
        STACKING,
    };

    static UInt32 DEFAULT_WORKSPACE = 0;
}