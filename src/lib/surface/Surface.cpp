#include "Surface.hpp"
#include "LNamespaces.h"
#include "LSurface.h"
#include "src/lib/TileyServer.hpp"

using namespace tiley;

void Surface::mappingChanged(){

    LSurface::mappingChanged();

    // 干下面的事:
    // 1. 确保是一个toplevel(窗口)
    // 2. 使用户聚焦到该窗口
    // 3. 提升至场景树和数据树的最顶层
    // 4. 激活窗口
}


void Surface::roleChanged(LBaseSurfaceRole *prevRole){
    LSurface::roleChanged(prevRole);
}

void Surface::layerChanged(){
    LSurface::layerChanged();
}

void Surface::orderChanged(){
    LSurface::orderChanged();
}

void Surface::minimizedChanged(){
    LSurface::minimizedChanged();
}