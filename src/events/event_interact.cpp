#include "include/events.h"
#include "include/core.hpp"
#include "include/interact.hpp"
#include "include/decoration.hpp"

#include "server.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "types.h"
#include "window_util.hpp"
#include "wlr/util/edges.h"
#include "wlr/util/log.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

using namespace tiley;

/*********重要: 处理鼠标拖动窗口移动的函数***********/
static void process_cursor_move(TileyServer& server){
    // 这个函数也是我们平铺式需要重写逻辑的地方
    // Hyprland的逻辑: 
    // "抓住"窗口 -> 
    // 缩放窗口至移动状态的大小 -> 
    // 将他在二分树中的父节点扩大两倍, 占据自己的位置 -> 
    // 根据鼠标释放点确定"槽位"

    struct surface_toplevel* toplevel = server.grabbed_toplevel;

    // 将窗口移动到: 光标新位置(绝对) - 光标距窗口边界距离
    // 可以分步理解:
    // 1. 窗口要跟随光标, 所以移动到光标的绝对新位置;
    // 2. 但是鼠标拖动的地方距离窗口的边框是有点距离的, 所以还需要减去这个距离, 
    // 得到的才是平滑移动的效果, 不然窗口左上角就会直接闪现到光标处。
    wlr_scene_node_set_position_(get_wlr_scene_tree_node(toplevel->scene_tree), 
        server.cursor->x - server.grab_x,
        server.cursor->y - server.grab_y     
    );

}


static void process_cursor_resize(TileyServer& server, WindowStateManager& manager){
    struct surface_toplevel* toplevel = server.grabbed_toplevel;

    // 计算机坐标系从屏幕左上角开始向右为x正方向, 向下为y正方向。在平铺式管理器中, 我们遵循一条法则:
    // 调整该窗口所在容器A的分割比例和其祖父容器B(A的父容器)的分割比例即可。(Hyprland的行为)
    // 例如现在有如下容器树(H=horizontal, 横向分割; V=vertical, 纵向分割):
    //       root
    //        |
    //        H
    //       / \
    //      1   V
    //         / \
    //        2   3
    // 如果我们调整窗口2的大小, 则会调整V的纵向分割比例和H的横向分割比例; 调整3同理;
    // 如果我们调整窗口1的大小, 则会调整H的横向分割比例, 但由于其祖父容器为桌面, 就只能调整横向分割比例了。
    // 此外, 如果有更多层:
    //       root
    //        |
    //        H1
    //       / \
    //      1   V
    //         / \
    //        2   H2
    //           /  \
    //          3    4
    // 此时调整4的大小, 只会影响到H2和V的比例, 而不会影响H1的比例, 调整3同理。


    // grab_x, grab_y: 鼠标相对于拖动窗口左上角的"拖动位置", 是相对坐标, 不考虑窗口自身左上角的绝对坐标。
    // 例如(50, 30)不是指拖动位置离屏幕左上角(50,30), 而是离窗口左上角(50, 30)


    // 1. 判断窗口类型并取得父容器, 参数检查
    // 1.1 如果是浮动窗口, 则使用堆叠管理器的调整逻辑
    // 2. 根据拖动方向确定修改对象。对应关系是: 左右拖动影响H容器, 上下拖动影响V容器, 且优先父容器, 其次祖父容器
    // 3. 根据拖动位移决定新的分割比例: 被拖动的容器比例 = 鼠标对应方向相对容器边缘距离 / 对应方向容器大小
    // 3.1 如果被拖动的容器是桌面, 则跳过
    // 4. 触发重绘

    // 1
    auto container = toplevel->container;

    // 1.1
    if(container->floating == STACKING){
        process_cursor_resize_floating(server);
        return;
    }

    assert(container->toplevel!=nullptr);  // 拖动的一定是一个窗口, 所以父节点一定是容器并且不是桌面
    auto parent = container->parent;
    auto grand_parent = parent->parent;  // 祖父节点有可能是桌面, 下面将判断, 如果是则跳过

    // 2
    if(server.resize_edges & WLR_EDGE_TOP || server.resize_edges & WLR_EDGE_BOTTOM){   //上下调整
        auto target = (parent->split == SPLIT_V) ? 
        parent : (grand_parent->split == SPLIT_V) ? grand_parent : nullptr;   //TODO: 也许可以换成位掩码, 简化一下
        
        double new_ratio = 0.0f;

        if(target != nullptr){  //有调整目标(不是桌面)
            // 3
            int total_height = target->geometry.height;
            double offset = server.cursor->y - target->geometry.y;
            new_ratio = offset / total_height;
            target->split_ratio = std::min(0.9, std::max(0.1, new_ratio));
        }

    }else if(server.resize_edges & WLR_EDGE_LEFT || server.resize_edges & WLR_EDGE_RIGHT){    //左右调整
        auto target = (parent->split == SPLIT_H) ?
        parent : (grand_parent->split == SPLIT_H) ? grand_parent : nullptr;

        double new_ratio = 0.0f;

        if(target != nullptr){
            // 3
            int total_width = target->geometry.width;
            double offset = server.cursor->x - target->geometry.x;
            new_ratio = offset / total_width;
            target->split_ratio = std::min(0.9, std::max(0.1, new_ratio));  //限制范围在[0.1, 0.9]之间
        }
        
    }

    // 4
    int workspace = manager.get_current_workspace();
    wlr_box display_geometry = get_display_geometry_box(server);
    manager.reflow(workspace, display_geometry);

    // 5 如果窗口浮动, 则同步更新边框
    if(toplevel->container->floating == STACKING){
        update_toplevel_decoration(toplevel);
    }
}

/*********鼠标处理: 通用的鼠标位置移动后的处理函数**********/
static void process_cursor_motion(TileyServer& server, WindowStateManager& manager, uint32_t time){
    // 2.1 首先判断鼠标在干什么: 是正常移动, 缩放窗口大小还是在移动窗口
    // 对于平铺式而言, 这里是主要的逻辑变化之处:
    // a. 移动窗口只能是切换"槽位", 而不能任意移动坐标
    // b. 缩放一个窗口大小会影响其他窗口的大小

    if(server.cursor_mode == tiley::TILEY_CURSOR_MOVE){
        if(manager.is_alt_down){
            process_cursor_move(server);
        }
        return;
    }else if(server.cursor_mode == tiley::TILEY_CURSOR_RESIZE){
        //TODO: 缩放窗口, 同时影响其他窗口
        process_cursor_resize(server, manager);
        return;
    }

    // 2.2 找到鼠标所在的窗口, 如果没有窗口(或者没有在控件上), 则显示默认光标。
    double sx, sy;
    struct wlr_seat* seat = server.seat;
    struct wlr_surface* surface = NULL;
    struct surface_toplevel* toplevel = desktop_toplevel_at(server, server.cursor->x, server.cursor->y, &surface, &sx, &sy);  //TODO: 编写查找目前用户正在交互的窗口的逻辑
    if(!toplevel){
        wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");
    }

    // 2.3 如果鼠标在一个窗口上, 并且该窗口未激活(focus_toplevel内部判断), 则激活该窗口
    focus_toplevel(toplevel);

    // 2.4 如果鼠标当前在一个surface上(surface是窗口里面的一个可绘制的小区域。一个按钮, 一个菜单都可以是一个surface)
    // 则通知该surface鼠标移入事件和鼠标移动事件。
    // 例如浏览器中, 类似于javascript中的element.onmousemove这样的事件监听器就会收到该事件。
    if(surface){
        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(seat, time, sx, sy);
    }else{
        // 如果找不到surface, 说明光标移出去了, 此时清空上一个surface的监听。
        // javascript中的element.onmouseleave就会触发。
        wlr_seat_pointer_clear_focus(seat);
    }
}

// 重置鼠标模式(从拖拽、改变大小模式复原成正常模式)
static void reset_cursor_mode(TileyServer& server){
    server.cursor_mode = tiley::TILEY_CURSOR_PASSTHROUGH;
    server.grabbed_toplevel = NULL;
}

void server_cursor_motion(struct wl_listener* _, void* data){
    //TODO: 这里写当光标发出相对移动事件时的处理逻辑
    //相对移动: 我想从坐标(x,y)左移100, 上移200(x-100, y-200)

    // 客户端函数

    // 大致干下面的事情:
    // 1. 根据事件传入的移动量移动光标
    // 2. 处理移动后要发生的事情

    wlr_log(WLR_DEBUG, "收到光标相对移动事件");

    TileyServer& server = TileyServer::getInstance();
    WindowStateManager& manager = WindowStateManager::getInstance();
    struct wlr_pointer_motion_event* event = 
        static_cast<wlr_pointer_motion_event*>(data);
    
    // 1
    wlr_cursor_move(server.cursor, &event->pointer->base, 
        event->delta_x, event->delta_y);

    // 2
    // 跳转至process_cursor_motion继续逻辑...
    process_cursor_motion(server, manager, event->time_msec);

}

void server_cursor_motion_absolute(struct wl_listener* _, void* data){
    // TODO: 这里写当光标发出绝对移动事件时的处理逻辑
    // 绝对移动: 我想移动到屏幕坐标(200,300), 直接"飞"过去
    // 绝对移动可能出现在两种情况下:
    
    // · 当我们的管理器在开发阶段还运行在其他桌面环境下(比如hyprland, KDE等)时, 对于
    // Tiley而言, 它是不知道外界的情况的, 对于它而言鼠标直接凭空出现在了它的舞台范围内,
    // 而之前处于一个默认位置(或者如果之前进去过, 就在退出的位置, 这个和虚拟机的逻辑有点像, 参考虚拟机怎么处理鼠标进入/退出的)
    // , 此时就相当于从默认位置移动到了进去的位置;
    
    // · 用户真的在用绝对移动的设备, 比如画画的用户的手绘板(点哪里就到哪里)。如果没有这个功能,
    // 就没办法画画了:(
    // 屏幕的尺寸绘画设备不可能提前知道, 为了做到绝对移动, 手绘板等一般使用归一化坐标[0,1]
    // Tiley需要映射这个坐标到屏幕上的真实位置。

    TileyServer& server = TileyServer::getInstance();
    WindowStateManager& manager = WindowStateManager::getInstance();
    struct wlr_pointer_motion_absolute_event* event = 
        static_cast<wlr_pointer_motion_absolute_event*>(data);
    
    wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x, event->y);

    process_cursor_motion(server, manager, event->time_msec);
}

void server_cursor_button(struct wl_listener* _, void* data){
    //TODO: 这里写当光标按钮按下时的处理逻辑
    //data中包含具体的事件数据(哪个按钮, 是按下还是释放)
    TileyServer& server = TileyServer::getInstance();
    WindowStateManager& manager = WindowStateManager::getInstance();

    struct wlr_pointer_button_event* event = 
        static_cast<wlr_pointer_button_event*>(data);

    wlr_seat_pointer_notify_button(
        server.seat, 
        event->time_msec, 
        event->button, 
         event->state
    );

    if(event->state == WL_POINTER_BUTTON_STATE_RELEASED){
        // 如果之前正在移动窗口, 则执行重新插入逻辑
        // 1. 拿到正在移动的窗口
        area_container* moving_container = manager.moving_container();
        // 2. 拿到目标放置的位置
        // TODO: 默认0号工作区
        area_container* target_container = manager.desktop_container_at(server.cursor->x, server.cursor->y, 0);
        std::cout << "鼠标释放" << std::endl;
        if(moving_container != nullptr){
            // 3. 获取目标分割模式(仍然是长边分割)
            std::cout << "尝试合并回正在移动的窗口" << std::endl;
            wlr_box output_box = get_display_geometry_box(server);
            wlr_box target_box = get_target_geometry_box(target_container, output_box.width, output_box.height);
            split_info split = target_box.width > target_box.height ? SPLIT_H : SPLIT_V;
            // 4. 重新挂载节点
            int workspace = manager.get_current_workspace();
            manager.attach(moving_container, target_container, split, workspace);
            // 5. 通知布局更新
            manager.reflow(0, output_box);
            std::cout << "更新布局" << std::endl;
        }
        reset_cursor_mode(server);
    } else {
        // 聚焦于点击的窗口
        double sx, sy;
        struct wlr_surface* surface = NULL;
        struct surface_toplevel* toplevel = desktop_toplevel_at(server, server.cursor->x, server.cursor->y, &surface, &sx, &sy);
        // 判断是否要移动窗口
        if(manager.is_alt_down){

            std::cout << "要移动窗口" << std::endl;

            // 1. 调用detach分离窗口
            area_container* container = toplevel->container;
            
            // 只在第一次处理时分离窗口, 并且只能是平铺状态
            if(container->floating == NONE){
                manager.detach(container, MOVING);
            }

            // 2. 如果是移动窗口, 绘制临时窗口装饰(随时修改)
            
            // 3. 通知布局更新
            wlr_output* output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
            struct wlr_box output_box;
            wlr_output_layout_get_box(server.output_layout, output, &output_box);

            // 拿到工作区
            int workspace = manager.get_current_workspace();
            manager.reflow(workspace, output_box);
        }
    }

}

void server_cursor_axis(struct wl_listener* listener, void* data){
    //TODO: 这里写当光标滚动时(上下或者左右都有)时的处理逻辑
    //data中包含具体的事件数据(怎么滚动的, 滚动量是多少)
    
    // 客户端函数

    TileyServer& server = TileyServer::getInstance();

    struct wlr_pointer_axis_event* event =
        static_cast<wlr_pointer_axis_event*>(data);

    // 干下面的事:
    // 通知客户端即可。

    //传入:
    //  seat: 用户
    //  time_msec: 事件发生时刻
    //  orientation: 滚动方向
    //  value: 滚动量
    //  value_discrete: 离散滚动量(鼠标大多数同时支持连续和离散滚动量)
    //  source: 事件来源(哪个鼠标)
    //  relative_direction: 区分正常/反转滚动 

    wlr_seat_pointer_notify_axis(
        server.seat, 
        event->time_msec, 
        event->orientation, 
        event->delta, 
        event->delta_discrete, 
        event->source, 
        event->relative_direction
    );
}

void server_cursor_frame(struct wl_listener* _, void* data){

    (void)data;  //忽略多余参数

    //TODO: 这里写当一个frame完成时的处理逻辑
    //时钟同步(非常重要! 否则会导致很多奇怪的现象, 包括但不限于反应慢、事件卡顿等)
    
    TileyServer& server = TileyServer::getInstance();
    wlr_seat_pointer_notify_frame(server.seat);

    return;
}

void seat_request_cursor(struct wl_listener* _, void* data){
    //TODO: 这里写当请求更换光标图像时的处理逻辑
    TileyServer& server = TileyServer::getInstance();
    struct wlr_seat_pointer_request_set_cursor_event *event = 
        static_cast<wlr_seat_pointer_request_set_cursor_event*>(data);
    struct wlr_seat_client* focused_client = server.seat->pointer_state.focused_client;
    
    //只有当发送请求的客户窗口就是当前被激活的窗口(用户聚焦的窗口)时才处理
    if(focused_client == event->seat_client){
        wlr_cursor_set_surface(server.cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
    
    return;
}

void seat_request_set_selection(struct wl_listener* _, void* data){
    // TODO: 这里写当请求选中屏幕上的内容时的处理逻辑
    // 我们尊敬的用户想复制内容

    // 客户端函数

    // 干下面的事情:

    // 直接调用库中设置选中内容的函数即可。
    // 参数:
    // seat: 当前用户; source: 选中的数据; serial: 同上, 操作的"车票", 用于保证安全

    TileyServer& server = TileyServer::getInstance();

    struct wlr_seat_request_set_selection_event* event =
        static_cast<wlr_seat_request_set_selection_event*>(data);

    wlr_seat_set_selection(server.seat, event->source, event->serial);
}