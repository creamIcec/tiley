#include <cstdlib>
#include <cstring>
#include <ctime>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "include/events.h"
#include "include/types.h"
#include "include/server.hpp"
#include "include/core.hpp"
#include "include/image_util.hpp"
#include "include/renderer.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "wlr/util/log.h"


using namespace tiley;


static void output_frame(struct wl_listener* listener, void* data){

    //TODO: 当有新的帧要输出时触发的函数

    // 客户端函数

    // 干下面的事:
    // 1. 手动渲染(非常重要, 从普通wlroots使用到高级wlroots使用的一个重大转变!)
    // 2. 清屏, 绘制壁纸
    // 3. 为了同步, 更新时钟(指示此时渲染一帧成功完成)

    TileyServer& server = TileyServer::getInstance();

    // 1
    struct output_display* output = wl_container_of(listener, output, frame);

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    struct wlr_render_pass* pass = wlr_output_begin_render_pass(output->wlr_output, &state, nullptr);

    if(pass == nullptr){
        wlr_log(WLR_ERROR, "错误: 无法开始手动渲染, 停止渲染帧。程序现在可以直接退出, 因为不会有任何对象再被渲染了。注意: 这个错误只应该在调试时出现。如果运行时出现, 说明程序发生错误。");
        return;
    }

    // 2
    int output_width, output_height;
    wlr_output_effective_resolution(output->wlr_output, &output_width, &output_height);

    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output_(server.scene, output->wlr_output);
    if(!scene_output){
        return;
    }
    
    // 清屏
    struct wlr_render_rect_options clear_rect_options = {
        {
            0, 0,
            output_width,
            output_height,
        },
        { 0.1f, 0.1f, 0.1f, 1.0f },
        NULL,
        WLR_RENDER_BLEND_MODE_NONE, // 使用 NONE 确保它完全覆盖旧内容
    };
    wlr_render_pass_add_rect(pass, &clear_rect_options);
    

    if(server.background_layer && output->wallpaper_node){

        wlr_renderer* renderer = server.renderer;
        
        // 从场景节点获取公开的 wlr_buffer
        struct wlr_buffer *wallpaper_wlr_buffer = get_buffer(output->wallpaper_node);
        
        // 从 wlr_buffer 为本帧临时创建一个 wlr_texture
        struct wlr_texture *wallpaper_texture = wlr_texture_from_buffer(renderer, wallpaper_wlr_buffer);

        if(wallpaper_texture){
            struct wlr_render_texture_options wallpaper_options = {
                wallpaper_texture,
                {
                    static_cast<double>(get_scene_buffer_x(output->wallpaper_node)),
                    static_cast<double>(get_scene_buffer_y(output->wallpaper_node)),
                    static_cast<double>(wallpaper_texture->width),
                    static_cast<double>(wallpaper_texture->height)
                }
            };
            wlr_render_pass_add_texture(pass, &wallpaper_options);
            // 立即清理资源
            wlr_texture_destroy(wallpaper_texture);
        }
    }

    struct surface_toplevel* toplevel;
    wl_list_for_each_reverse(toplevel, &server.toplevels, link){
        if(get_scene_buffer_node_enabled(get_wlr_scene_tree_node(toplevel->scene_tree))){
            double scene_x = get_scene_tree_node_x(toplevel);
            double scene_y = get_scene_tree_node_y(toplevel);
            
            double render_x = scene_x - get_scene_output_x(scene_output);
            double render_y = scene_y - get_scene_output_y(scene_output);

            render_toplevel_with_tint(toplevel, render_x, render_y, pass);
        }
    }


    wlr_output_add_software_cursors_to_render_pass(output->wlr_output, pass, nullptr);

    if (!wlr_render_pass_submit(pass)) {
        wlr_log(WLR_ERROR, "Failed to submit render pass on output '%s'", output->wlr_output->name);
    }

    // 7. 发送 frame_done 事件
    // 注意: 在新的 API 中，这一步由 wlr_output_commit_state() 隐式处理
    // 我们需要提交一个空的 state 来触发 buffer交换 和 frame_done
    if (!wlr_output_commit_state(output->wlr_output, &state)) {
        wlr_log(WLR_ERROR, "Failed to commit output state on '%s'", output->wlr_output->name);
    }
    wlr_output_state_finish(&state);


    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);

}


static void output_request_state(struct wl_listener *listener, void *data) {
	// TODO: 当屏幕状态改变时触发的函数
    // 第一次, 初始化屏幕时也会触发
    wlr_log(WLR_DEBUG, "屏幕状态发生改变");

    struct output_display* output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state* event = 
        static_cast<wlr_output_event_request_state*>(data);

    // 如下几乎只会在嵌套模式下输出(除非外部屏幕也发生了改变)
    wlr_log(WLR_INFO, "接收到来自宿主对 %s 的状态变更请求", output->wlr_output->name);
    if (event->state->committed & WLR_OUTPUT_STATE_SCALE) {
        wlr_log(WLR_INFO, "接收到对 %s 的状态请求，包含新的缩放值: %f", 
                output->wlr_output->name, event->state->scale);
    }
    if (event->state->committed & WLR_OUTPUT_STATE_MODE) {
        wlr_log(WLR_INFO, "接收到对 %s 的状态请求，包含新的模式", output->wlr_output->name);
    }
    
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
    // 3. 判断是否是嵌套运行。如果是嵌套运行, 则设置自定义显示参数, 防止变糊
    // 4. 物理模式指定显示参数: (长度, 宽度, 刷新率)
    // 5. 提交渲染更新状态, 包括自定义的缩放
    // 6. 为新的显示屏注册客户端事件处理器: 刷新屏幕事件, 刷新屏幕状态事件和屏幕拔出事件
    // 7. 添加屏幕到输出布局中, 同时也告知客户端可以查询关于该屏幕的信息: 分辨率, 缩放比例, 名称, 制造商等等
    // 8. 添加屏幕到窗口管理中, 便于后期查找

    tiley::TileyServer& server = tiley::TileyServer::getInstance();
    tiley::WindowStateManager& manager = tiley::WindowStateManager::getInstance();

    wlr_output* wlr_output = static_cast<struct wlr_output*>(data);

    //1
    wlr_output_init_render(wlr_output, server.allocator, server.renderer);

    //2
    struct wlr_output_state state;
    wlr_output_state_init(&state);

    //3,4
    struct wlr_output_mode* mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_state_set_enabled(&state, true);

    //5
    if (!wlr_output_commit_state(wlr_output, &state)) {
        wlr_log(WLR_ERROR, "无法提交输出 %s 的初始状态", wlr_output->name);
        wlr_output_state_finish(&state);
        return;
    }

    wlr_output_state_finish(&state);

    struct wlr_output_state scale_state;
    wlr_output_state_init(&scale_state);
    float scale = 1.0f;
    wlr_log(WLR_INFO, "为输出 %s 主动设置内部缩放为 %f", wlr_output->name, scale);
    wlr_output_state_set_scale(&scale_state, scale);
    if (!wlr_output_commit_state(wlr_output, &scale_state)) {
        wlr_log(WLR_ERROR, "无法为输出 %s 提交缩放状态", wlr_output->name);
    }
    wlr_output_state_finish(&scale_state);

    //拿到显示屏对象
    struct output_display* output = static_cast<output_display*>(calloc(1, sizeof(*output)));
    output->wlr_output = wlr_output;

    //6
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

    //7
    struct wlr_output_layout_output* l_output = wlr_output_layout_add_auto(server.output_layout, wlr_output);

    //如切换到手动渲染: 删除
    output->scene_output = wlr_scene_output_create_(server.scene, wlr_output);
    wlr_scene_output_layout_add_output_(server.scene_layout, l_output, output->scene_output);

    const char* wallpaper_path = "/home/iriseplos/tiley_test_wallpaper/sunset-girl.png";  //现在先写死
    wlr_box output_box;
    wlr_output_layout_get_box(server.output_layout, output->wlr_output, &output_box);
    struct wlr_buffer* wall_buffer = create_wallpaper_buffer_scaled(wallpaper_path, output_box.width, output_box.height); 


    if (wall_buffer) {
        // 将壁纸节点创建在 background_layer 中，并保存在 output 里
        output->wallpaper_node = wlr_scene_buffer_create_(server.background_layer, wall_buffer);
        wlr_buffer_drop(wall_buffer); // 场景已接管，释放本地引用

        if (output->wallpaper_node) {
            // 壁纸的位置就是这个 output 在全局布局中的位置
            wlr_scene_node_set_position_(get_scene_buffer_node(output->wallpaper_node), l_output->x, l_output->y);
            
            // 可以修改 create_buffer_from_path_manual
            // 让它能把图片拉伸到 output 的大小 (wlr_output->width, wlr_output->height)
        }
    }
    
    //8
    manager.insert_display(output);

}