#include "include/events.h"
#include "src/wrap/c99_unsafe_defs_wrap.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <wayland-server-core.h>
#include <wayland-util.h>

extern "C"{
    #include <wlr/util/log.h>
    #include <wlr/backend.h>
    #include <wlr/render/allocator.h>
    #include <wlr/types/wlr_compositor.h>
    #include <wlr/types/wlr_subcompositor.h>
    #include <wlr/types/wlr_data_device.h>
    #include <wlr/types/wlr_output_layout.h>
    #include <wlr/types/wlr_xdg_shell.h>
    #include <wlr/types/wlr_cursor.h>
    #include <wlr/types/wlr_xcursor_manager.h>
}

#define PROJECT_NAME "tiley"

enum tiley_cursor_mode {
    TILEY_CURSOR_PASSTHROUGH,
    TILEY_CURSOR_MOVE,
    TILEY_CURSOR_RESIZE,
};

int main(int argc, const char* argv[]){
    std::cout << "我们的桌面管理器从这里开始!" << std::endl;
    wlr_log_init(WLR_INFO, NULL);
    wlr_log(WLR_INFO, "这是一条来自wlroots的log打印, 如果你看到了这条打印信息, 说明wlroots依赖引用成功!");

    // 创建显示客户端
    wl_display* wl_display = wl_display_create();

    // 创建后端, 该后端是智能的, 会自动根据X11/Wayland的窗口类型调整打开的类型
    wlr_backend* backend = wlr_backend_autocreate(wl_display_get_event_loop(wl_display), NULL);

    if(backend == NULL){
        wlr_log(WLR_ERROR, "无法创建后端。请检查是否有显示服务器。");
        return 1;
    }

    wlr_renderer* renderer = wlr_renderer_autocreate(backend);

    wlr_renderer_init_wl_display(renderer, wl_display);

    // 创建渲染buffer管理器
    wlr_allocator* allocator = wlr_allocator_autocreate(backend, renderer);

    if(allocator == NULL){
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        return 1;
    }

    wlr_compositor_create(wl_display, 5, renderer);
    wlr_subcompositor_create(wl_display);
    wlr_data_device_manager_create(wl_display);


    /***********事件注册开始************/
    // 一共11个主要事件, 具体查看events.h中的说明

    wlr_output_layout* output_layout = wlr_output_layout_create(wl_display);

    wl_list outputs, toplevels;

    wl_list_init(&outputs);

    wl_listener new_output;

    new_output.notify = server_new_output;

    wl_signal_add(&backend->events.new_output, &new_output);

    wlr_scene* scene = wlr_scene_create_();

    wlr_scene_output_layout* scene_layout = wlr_scene_attach_output_layout_(scene, output_layout);

    wl_list_init(&toplevels);

    wlr_xdg_shell* xdg_shell = wlr_xdg_shell_create(wl_display, 3);

    wl_listener new_xdg_toplevel;

    new_xdg_toplevel.notify = server_new_xdg_toplevel;

    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);

    wl_listener new_xdg_popup;

    new_xdg_popup.notify = server_new_xdg_popup;

    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    wlr_cursor* cursor = wlr_cursor_create();

    wlr_cursor_attach_output_layout(cursor, output_layout);

    wlr_xcursor_manager* cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    tiley_cursor_mode cursor_mode = TILEY_CURSOR_PASSTHROUGH;

    wl_listener cursor_motion;

    cursor_motion.notify = server_cursor_motion;

    wl_signal_add(&cursor->events.motion, &cursor_motion);

    wl_listener cursor_motion_absolute;

    cursor_motion_absolute.notify = server_cursor_motion_absolute;

    wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);

    wl_listener cursor_button;

    cursor_motion.notify = server_cursor_button;

    wl_signal_add(&cursor->events.button, &cursor_button);

    wl_listener cursor_axis;

    cursor_axis.notify = server_cursor_axis;

    wl_signal_add(&cursor->events.axis, &cursor_axis);

    wl_listener cursor_frame;

    cursor_frame.notify = server_cursor_frame;

    wl_signal_add(&cursor->events.frame, &cursor_frame);

    wl_list keyboards;

    wl_list_init(&keyboards);

    wl_listener new_input;

    new_input.notify = server_new_input;

    wl_signal_add(&backend->events.new_input, &new_input);

    wlr_seat* seat = wlr_seat_create(wl_display, "seat0");

    wl_listener request_cursor;

    request_cursor.notify = seat_request_cursor;

    wl_signal_add(&seat->events.request_set_cursor, &request_cursor);

    wl_listener request_set_selection;

    request_set_selection.notify = seat_request_set_selection;

    wl_signal_add(&seat->events.request_set_selection, &request_set_selection);

    /***********事件注册结束************/

    /***********启动准备开始************/

    // 连接到Wayland服务器
    const char* socket = wl_display_add_socket_auto(wl_display);

    if(!socket){
        wlr_backend_destroy(backend);
        return 1;
    }

    // 启动后端。具体逻辑可以查看start内编写的内容
    // 这里的逻辑是:
    // 1. 尝试启动后端, 并接受true/false返回值表示是否启动成功
    // 2. 如果不成功, 则进行一些清理程序, 再直接停止整个程序
    // TODO: 逻辑优化: 当启动不成功时, 添加报错信息。
    if(!wlr_backend_start(backend)){
        wlr_backend_destroy(backend);
        wl_display_destroy(wl_display);
        return 1;
    }

    // 设置"WAYLAND_DISPLAY"环境变量, 使其指向tiley.
    // 该环境变量的目的是告诉系统: 我想注册一个新的管理器, id是socket。
    // 参考: https://wayland-book.com/wayland-display/creation.html
    // https://wayland.arktoria.org/4-wayland-display/creation.html
    setenv("WAYLAND_DISPLY", socket, true);

    
    /***********启动准备结束*************/

    /**************启!动!***************/

    // 启动事件循环, while(true)这个万物之源:)
    // 该函数即为整个应用的主函数, 除非用户要求退出/出现错误, 否则不会退出。
    // 主要完成的工作即是我们在上面编写的各种输入处理事件循环, 不停地发出frame事件等等。
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
    
    wl_display_run(wl_display);
    
    // 当用户退出程序...

    /**********退出清理工作开始**********/

    // 关闭所有客户端, 最后关闭服务器
    wl_display_destroy_clients(wl_display);

    // 关闭创建新窗口的功能
    wl_list_remove(&new_xdg_toplevel.link);
    wl_list_remove(&new_xdg_popup.link);

    // 关闭鼠标输入
    wl_list_remove(&cursor_motion.link);
    wl_list_remove(&cursor_motion_absolute.link);
    wl_list_remove(&cursor_button.link);     
    wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_frame.link);
    
    // 关闭新的输入设备接入监听
    wl_list_remove(&new_input.link);

    // 关闭交互功能
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);

    // 关闭新的显示屏接入监听
    wl_list_remove(&new_output.link);

    // 销毁场景对象树
    wlr_scene_node_destroy_(get_wlr_scene_node(scene));

    // 销毁光标尺寸管理器
    wlr_xcursor_manager_destroy(cursor_mgr);

    // 销毁光标
    wlr_cursor_destroy(cursor);

    // 销毁渲染buffer管理器
    wlr_allocator_destroy(allocator);

    // 销毁渲染器
    wlr_renderer_destroy(renderer);

    // 关闭后端服务器
    wlr_backend_destroy(backend);

    // 退出tiley
    wl_display_destroy(wl_display);

    return 0;

}