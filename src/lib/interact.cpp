#include "include/interact.hpp"
#include "include/server.hpp"
#include "include/core.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "types.h"

/*********重要: 聚焦窗口的函数**********/

bool focus_toplevel(struct surface_toplevel* toplevel){

    //对于我们的平铺式管理器而言, 需要做下面的事情:
    //1. 从上一个窗口失焦
    //2. 将该窗口置于顶层
    //3. 激活新的窗口
    //4. 将键盘聚焦到该窗口
    //5. 平铺式特色: 将鼠标瞬移到该窗口(对于刚创建的窗口, 移动到中心)

    // tinywl中说这个函数仅用于键盘聚焦。不太能理解"仅用于键盘"的含义,
    // 因为正常情况下确实只有键盘聚焦。(需要其他设备也将输入定向到这个窗口中吗?)

    if(toplevel == NULL){
        return false;  // 防止没有窗口(尤其是针对调用者是通过查找得到的窗口的情况, 有可能没有窗口)
    }

    tiley::TileyServer& server = tiley::TileyServer::getInstance();
    struct wlr_seat* seat = server.seat;
    struct wlr_surface* prev_surface = seat->keyboard_state.focused_surface;
    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;

    if(prev_surface == surface){
        // 如果就是之前那个(或者都为空), 不用再聚焦
        return false;
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

    // 设置聚焦窗口
    tiley::WindowStateManager& manager = tiley::WindowStateManager::getInstance();
    manager.set_focused_container(toplevel->container);

    return true;
}

/**********切换一个窗口的"悬挂"状态, 也就是在堆叠显示/平铺显示之间切换**************/
void toggle_overhang_toplevel(struct area_container *container, tiley::WindowStateManager& manager, tiley::TileyServer& server, int workspace){
    //1. 拿到toplevel的container
    //1.1 入参检查
    if(container == nullptr){
        return;
    }
    //1.2 如果是容器, 不允许分离(只能分离窗口)
    //注意: 从容器树分离的没有根节点, 无需检查container->parent
    if(container->toplevel == nullptr){
        return;
    }

    // 获取目标分割模式(仍然是长边分割)
    wlr_output* output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
    int32_t display_width = output->width;
    int32_t display_height = output->height;

    //2. 判断窗口处于的状态, 如果没有悬挂, 则调用detach分离, 理由为STACKING
    // 优先级(按照hyprland):
    // A. 悬挂状态
    // B. 移动状态
    // C. 正常状态
    
    if(container->floating != STACKING){
        //2.1 将窗口移动到单独的浮动场景树中
        wlr_scene_node* node = get_wlr_scene_tree_node(container->toplevel->scene_tree);
        wlr_scene_node_reparent_(node, server.floating_layer);
        wlr_scene_node_raise_to_top_(node);
    }

    // 在hyprland中, 按此优先级, 高的会覆盖低的。比如如果用户正在移动窗口, 但在移动过程中按下了悬挂快捷键, 会马上停止移动, 在当时的位置变成悬挂。
    if(container->floating == NONE){
        manager.detach(container, STACKING);   
    }else if(container->floating == MOVING){
        manager.interupt_moving(); //清除移动状态
        container->floating = STACKING;
    }else{   //3. 如果就是STACKING, 说明要切换回去, 则调用attach合并
        // 3.1 获取鼠标位置的container
        area_container* target_container = manager.desktop_container_at(server.cursor->x, server.cursor->y, workspace);
        int width, height;
        std::cout<< "窗口处于堆叠模式, 合并回平铺树" << std::endl;
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

        split_info split = width > height ? SPLIT_H : SPLIT_V;

        // 3.2 移回普通场景树
        // 2. 将其场景节点移回 "tiled" 渲染层
        wlr_scene_node_reparent_(get_wlr_scene_tree_node(container->toplevel->scene_tree), server.tiled_layer);

        std::cout<< "移回tiled" << std::endl;

        // 3.2 合并容器
        manager.attach(container, target_container, split);
    }

    // 4. 无论何种情况, 都需要通知布局更新
    manager.reflow(0, {0,0, display_width, display_height});
    std::cout << "更新布局" << std::endl;
}