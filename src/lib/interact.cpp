#include "include/interact.hpp"
#include "include/server.hpp"

/*********重要: 聚焦窗口的函数**********/

void focus_toplevel(struct surface_toplevel* toplevel){

    //对于我们的平铺式管理器而言, 需要做下面的事情:
    //1. 从上一个窗口失焦
    //2. 将该窗口置于顶层
    //3. 激活新的窗口
    //4. 将键盘聚焦到该窗口
    //5. 平铺式特色: 将鼠标瞬移到该窗口(对于刚创建的窗口, 移动到中心)

    // tinywl中说这个函数仅用于键盘聚焦。不太能理解"仅用于键盘"的含义,
    // 因为正常情况下确实只有键盘聚焦。(需要其他设备也将输入定向到这个窗口中吗?)

    if(toplevel == NULL){
        return;  // 防止没有窗口(尤其是针对调用者是通过查找得到的窗口的情况, 有可能没有窗口)
    }

    tiley::TileyServer& server = tiley::TileyServer::getInstance();
    struct wlr_seat* seat = server.seat;
    struct wlr_surface* prev_surface = seat->keyboard_state.focused_surface;
    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;

    if(prev_surface == surface){
        // 如果就是之前那个(或者都为空), 不用再聚焦
        return;
    }

    if(prev_surface){
        //失焦之前的窗口
        struct wlr_xdg_toplevel* prev_toplevel = 
            wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if(prev_toplevel != NULL){
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
        }
    }

    // 聚焦现在的窗口
    // 1. 拿到键盘
    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
    // 2. 从场景中将窗口移动到最前面(前端)
    wlr_scene_node_raise_to_top_(get_wlr_scene_tree_node(toplevel->scene_tree));
    // 3. 从逻辑上将窗口移动到最前面(后端)
    wl_list_remove(&toplevel->link);
    wl_list_insert(&server.toplevels, &toplevel->link);
    // 4. 激活窗口的surface
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
    // 5. 告知用户键盘已经移动到新的窗口中。后续输入将定向到这个窗口中
    if(keyboard != NULL){
        wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }
}