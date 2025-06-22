#include "include/events.h"
#include "include/server.hpp"
#include "wlr/util/log.h"
#include "include/hotkey.hpp"
#include <bits/getopt_core.h>
#include <cstddef>
#include <cstdlib>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <getopt.h>
#include <thread>
#define PROJECT_NAME "tiley"

/* 处理启动参数, 目前唯一功能是根据启动参数决定是否输出"DEBUG"等级的log.
   为了逻辑配套, 建议我们在开发的时候打印的为了调试(例如, 检查某个事件是否正常触发, 打印某些数据)的log
   都使用WLR_DEBUG这个等级。这样便可以通过设置"--debug"参数一键切换是否输出这些日志, 避免了手动删除打印代码
*/
void setup_params(int argc, char* argv[]){

    bool enable_debug = false;
    
    int c;
    // https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html
    struct option longopts[] = {
        {"debug", no_argument, NULL, 'd'},
        {0,0,0,0}
    };

    while((c = getopt_long(argc, argv, "ds:", longopts, NULL)) != -1){
        switch(c){
            case 'd':
                enable_debug = true;
                break;
            default:
                break;
        }
    }

    wlr_log_init(enable_debug ? WLR_DEBUG : WLR_INFO, NULL);
}

int main(int argc, char* argv[]){
    
    // 处理启动参数, 目前可用:
    /*
        --debug, -d: 启用调试输出。
    */
    setup_params(argc, argv);

    wlr_log(WLR_DEBUG, "这是一条来自wlroots的log打印, 如果你看到了这条打印信息, 说明wlroots依赖引用成功!");

    // 创建服务器对象
    // 该对象使用unique_ptr, 无需手动管理内存
    tiley::TileyServer& server = tiley::TileyServer::getInstance();

    // 创建显示客户端
    server.wl_display_ = wl_display_create();

    // 创建后端, 该后端是智能的, 会自动根据X11/Wayland的窗口类型调整打开的类型
    server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display_), NULL);

    if(server.backend == NULL){
        wlr_log(WLR_ERROR, "无法创建后端。请检查是否有显示服务器。");
        return 1;
    }

    server.renderer = wlr_renderer_autocreate(server.backend);
    if (server.renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return 1;
	}

    wlr_renderer_init_wl_display(server.renderer, server.wl_display_);

    // 创建渲染buffer管理器
    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);

    if(server.allocator == NULL){
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        return 1;
    }

    wlr_compositor_create(server.wl_display_, 5, server.renderer);
    wlr_subcompositor_create(server.wl_display_);
    wlr_data_device_manager_create(server.wl_display_);


    /***********事件注册开始************/
    // 一共11个主要事件, 具体查看events.h中的说明

    server.output_layout = wlr_output_layout_create(server.wl_display_);

    wl_list_init(&server.outputs);

    server.new_output.notify = server_new_output;

    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    server.scene = wlr_scene_create_();

    server.scene_layout = wlr_scene_attach_output_layout_(server.scene, server.output_layout);

    wl_list_init(&server.toplevels);

    server.xdg_shell = wlr_xdg_shell_create(server.wl_display_, 3);

    server.new_xdg_toplevel.notify = server_new_xdg_toplevel;

    wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);

    server.new_xdg_popup.notify = server_new_xdg_popup;

    wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

    server.cursor = wlr_cursor_create();

    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    server.cursor_mode = tiley::TILEY_CURSOR_PASSTHROUGH;

    server.cursor_motion.notify = server_cursor_motion;

    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);

    server.cursor_motion_absolute.notify = server_cursor_motion_absolute;

    wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);

    server.cursor_button.notify = server_cursor_button;

    wl_signal_add(&server.cursor->events.button, &server.cursor_button);

    server.cursor_axis.notify = server_cursor_axis;

    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);

    server.cursor_frame.notify = server_cursor_frame;

    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

    wl_list_init(&server.keyboards);

    server.new_input.notify = server_new_input;

    wl_signal_add(&server.backend->events.new_input, &server.new_input);

    server.seat = wlr_seat_create(server.wl_display_, "seat0");

    server.request_cursor.notify = seat_request_cursor;

    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);

    server.request_set_selection.notify = seat_request_set_selection;

    wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

    /***********事件注册结束************/
    // 首次加载
load_hotkey_config("./hotkey.json");
// 后台监视文件变化，自动重载
std::thread([]{
    watch_hotkey_file("./hotkey.json");
}).detach();

    /***********启动准备开始************/

    // 连接到Wayland服务器
    const char* socket = wl_display_add_socket_auto(server.wl_display_);

    if(!socket){
        wlr_backend_destroy(server.backend);
        return 1;
    }

    // 启动后端。具体逻辑可以查看start内编写的内容
    // 这里的逻辑是:
    // 1. 尝试启动后端, 并接受true/false返回值表示是否启动成功
    // 2. 如果不成功, 则进行一些清理程序, 再直接停止整个程序
    // TODO: 逻辑优化: 当启动不成功时, 添加报错信息。
    if(!wlr_backend_start(server.backend)){
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display_);
        return 1;
    }

    // 设置"WAYLAND_DISPLAY"环境变量, 使其指向tiley.
    // 该环境变量的目的是告诉系统: 我想注册一个新的管理器, id是socket。
    // 参考: https://wayland-book.com/wayland-display/creation.html
    // https://wayland.arktoria.org/4-wayland-display/creation.html
    setenv("WAYLAND_DISPLAY", socket, true);

    
    /***********启动准备结束*************/

    /**************启!动!***************/

    // 启动事件循环, while(true)这个万物之源:)
    // 该函数即为整个应用的主函数, 除非用户要求退出/出现错误, 否则不会退出。
    // 主要完成的工作即是我们在上面编写的各种输入处理事件循环, 不停地发出frame事件等等。
    wlr_log(WLR_INFO, "正在 WAYLAND_DISPLAY=%s 上运行",
			socket);
    wlr_log(WLR_INFO, "若要在Tiley中打开程序, 请在另一个控制台中输入:");
    wlr_log(WLR_INFO, "\nexport WAYLAND_DISPLAY=%s\n<程序名>", socket);
    
    wl_display_run(server.wl_display_);
    
    // 当用户退出程序...

    /**********退出清理工作开始**********/

    // 关闭所有客户端, 最后关闭服务器
    wl_display_destroy_clients(server.wl_display_);

    // 关闭创建新窗口的功能
    wl_list_remove(&server.new_xdg_toplevel.link);
    wl_list_remove(&server.new_xdg_popup.link);

    // 关闭鼠标输入
    wl_list_remove(&server.cursor_motion.link);
    wl_list_remove(&server.cursor_motion_absolute.link);
    wl_list_remove(&server.cursor_button.link);     
    wl_list_remove(&server.cursor_axis.link);
	wl_list_remove(&server.cursor_frame.link);
    
    // 关闭新的输入设备接入监听
    wl_list_remove(&server.new_input.link);

    // 关闭交互功能
    wl_list_remove(&server.request_cursor.link);
    wl_list_remove(&server.request_set_selection.link);

    // 关闭新的显示屏接入监听
    wl_list_remove(&server.new_output.link);

    // 销毁场景对象树
    wlr_scene_node_destroy_(get_wlr_scene_root_node(server.scene));

    // 销毁光标尺寸管理器
    wlr_xcursor_manager_destroy(server.cursor_mgr);

    // 销毁光标
    wlr_cursor_destroy(server.cursor);

    // 销毁渲染buffer管理器
    wlr_allocator_destroy(server.allocator);

    // 销毁渲染器
    wlr_renderer_destroy(server.renderer);

    // 关闭后端服务器
    wlr_backend_destroy(server.backend);

    // 退出tiley
    wl_display_destroy(server.wl_display_);

    return 0;

}