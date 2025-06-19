#include <cstdlib>
#include <ctime>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "include/events.h"
#include "include/types.h"
#include "include/server.hpp"
#include "include/core.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "wlr/util/log.h"

using namespace tiley;

static void output_frame(struct wl_listener* listener, void* data){
    //TODO: 当有新的帧要输出时触发的函数

    // 客户端函数

    // 干下面的事:
    // 1. 获取到场景输出(画布)
    // 2. 渲染并提交帧
    // 3. 为了同步, 更新时钟(指示此时渲染一帧成功完成)

    struct output_display* output = wl_container_of(listener, output, frame);
    
    // 1
    struct wlr_scene* scene = TileyServer::getInstance().scene;
    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output_(scene, output->wlr_output);

    // 2
    wlr_scene_output_commit_(scene_output, NULL);

    // 3
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done_(scene_output, &now);

}

static void output_request_state(struct wl_listener *listener, void *data) {
	// TODO: 当屏幕状态改变时触发的函数
    // 第一次, 初始化屏幕时也会触发
    wlr_log(WLR_DEBUG, "屏幕状态发生改变");

    struct output_display* output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state* event = 
        static_cast<wlr_output_event_request_state*>(data);
    
    // 使用屏幕状态提交新的状态
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
    //TODO: 当屏幕被拔出时(变得不可用时)触发的函数
    struct output_display* output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);

    free(output);
}

void server_new_output(struct wl_listener* _, void* data){
    //TODO: 这里写当新的显示屏接入时的处理逻辑

    //客户端函数

    //大致来讲干下面的事情:
    // 1. 指定新的显示屏使用我们的buffer allocator和renderer
    // 2. 打开新的显示屏的输出状态
    // 3. 指定显示参数: (长度, 宽度, 刷新率)
    // 4. 提交渲染更新状态(虽然这里并不渲染任何东西, 但是要让服务端知道多了一个显示屏)
    // 5. 为新的显示屏注册客户端事件处理器: 刷新屏幕事件, 刷新屏幕状态事件和屏幕拔出事件
    // 6. 添加屏幕到输出布局中, 同时也告知客户端可以查询关于该屏幕的信息: 分辨率, 缩放比例, 名称, 制造商等等
    // 7. 添加屏幕到窗口管理中, 便于后期查找

    tiley::TileyServer& server = tiley::TileyServer::getInstance();
    tiley::WindowStateManager& manager = tiley::WindowStateManager::getInstance();

    wlr_output* wlr_output = static_cast<struct wlr_output*>(data);

    //1
    wlr_output_init_render(wlr_output, server.allocator, server.renderer);

    //2
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    //3
    struct wlr_output_mode* mode = wlr_output_preferred_mode(wlr_output);
    if(mode != NULL){
        wlr_output_state_set_mode(&state, mode);
    }

    //4
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    //拿到显示屏对象
    struct output_display* output = static_cast<output_display*>(calloc(1, sizeof(*output)));
    output->wlr_output = wlr_output;

    //5
    //刷新屏幕事件
    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    //刷新屏幕状态事件
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    //屏幕拔出事件
    output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    
    wl_list_insert(&server.outputs, &output->link);

    //6
    struct wlr_output_layout_output* l_output = wlr_output_layout_add_auto(server.output_layout, wlr_output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create_(server.scene, wlr_output);
    wlr_scene_output_layout_add_output_(server.scene_layout, l_output, scene_output);

    //7
    manager.insert_display(output);

}