#include "include/window_util.hpp"
#include "server.hpp"
#include "types.h"

#include <cstdint>
#include <iostream>

wlr_box get_target_geometry_box(area_container* target_container, uint32_t display_width, uint32_t display_height){
    int width, height;
    if(target_container->parent == nullptr){   // 为空说明只有桌面容器
        std::cout << "合并窗口到桌面根节点" << std::endl;
        width = display_width;
        height = display_height; // 设置为桌面参数    
    } else {  //否则拿到窗口参数
        wlr_box bb = target_container->toplevel->xdg_toplevel->base->geometry;
        width = bb.width;
        height = bb.height;
    }
    std::cout<< "拿到窗口参数" << std::endl;
    return {0,0,width,height};
}

wlr_box get_display_geometry_box(tiley::TileyServer& server){
    wlr_output* output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
    struct wlr_box output_box;
    wlr_output_layout_get_box(server.output_layout, output, &output_box);
    return output_box;
}