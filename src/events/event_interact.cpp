#include "include/events.h"
#include <wayland-server-core.h>

void server_cursor_motion(struct wl_listener* listener, void* data){
    //TODO: 这里写当光标发出相对移动事件时的处理逻辑
    //相对移动: 我想从坐标(x,y)左移100, 上移200(x-100, y-200)
    return;
}

void server_cursor_motion_absolute(struct wl_listener* listener, void* data){
    //TODO: 这里写当光标发出绝对移动事件时的处理逻辑
    //绝对移动: 我想移动到屏幕坐标(200,300)
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

void seat_request_cursor(struct wl_listener* listener, void* data){
    //TODO: 这里写当请求更换光标图像时的处理逻辑
    return;
}

void seat_request_set_selection(struct wl_listener *listener, void *data){
    //TODO: 这里写当请求选中屏幕上的内容时的处理逻辑
    return;
}
