#ifndef __SERVER_H__
#define __SERVER_H__

#include "src/wrap/c99_unsafe_defs_wrap.h"

#include <GLES2/gl2.h>
#include <memory>
#include <mutex>
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
    #include <wlr/types/wlr_output.h>
    #include <wlr/types/wlr_xdg_shell.h>
    #include <wlr/types/wlr_cursor.h>
    #include <wlr/types/wlr_xcursor_manager.h>
    #include <wlr/render/drm_format_set.h>
    #include <wlr/render/gles2.h>
    #include <wlr/types/wlr_buffer.h>
    #include <render/allocator/shm.h>
}

namespace tiley{

    enum tiley_cursor_mode {
        TILEY_CURSOR_PASSTHROUGH,
        TILEY_CURSOR_MOVE,
        TILEY_CURSOR_RESIZE,
    };

    struct TileyView;

    // 用于管理原生 GLES2 着色器程序
    struct TileyShader {
        GLuint program = 0;
        GLint proj = -1;
        GLint tex = -1;
        GLint alpha = -1;
        // 为将来的圆角做准备
        GLint size = -1;
        GLint radius = -1;
    };

    class TileyServer{
        public:
            wl_display *wl_display_;
            wlr_backend *backend;
            wlr_renderer *renderer;
            wlr_allocator *allocator;
            wlr_scene *scene;

            // 场景树图层顺序:  floating_layer > tiled_layer > background_layer
            wlr_scene_tree *floating_layer;     //浮动场景树. 当窗口浮动时, 移动到该场景树反之移回scene的场景树;
            wlr_scene_tree *tiled_layer;        //正常情况下的平铺场景树;
            wlr_scene_tree *background_layer;   //背景场景树, 为加入壁纸显示做准备

            wlr_scene_output_layout *scene_layout;

            wlr_xdg_shell *xdg_shell;
            wl_listener new_xdg_toplevel;
            wl_listener new_xdg_popup;
            wl_list toplevels;

            wlr_cursor *cursor;
            wlr_xcursor_manager *cursor_mgr;
            wl_listener cursor_motion;
            wl_listener cursor_motion_absolute;
            wl_listener cursor_button;
            wl_listener cursor_axis;
            wl_listener cursor_frame;

            wlr_seat *seat;
            wl_listener new_input;
            wl_listener request_cursor;
            wl_listener request_set_selection;
            wl_list keyboards;
            tiley_cursor_mode cursor_mode;
            struct surface_toplevel *grabbed_toplevel;
            double grab_x, grab_y;
            wlr_box grab_geobox;
            uint32_t resize_edges;

            wlr_output_layout *output_layout;
            wl_list outputs;
            wl_listener new_output;

            struct wlr_scene_buffer *wallpaper_node;
            TileyShader tint_shader;
            
            static TileyServer& getInstance();

        private:

            struct ServerDeleter {
                void operator()(TileyServer* p) const {
                    delete p;
                }
            };

            friend struct ServerDeleter;

            TileyServer();
            ~TileyServer();
            static std::unique_ptr<TileyServer, ServerDeleter> INSTANCE;
            static std::once_flag onceFlag;
            TileyServer(const TileyServer&) = delete;
            TileyServer& operator=(const TileyServer&) = delete;
    };

    // 我们的自定义场景节点，用于渲染窗口
    struct TileyViewNode {
        struct wlr_scene_node* node; // 必须是第一个成员
        TileyServer& server;
        struct TileyView* view; // 指向所属的 view
    };

    // 声明着色器初始化函数
    bool init_shader(TileyShader& shader, const std::string& vert_path, const std::string& frag_path);
    
}

#endif