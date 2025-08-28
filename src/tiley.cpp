#include "LNamespaces.h"
#include <LLauncher.h>
#include <LLog.h>
#include "src/lib/TileyCompositor.hpp"
#include <cstdlib>

#include <getopt.h>

#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/ipc/IPCManager.hpp"
#include "src/lib/client/WallpaperManager.hpp"
#include "src/lib/types.hpp"

// Startup args collection
tiley::LaunchArgs setupParams(int argc, char* argv[]){

    tiley::LaunchArgs args = {false, nullptr};
    
    int c;

    // https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html
    struct option longopts[] = {
        {"debug", no_argument, NULL, 'd'},
        {"start", required_argument, NULL, 's'},
        {0,0,0,0}
    };

    while((c = getopt_long(argc, argv, "ds:", longopts, NULL)) != -1){
        switch(c){
            case 'd':
                args.enableDebug = true;
                break;
            case 's':
                args.startupCMD = optarg;
                break;
            default:
                break;
        }
    }

    return args;
}

int main(int argc, char* argv[]){
    tiley::LaunchArgs args = setupParams(argc, argv);
    // Set DE name
    setenv("XDG_CURRENT_DESKTOP", "Tiley", 1);
    // Refer to LLog::init()
    setenv("LOUVRE_DEBUG", args.enableDebug ? "4" : "1", 0);
    // Enable SRM debug
    setenv("SRM_DEBUG", args.enableDebug ? "4" : "1", 0);

    setenv("SRM_RENDER_MODE_ITSELF_FB_COUNT", "3", 0);

    setenv("SRM_RENDER_MODE_PRIME_FB_COUNT", "3", 0);
    setenv("SRM_RENDER_MODE_DUMB_FB_COUNT", "3", 0);
    setenv("SRM_RENDER_MODE_CPU_FB_COUNT", "3", 0);

    // GBM format allocation
    setenv("SRM_FORCE_GL_ALLOCATION", "1", 0);

    /* Enable support of softwares */
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);
    setenv("QT_QPA_PLATFORM", "wayland-egl", 1);
    setenv("GDK_BACKEND", "wayland", 1);
    setenv("LOUVRE_WAYLAND_DISPLAY", "wayland-2", 0);

    Louvre::LLauncher::startDaemon();

    /* Load compositor resources */
    // Shaders
    tiley::TileyServer::getInstance().initOpenGLResources();
    // Keyboard Shortcut
    tiley::TileyServer::getInstance().initKeyEventHandlers();
    // Wallpapers
    tiley::WallpaperManager::getInstance().initialize();
    
    if(args.startupCMD){
        tiley::TileyServer::getInstance().populateStartupCMD(std::string("/bin/sh -c ").append(args.startupCMD));
    }

    tiley::TileyCompositor compositor;

    if(!compositor.start()){
        LLog::fatal("Launch Compositor Failed.");
        return EXIT_FAILURE;
    }

    // Special settings for wayland backend
    if(compositor.graphicBackendId() == LGraphicBackendWayland){
        // TODO
    }

    // Window Management Initialization
    tiley::TileyWindowStateManager::getInstance().initialize();
    // IPC Management Initialization
    tiley::IPCManager::getInstance().initialize();

    //***************Launch****************
    while(compositor.state() != LCompositor::Uninitialized){
        compositor.processLoop(100);
    }

    tiley::TileyServer::getInstance().uninitOpenGLResources();

    return EXIT_SUCCESS;

}