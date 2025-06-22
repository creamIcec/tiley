#include "include/events.h"
#include "include/core.hpp"
#include "include/interact.hpp"
#include "include/decoration.hpp"

#include "server.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "types.h"
#include "wlr/util/edges.h"
#include "wlr/util/log.h"
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


/*********重要: 处理鼠标调整窗口大小的函数*************/
static void process_cursor_resize(TileyServer& server){
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

/*********重要: 判断某个鼠标位置对应的窗口函数**********/

static struct surface_toplevel* desktop_toplevel_at(
    TileyServer& server, double lx, double ly,
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
        process_cursor_resize(server);
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
            wlr_output* output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
            int32_t display_width = output->width;
            int32_t display_height = output->height;

            int width, height;
            if(target_container->parent == nullptr){   // 为空说明只有桌面容器
                wlr_log(WLR_DEBUG, "合并窗口到桌面根节点");
                width = display_width;
                height = display_height; // 设置为桌面参数    
            } else {  //否则拿到窗口参数
                wlr_box bb = target_container->toplevel->xdg_toplevel->base->geometry;
                width = bb.width;
                height = bb.height;
            }

            split_info split = width > height ? SPLIT_H : SPLIT_V;

            // 4. 重新挂载节点
            int workspace = manager.get_current_workspace();
            manager.attach(moving_container, target_container, split, workspace);
            
            // 5. 通知布局更新
            manager.reflow(0, {0,0, display_width, display_height});
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
            int32_t display_width = output->width;
            int32_t display_height = output->height;

            // TODO: 仍然是默认0号工作区
            manager.reflow(0, {0,0,display_width,display_height});
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