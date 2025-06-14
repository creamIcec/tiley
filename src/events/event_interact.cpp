#include "include/events.h"
#include "server.hpp"
#include "types.h"
#include "wlr/util/log.h"
#include <cstdint>
#include <wayland-server-core.h>

using namespace tiley;

void server_cursor_motion(struct wl_listener* _, void* data){
    //TODO: 这里写当光标发出相对移动事件时的处理逻辑
    //相对移动: 我想从坐标(x,y)左移100, 上移200(x-100, y-200)

    // 客户端函数

    // 大致干下面的事情:
    // 1. 根据事件传入的移动量移动光标
    // 2. 处理移动后要发生的事情

    wlr_log(WLR_DEBUG, "收到光标相对移动事件");

    TileyServer& server = TileyServer::getInstance();
    struct wlr_pointer_motion_event* event = 
        static_cast<wlr_pointer_motion_event*>(data);
    
    // 1
    wlr_cursor_move(server.cursor, &event->pointer->base, 
        event->delta_x, event->delta_y);

    
    uint32_t time = event->time_msec;

    // 2
    // 2.1 首先判断鼠标在干什么: 是正常移动, 缩放窗口大小还是在移动窗口
    // 对于平铺式而言, 这里是主要的逻辑变化之处:
    // a. 移动窗口只能是切换"槽位", 而不能任意移动坐标
    // b. 缩放一个窗口大小会影响其他窗口的大小

    if(server.cursor_mode == tiley::TILEY_CURSOR_MOVE){
        //TODO: 切换槽位
        return;
    }else if(server.cursor_mode == tiley::TILEY_CURSOR_RESIZE){
        //TODO: 缩放窗口, 同时影响其他窗口
        return;
    }

    // 2.2 找到鼠标所在的窗口, 如果没有窗口(或者没有在控件上), 则显示默认光标。
    double sx, sy;
    struct wlr_seat* seat = server.seat;
    struct wlr_surface* surface = NULL;
    struct surface_toplevel* toplevel = nullptr;  //TODO: 编写查找目前用户正在交互的窗口的逻辑
    if(!toplevel){
        wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");
    }
    // 2.3 如果鼠标当前在一个surface上(surface是窗口里面的一个可绘制的小区域。一个按钮, 一个菜单都可以是一个surface)
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

void server_cursor_motion_absolute(struct wl_listener* listener, void* data){
    //TODO: 这里写当光标发出绝对移动事件时的处理逻辑
    //绝对移动: 我想移动到屏幕坐标(200,300), 直接"飞"过去
    //这个事件处理器在平铺式管理器中很常用
    return;
}

void server_cursor_button(struct wl_listener* listener, void* data){
    //TODO: 这里写当光标按钮按下时的处理逻辑
    //data中包含具体的事件数据(哪个按钮, 是按下还是释放)
    return;
}

void server_cursor_axis(struct wl_listener* listener, void* data){
    //TODO: 这里写当光标滚动时(上下或者左右都有)时的处理逻辑
    //data中包含具体的事件数据(怎么滚动的, 滚动量是多少)
    return;
}

void server_cursor_frame(struct wl_listener* listener, void* data){
    //TODO: 这里写当一个frame完成时的处理逻辑
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

void seat_request_set_selection(struct wl_listener *listener, void *data){
    //TODO: 这里写当请求选中屏幕上的内容时的处理逻辑
    return;
}
