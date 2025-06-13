#include "include/events.h"

void server_new_input(struct wl_listener* listener, void* data){
    // TODO: 这里写新设备插入时的逻辑
    // 键盘和鼠标输入注册的事件也应该写在这儿。这是为了节省资源: 有一个设备我们才处理一个,
    // 否则不处理。
    return;
}