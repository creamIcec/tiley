#include <wayland-server-core.h>
#include <wayland-util.h>

#include "include/events.h"

void server_new_output(struct wl_listener* listener, void* data){
    //TODO: 这里写当新的显示屏接入时的处理逻辑

    //客户端函数

    //大致来讲干下面的事情:
    // 1. 指定新的显示屏使用我们的buffer allocator和renderer
    // 2. 打开新的显示屏的输出状态
    // 3. 指定显示参数: (长度, 宽度, 刷新率)
    // 4. 提交渲染更新状态(虽然这里并不渲染任何东西, 但是要让服务端知道多了一个显示屏)
    // 5. 为新的显示屏注册客户端事件处理器: 刷新屏幕事件, 刷新屏幕状态事件和屏幕拔出事件
    // 6. 添加屏幕到输出布局中, 同时也告知客户端可以查询关于该屏幕的信息: 分辨率, 缩放比例, 名称, 制造商等等


    return;
}