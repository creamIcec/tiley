#include "include/interact.hpp"
#include "include/server.hpp"
#include "include/core.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "types.h"
#include "include/decoration.hpp"
#include "include/window_util.hpp"

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

    wlr_box output_box = get_display_geometry_box(server);


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

    bool stacking_flag = false;

    // 在hyprland中, 按此优先级, 高的会覆盖低的。比如如果用户正在移动窗口, 但在移动过程中按下了悬挂快捷键, 会马上停止移动, 在当时的位置变成悬挂。
    
    wlr_box target_box;

    if(container->floating == NONE){
        manager.detach(container, STACKING);
        stacking_flag = true;   
    }else if(container->floating == MOVING){
        manager.stop_moving(); //清除移动状态
        container->floating = STACKING;
        stacking_flag = true;
    }else{   // 3. 如果就是STACKING, 说明要切换回去, 则调用attach合并
        // 3.1 获取鼠标位置的container
        area_container* target_container = manager.desktop_container_at(server.cursor->x, server.cursor->y, workspace);

        std::cout<< "窗口处于堆叠模式, 合并回平铺树" << std::endl;
        target_box = get_target_geometry_box(target_container, output_box.width, output_box.height);

        int width = target_box.width;
        int height = target_box.height;

        std::cout<< "拿到窗口参数" << std::endl;

        split_info split = width > height ? SPLIT_H : SPLIT_V;

        // 3.2 移回普通场景树
        // 2. 将其场景节点移回 "tiled" 渲染层
        wlr_scene_node_reparent_(get_wlr_scene_tree_node(container->toplevel->scene_tree), server.tiled_layer);

        std::cout<< "移回tiled" << std::endl;

        // 3.2 合并容器
        manager.attach(container, target_container, split, workspace);
    }

    surface_toplevel* toplevel = container->toplevel;

    create_toplevel_decoration(toplevel);

    if(stacking_flag){
        update_toplevel_decoration(toplevel);
        set_toplevel_decoration_enabled(toplevel, true);
    }else{
        set_toplevel_decoration_enabled(toplevel, false);
    }


    // 4. 无论何种情况, 都需要通知布局更新
    manager.reflow(0, output_box);
    std::cout << "更新布局" << std::endl;


}

/*********重要: 判断某个鼠标位置对应的窗口函数**********/

struct surface_toplevel* desktop_toplevel_at(
    tiley::TileyServer& server, double lx, double ly,
    struct wlr_surface* *surface, double* sx, double* sy
){
    // 这个函数非常重要, 它负责从场景图中找到我们现在传入的坐标对应的窗口是谁。
    // 这和我们自定义的坐标安排逻辑并不冲突, 因为它只负责找, 不负责设置。
    // 打个比方, 场景图是剧场中的舞台, 每个窗口是台上的演员。
    // 我们的平铺式布局算法是导演, 负责把每个演员安排到对应的位置(通过
    // 一个set函数(wlr_scene_node_set_position))告诉场务每个窗口的位置在哪儿; 
    // 而场景图的wlr_scene_node_at则是场务, 从小本本上根据坐标找到对应窗口。

    // 所以与其说wlr_scene是一个完整的工具类, 不如简单认为他只是一个数据结构,
    // 只能靠我们去安排, 它不负责安排, 只负责获取。这是职责分离的一个体现, 也是
    // 对不同布局算法都支持的灵活性体现。场务如何保存位置和我们无关, 不假设我们
    // 一定是堆叠式管理器, 只负责从我们告诉他的数据中找到对应的东西, 数据都来自于
    // 我们。

    // lx: 鼠标横坐标; ly: 鼠标纵坐标; &sx,&sy: 作为引用带回, 窗口左上角起始坐标。

    wlr_log(WLR_DEBUG, "开始目标窗口查找");

    struct wlr_scene_node* node = wlr_scene_node_at_(get_wlr_scene_root_node(server.scene), lx, ly, sx, sy);
    if(node == NULL || get_toplevel_node_type_(node) != WLR_SCENE_NODE_BUFFER_){
        return NULL;     
    }

    struct wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node_(node);
    struct wlr_scene_surface* scene_surface = 
        wlr_scene_surface_try_from_buffer_(scene_buffer);

    wlr_log(WLR_DEBUG, "获取到surface的buffer");

    if(!scene_surface){
        return NULL;
    }
    
    // 将surface指向获取到的surface
    // 注意(看形参): 这里的surface是个二重指针
    *surface = get_surface_from_scene_surface(scene_surface);

    wlr_log(WLR_DEBUG, "获取到surface");

    struct wlr_scene_tree* tree = get_parent(node);

    wlr_log(WLR_DEBUG, "获取到父窗口");

    // C/C++兼容trick太多了...
    while(tree != NULL && get_tree_node_data(get_wlr_scene_tree_node(tree)) == NULL){
        wlr_log(WLR_DEBUG, "查找");
        set_tree(&tree, get_parent(get_wlr_scene_tree_node(tree)));
    }

    wlr_log(WLR_DEBUG, "已完成目标窗口查找");

    if(tree == NULL){
        return NULL;
    }

    return static_cast<surface_toplevel*>(
        get_tree_node_data(get_wlr_scene_tree_node(tree))
    );
}

// 处理处于浮动图层的窗口大小调整的逻辑
void process_cursor_resize_floating(tiley::TileyServer& server){
    struct surface_toplevel* toplevel = server.grabbed_toplevel;

    // 需要分情况。在计算机显示坐标系中, 坐标轴从左上角开始向右向下:
    // · 如果调整左边界: 移动左上角位置 + 更改大小
    // · 如果调整上边界: 移动左上角位置 + 更改大小
    // · 如果是其他两个边界: 仅更改大小
    
    // 1. 同移动的逻辑, 得到目标的绝对坐标
    double border_x = server.cursor->x - server.grab_x;
    double border_y = server.cursor->y - server.grab_y;

    // 2. 先存储先前的坐标
    int new_left = server.grab_geobox.x;
    int new_right = server.grab_geobox.x + server.grab_geobox.width;
    int new_top = server.grab_geobox.y;
    int new_bottom = server.grab_geobox.y + server.grab_geobox.height;

    // 3 调整大小
    if(server.resize_edges & WLR_EDGE_TOP){
        new_top = border_y;  //将窗口上边界设为目标y
        if(new_top >= new_bottom){
            new_top = new_bottom - 1;  //如果鼠标在往下面拖动上边界并且超过了下边界, 则锁死在离下边缘1像素的地方, 防止变成负数
        }
    } else if (server.resize_edges & WLR_EDGE_BOTTOM){
        new_bottom = border_y;
        if(new_bottom <= new_top){
            new_bottom = new_top + 1;
        }
    }

    // 调整上下边界和左右边界之间可以同时发生, 所以这里另起if

    if(server.resize_edges & WLR_EDGE_LEFT){
        new_left = border_x;
        if(new_left >= new_right){
            new_left = new_right - 1;
        }
    } else if (server.resize_edges & WLR_EDGE_RIGHT){
        new_right = border_x;
        if(new_right <= new_left){
            new_right = new_left + 1;
        }
    }

    // 4 调整位置
    struct wlr_box* geo_box = &toplevel->xdg_toplevel->base->geometry;
    wlr_scene_node_set_position_(get_wlr_scene_tree_node(toplevel->scene_tree), 
        new_left - geo_box->x, 
        new_top - geo_box->y
    );

    // 更新大小
    int new_width = new_right - new_left;
    int new_height = new_bottom - new_top;

    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_width, new_height);

    // 5 如果窗口浮动, 则同步更新边框
    if(toplevel->container->floating == STACKING){
        update_toplevel_decoration(toplevel);
    }
}