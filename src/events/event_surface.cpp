#include "include/events.h"
#include "include/server.hpp"
#include "include/interact.hpp"
#include "include/core.hpp"
#include "include/types.h"

#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "wlr/util/box.h"
#include "wlr/util/edges.h"
#include "wlr/util/log.h"

#include <cassert>
#include <cstdlib>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <iostream>

using namespace tiley;

static void reset_cursor_mode(TileyServer& server){
    server.cursor_mode = tiley::TILEY_CURSOR_PASSTHROUGH;
    server.grabbed_toplevel = nullptr;
}

static void begin_interactive(struct surface_toplevel* toplevel,
    enum tiley_cursor_mode mode, uint32_t edges){
    // TODO: 这个是tinywl的函数, 这里面涉及到的移动和缩放操作
    // 在平铺式和堆叠式中行为大相径庭. 为了完整性暂时使用tinywl的写法
    // 之后修改为平铺式的行为
    
    TileyServer& server = TileyServer::getInstance();
    server.grabbed_toplevel = toplevel;
    server.cursor_mode = mode;

    if(mode == tiley::TILEY_CURSOR_MOVE){
        // 计算相对窗口左上角的"拖拽位置"
        server.grab_x = server.cursor->x - get_scene_tree_node_x(toplevel);
        server.grab_y = server.cursor->y - get_scene_tree_node_y(toplevel);
    }else{
        // 获得窗口的碰撞箱
        struct wlr_box* geo_box = &toplevel->xdg_toplevel->base->geometry;

        // wlroots用一个4位二进制数(相当于4个标志位)表示拖拽的边缘, 以便使用位运算简化逻辑(允许同时操作多条边)
        // 0000按顺序代表上,下,左,右
        
        // 根据
        double border_x = (get_scene_tree_node_x(toplevel) + geo_box->x)
            + ((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
        double border_y = (get_scene_tree_node_y(toplevel) + geo_box->y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
        

        server.grab_x = server.cursor->x - border_x;
        server.grab_y = server.cursor->y - border_y;

        server.grab_geobox = *geo_box;
        server.grab_geobox.x += get_scene_tree_node_x(toplevel);
        server.grab_geobox.y += get_scene_tree_node_y(toplevel);

        server.resize_edges = edges;
    }
    
}

/*********弹出窗口surface新帧待渲染事件*************/

static void xdg_popup_commit(struct wl_listener* listener, void* data){
    // 同toplevel_commit
    
    struct surface_popup* popup = wl_container_of(listener, popup, commit);

    if(popup->xdg_popup->base->initial_commit){
        // 排入待处理队列
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

static void xdg_popup_destroy(struct wl_listener* listener, void* data){
    // 同toplevel destroy
	struct surface_popup* popup = wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);

	free(popup);
}

/*********窗口渲染状态改变事件**********/

static void xdg_toplevel_map(struct wl_listener* listener, void* _){
    // 当一个窗口准备好在屏幕上渲染时触发(例如刚打开)
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, map);

    TileyServer& server = TileyServer::getInstance();

    // 1. 将这个窗口插入到服务器管理列表中
    wl_list_insert(&TileyServer::getInstance().toplevels, &toplevel->link);

    // 2. 执行操作: 使用户聚焦到这个窗口
    focus_toplevel(toplevel);

    // 3. 提升节点到场景树最顶层
    wlr_scene_node_raise_to_top_(get_toplevel_node(toplevel));

    // 4. 同步提升窗口到数据树最顶层
    wl_list_remove(&toplevel->link);
    wl_list_insert(&server.toplevels, &toplevel->link);

    // 5. 激活窗口
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);

    // 6. 键盘聚焦. 聚焦后, wlroots会自动管理后续的键盘输入事件的目标, 因此不用在每次
    // 键盘输入之前都检查目标.
    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(server.seat);
    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;

    if(keyboard != NULL){
        wlr_seat_keyboard_notify_enter(server.seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }

}

static void xdg_toplevel_unmap(struct wl_listener* listener, void* data){
    // 当一个窗口不再应该被显示时触发(例如最小化到后台)
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, unmap);

    TileyServer& server = TileyServer::getInstance();

    // 1. 执行操作:
    // 1.1 如果正在操作一个窗口(拖动, 调整大小等), 将光标恢复到默认模式
    if (toplevel == server.grabbed_toplevel){
        reset_cursor_mode(server);
    }
    // 1.2 将光标图标恢复到默认图标
    // 1.3 平铺式特色: 通知其他窗口调整大小
    // 1.4 (是否需要手动调用?) 触发鼠标进入其他窗口事件
    
    // 2. 从服务器中移除该窗口
    wl_list_remove(&toplevel->link);
}

static void xdg_toplevel_commit(struct wl_listener* listener, void* data){
    // 当新的帧被提交待渲染时触发
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, commit);

    TileyServer& server = TileyServer::getInstance();
    
    // 如果是第一次渲染
    if(toplevel->xdg_toplevel->base->initial_commit){
        // 这里我们可以进行一些窗口显示配置, 例如设置窗口大小等等
        // 这个对于平铺式管理非常重要, 因为在平铺式管理中, 对于普通窗口, 
        // 我们不能让它自由决定自己的大小, 而是为其分配树中的下一块区域(对半)
        // FIXME: 平铺式决定窗口大小。

        // 获取鼠标所在的显示器的大小
        wlr_output* output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
        int32_t display_width = output->width;
        int32_t display_height = output->height;

        WindowStateManager& manager = WindowStateManager::getInstance();

        wlr_log(WLR_DEBUG, "获取显示器大小");

        // 设置窗口大小

        // 1. 根据鼠标位置取得窗口尺寸
        // FIXME: 暂时是0号workspace

        int width, height;

        area_container* old_container = manager.desktop_container_at(server.cursor->x, server.cursor->y, 0);
        //std::cout << "鼠标所在对象地址:" << old_container << std::endl;
        if(old_container->parent == nullptr){   // 为空说明只有桌面容器
            wlr_log(WLR_DEBUG, "打开新窗口: 之前是桌面");
            width = display_width;
            height = display_height; // 设置为桌面参数    
        } else {  //否则拿到窗口参数
            wlr_box bb = old_container->toplevel->xdg_toplevel->base->geometry;
            width = bb.width;
            height = bb.height;
        }

        wlr_log(WLR_DEBUG, "获得父容器");

        // 2. 根据窗口的长边得到分割方式
        split_info split = width > height ? SPLIT_H : SPLIT_V;

        wlr_log(WLR_DEBUG, "创建新节点");

        // 3. 拿到toplevel的container并插入容器树
        manager.insert(toplevel->container, old_container, split);

        wlr_log(WLR_DEBUG, "插入容器树");

        // 5. 重新计算布局
        manager.reflow(0, {0,0,display_width,display_height});

        wlr_log(WLR_DEBUG, "重新计算布局");
    }
}

static void xdg_toplevel_destory(struct wl_listener* listener, void* data){
    // 当窗口因为进程结束而退出时触发
    // 注意: 和unmap的区别在于, unmap只是暂时不渲染它了(有可能是因为最小化等原因), 但是它的进程还在;
    // destroy是整个窗口对应的进程都退出了
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, destroy);

    WindowStateManager& manager = WindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    // 1. 执行从树中移除节点的操作
    if (toplevel->container) {
        manager.remove(toplevel->container);
    }

    // 2. 触发重新计算布局
    
    wlr_output* output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
    //struct output_display* display = wl_container_of(output, display, wlr_output);
    //int workspace = manager.get_workspace_by_output_display(display);
    int workspace = 0;  //TODO: 上述代码有问题, 暂时不启用, 默认用workspace0
    std::cout << "获取workspace完成" << std::endl;
    if(workspace != -1){
        manager.reflow(workspace, {0,0, output->width, output->height});
    }

    std::cout << "重新计算布局完成" << std::endl;

    // 3. TODO: 平铺式常见做法: 移动鼠标到兄弟窗口

    // 执行一系列销毁操作
    wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);

    // 释放窗口对象的内存
    free(toplevel);
}


/***********窗口交互事件***********/

static void xdg_toplevel_request_move(struct wl_listener* listener, void* data){
    // 当窗口尝试移动时触发
    
    // FIXME: 使用更加完善的校验机制, 确保移动操作是由用户主动发出的/允许程序发出, 
    // 而不是被恶意程序劫持等. 如果用户不想移动一个窗口, 但是窗口却自己移动了,
    // 这会干扰甚至气死用户(). 我们可以通过类似于车票的机制进行校验. 票证相符的才能移动:
    // 当用户按下移动窗口的按键时, 给这个窗口一张票, 然后窗口想要移动的时候验票
    
    // TODO: 加入平铺式的快捷键判定. 在主流的平铺式管理器中(比如hyprland, sway), 一般不能直接通过鼠标
    // 移动窗口, 需要同时按住一个快捷键. 我们遵循社区主流规范.
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, request_move);
    
    // 改成0b0000可能清晰一些, 表示四个标志位
    begin_interactive(toplevel, tiley::TILEY_CURSOR_MOVE, 0b0000);
}


static void xdg_toplevel_request_resize(struct wl_listener* listener, void* data){
    // 当窗口尝试调整大小时触发

    // FIXME: 同上request_move, 需要校验操作合法性
    struct wlr_xdg_toplevel_resize_event* event = 
        static_cast<wlr_xdg_toplevel_resize_event*>(data);
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, request_resize);
    begin_interactive(toplevel, tiley::TILEY_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maxmize(struct wl_listener* listener, void* data){
    // TODO: 平铺式特色: 改为全屏
    // 改为全屏后, 这个事件就可以忽略了
    
    // FIXME: 同上, 需要校验操作合法性
    struct surface_toplevel* toplevel = wl_container_of(listener, toplevel, request_maximize);
    if(toplevel->xdg_toplevel->base->initialized){
        // 我们暂时礼貌拒绝客户端: 抱歉现在还不支持全屏...之后一定会有的
        // 发送一个不含任何具体配置的配置指令, 让客户端不再苦苦等待
        // 如果不发送, 则客户端会一直等着服务器给他回应
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
    }

}

static void xdg_toplevel_request_fullscreen(
		struct wl_listener *listener, void *data) {
	// TODO: 平铺式特色: 改为全屏
    // 改为全屏后, 上面的最大化事件就可以忽略了

	struct surface_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);

	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
}


void server_new_xdg_toplevel(struct wl_listener* _, void* data){
    //TODO: 这里写当新窗口创建时的处理逻辑
    
    // 客户端函数

    // 干下面的事情:
    // 1. 为新的窗口分配一块surface和以该窗口为root的场景子树
    // 2. 添加窗口到场景树中, 并为其生成新的子树
    // 3. 注册事件
    
    wlr_log(WLR_DEBUG, "触发新的窗口打开");


    TileyServer& server = TileyServer::getInstance();
    WindowStateManager& manager = WindowStateManager::getInstance();
    struct wlr_xdg_toplevel* xdg_toplevel = 
        static_cast<wlr_xdg_toplevel*>(data);

    // 1
    struct surface_toplevel* toplevel = 
        static_cast<surface_toplevel*>(calloc(1, sizeof(surface_toplevel)));

    toplevel->xdg_toplevel = xdg_toplevel;
    toplevel->scene_tree = 
        wlr_scene_xdg_surface_create_(get_wlr_scene_tree(server.scene), xdg_toplevel->base);
    toplevel->container = manager.create_toplevel_container(toplevel);  //为toplevel分配一个container

    wlr_log(WLR_DEBUG, "新窗口分配新子树完成");
    
    // 2

    // 这里我们需要绕开C/C++的隔离限制
    set_tree_node_data(toplevel);
    xdg_toplevel->base->data = toplevel->scene_tree;

    wlr_log(WLR_DEBUG, "完成节点逻辑数据挂载");

    // 3
    // 3.1 注册开始渲染/停止渲染事件
    toplevel->map.notify = xdg_toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
    toplevel->unmap.notify = xdg_toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);

    // 3.2 注册有新的帧提交事件
    toplevel->commit.notify = xdg_toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

    // 3.3 注册窗口销毁事件
    toplevel->destroy.notify = xdg_toplevel_destory;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

    // 3.4 注册窗口操作事件
    // 移动窗口
    toplevel->request_move.notify = xdg_toplevel_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
    // 调整窗口大小
    toplevel->request_resize.notify = xdg_toplevel_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
    // 最大化
    toplevel->request_maximize.notify = xdg_toplevel_request_maxmize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
    // 全屏
    toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);

    wlr_log(WLR_DEBUG, "完成新窗口事件注册");

}

void server_new_xdg_popup(struct wl_listener* listener, void* data){
    //TODO: 这里写当新的弹出界面创建时的处理逻辑

    //客户端函数

    // 干下面的事情:
    // 1. 分配新的弹出窗口管理对象内存
    // 2. 指定这个弹出窗口的父窗口节点
    // 3. 注册创建和销毁事件

    struct wlr_xdg_popup* xdg_popup = 
        static_cast<wlr_xdg_popup*>(data);

    // 1
    struct surface_popup* popup = 
        static_cast<surface_popup*>(calloc(1, sizeof(surface_popup)));

    popup->xdg_popup = xdg_popup;

    // 2
    struct wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);

    assert(parent!=NULL);  //这个assert可以改成其他更友好的方式

    struct wlr_scene_tree* parent_tree = 
        static_cast<wlr_scene_tree*>(parent->data);

    xdg_popup->base->data = wlr_scene_xdg_surface_create_(parent_tree, xdg_popup->base);

    popup->commit.notify = xdg_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    popup->destroy.notify = xdg_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);

}