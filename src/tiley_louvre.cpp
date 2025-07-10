#include "LNamespaces.h"
#include <LLauncher.h>
#include <LLog.h>
#include "src/lib/TileyCompositor.hpp"
#include <cstdlib>

int main(){
    // 启用Louvre的调试输出
    setenv("LOUVRE_DEBUG", "1", 0);
    // 启用SRM的调试输出
    setenv("SRM_DEBUG", "1", 0);

    // TODO: 随着时间的推移, 这个列表应该会得到扩充, 以期支持越来越多的不同框架的软件
    // 特殊: 启用Firefox的WAYLAND支持
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);
    // 特殊: 启用QT的WAYLAND支持
    setenv("QT_QPA_PLATFORM", "wayland-egl", 1);
    // 特殊: 启用GDK的WAYLAND支持
    setenv("GDK_BACKEND", "wayland", 1);

    setenv("LOUVRE_WAYLAND_DISPLAY", "wayland-2", 0);

    Louvre::LLauncher::startDaemon();

    tiley::TileyCompositor compositor;

    if(!compositor.start()){
        LLog::fatal("启动合成器失败。");
        return EXIT_FAILURE;
    }

    //***************启动****************
    // 主循环
    while(compositor.state() != LCompositor::Uninitialized){
        compositor.processLoop(-1);
    }

    return EXIT_SUCCESS;

}