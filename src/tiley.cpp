#include "LNamespaces.h"
#include <LLauncher.h>
#include <LLog.h>
#include "src/lib/TileyCompositor.hpp"
#include <cstdlib>

#include <getopt.h>
#include <signal.h>

#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/ipc/IPCManager.hpp"
#include "src/lib/client/WallpaperManager.hpp"
#include "src/lib/types.hpp"

// 设置启动参数, 具体含义在LaunchArgs结构体中说明
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
    // 设置桌面环境名称为我们的合成器名字
    setenv("XDG_CURRENT_DESKTOP", "Tiley", 1);
    // 启用Louvre的调试输出
    // 等级数字参考LLog::init()中的说明
    setenv("LOUVRE_DEBUG", args.enableDebug ? "4" : "1", 0);
    // 启用SRM的调试输出
    setenv("SRM_DEBUG", args.enableDebug ? "4" : "1", 0);

    /* Enable triple buffering when using the DRM backend (provides a smoother
     * experience but consumes more CPU) */
    setenv("SRM_RENDER_MODE_ITSELF_FB_COUNT", "3", 0);

    /* Same but for non-primary GPUs */
    setenv("SRM_RENDER_MODE_PRIME_FB_COUNT", "3", 0);
    setenv("SRM_RENDER_MODE_DUMB_FB_COUNT", "3", 0);
    setenv("SRM_RENDER_MODE_CPU_FB_COUNT", "3", 0);

    /* Force OpenGL buffer allocation instead of GBM when using the DRM backend
     */
    setenv("SRM_FORCE_GL_ALLOCATION", "1", 0);

    // TODO: 随着时间的推移, 这个列表应该会得到扩充, 以期支持越来越多的不同框架的软件
    // 特殊: 启用Firefox的WAYLAND支持
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);
    // 特殊: 启用QT的WAYLAND支持
    setenv("QT_QPA_PLATFORM", "wayland-egl", 1);
    // 特殊: 启用GDK的WAYLAND支持
    setenv("GDK_BACKEND", "wayland", 1);
    // 当嵌套运行时, 启用wayland后端
    setenv("LOUVRE_WAYLAND_DISPLAY", "wayland-2", 0);

    Louvre::LLauncher::startDaemon();

    // 加载服务器需要的资源
    // 着色器脚本
    tiley::TileyServer::getInstance().initOpenGLResources();
    // 键盘快捷键映射表注册
    tiley::TileyServer::getInstance().initKeyEventHandlers();
    // 壁纸管理器初始化
    tiley::WallpaperManager::getInstance().initialize();
    
    if(args.startupCMD){
        tiley::TileyServer::getInstance().populateStartupCMD(std::string("/bin/sh -c ").append(args.startupCMD));
    }

    tiley::TileyCompositor compositor;

    if(!compositor.start()){
        LLog::fatal("启动合成器失败。");
        return EXIT_FAILURE;
    }

    // 设置服务器在嵌套模式下的名称
    if(getenv("WAYLAND_DISPLAY") != nullptr){
        // TODO
    }

    // 窗口管理器初始化
    tiley::TileyWindowStateManager::getInstance().initialize();
    // 进程间通信管理类初始化
    tiley::IPCManager::getInstance().initialize();

    //***************启动****************
    // 主循环
    while(compositor.state() != LCompositor::Uninitialized){
        compositor.processLoop(100);
    }

    // 释放服务器资源
    tiley::TileyServer::getInstance().uninitOpenGLResources();

    return EXIT_SUCCESS;

}