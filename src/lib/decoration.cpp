#include "include/decoration.hpp"
#include "include/types.h"
#include "server.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"

// 定义浮动边框的厚度和颜色
const int FLOATING_BORDER_THICKNESS = 4; // 可以比移动边框细一点
const float FLOATING_BORDER_COLOR[] = {0.1f, 0.6f, 1.0f, 0.9f}; // 好看的蓝色:D

// 函数：控制装饰的可见性
void set_toplevel_decoration_enabled(surface_toplevel* toplevel, bool enabled) {
    if (!toplevel->decoration.top) return;
    
    wlr_scene_node_set_enabled_(get_wlr_scene_rect_node(toplevel->decoration.top), enabled);
    wlr_scene_node_set_enabled_(get_wlr_scene_rect_node(toplevel->decoration.bottom), enabled);
    wlr_scene_node_set_enabled_(get_wlr_scene_rect_node(toplevel->decoration.left), enabled);
    wlr_scene_node_set_enabled_(get_wlr_scene_rect_node(toplevel->decoration.right), enabled);
}

// 当装饰不存在时, 创建窗口装饰
void create_toplevel_decoration(surface_toplevel* toplevel) {
    if (toplevel->decoration.top) return;
    
    toplevel->decoration.top    = wlr_scene_rect_create_(toplevel->scene_tree, 0, 0, FLOATING_BORDER_COLOR);
    toplevel->decoration.bottom = wlr_scene_rect_create_(toplevel->scene_tree, 0, 0, FLOATING_BORDER_COLOR);
    toplevel->decoration.left   = wlr_scene_rect_create_(toplevel->scene_tree, 0, 0, FLOATING_BORDER_COLOR);
    toplevel->decoration.right  = wlr_scene_rect_create_(toplevel->scene_tree, 0, 0, FLOATING_BORDER_COLOR);

    // 刚创建时默认不显示
    set_toplevel_decoration_enabled(toplevel, false);
}

// 根据窗口当前大小更新装饰的位置和尺寸
void update_toplevel_decoration(surface_toplevel* toplevel) {
    if (!toplevel->decoration.top) return;

    int width = toplevel->xdg_toplevel->current.width;
    int height = toplevel->xdg_toplevel->current.height;
    const int BORDER = FLOATING_BORDER_THICKNESS;

    wlr_scene_rect_set_size_(toplevel->decoration.top, width, BORDER);
    wlr_scene_node_set_position_(get_wlr_scene_rect_node(toplevel->decoration.top), 0, -BORDER);
    
    wlr_scene_rect_set_size_(toplevel->decoration.bottom, width, BORDER);
    wlr_scene_node_set_position_(get_wlr_scene_rect_node(toplevel->decoration.bottom), 0, height);
    
    wlr_scene_rect_set_size_(toplevel->decoration.left, BORDER, height + 2 * BORDER);
    wlr_scene_node_set_position_(get_wlr_scene_rect_node(toplevel->decoration.left), -BORDER, -BORDER);
    
    wlr_scene_rect_set_size_(toplevel->decoration.right, BORDER, height + 2 * BORDER);
    wlr_scene_node_set_position_(get_wlr_scene_rect_node(toplevel->decoration.right), width, -BORDER);
}

// 销毁装饰(当窗口关闭时调用)
void destroy_toplevel_decoration(surface_toplevel* toplevel) {
    if (!toplevel->decoration.top) return;
    wlr_scene_node_destroy_(get_wlr_scene_rect_node(toplevel->decoration.top));
    wlr_scene_node_destroy_(get_wlr_scene_rect_node(toplevel->decoration.bottom));
    wlr_scene_node_destroy_(get_wlr_scene_rect_node(toplevel->decoration.left));
    wlr_scene_node_destroy_(get_wlr_scene_rect_node(toplevel->decoration.right));
    toplevel->decoration.top = nullptr;
}