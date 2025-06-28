#ifndef __TYPES_H__
#define __TYPES_H__

#include "wlr/util/box.h"
#include <wayland-server-core.h>
#include <wayland-util.h>

// 定义窗口装饰(目前用于浮动窗口的标记)
struct toplevel_decoration {
    struct wlr_scene_rect* top;
    struct wlr_scene_rect* bottom;
    struct wlr_scene_rect* left;
    struct wlr_scene_rect* right;
};

// 定义一个显示屏管理对象

struct output_display {
    struct wl_list link;
    struct wlr_output *wlr_output;
    // 当有新的帧要渲染时触发
    struct wl_listener frame;
    // 当显示屏状态改变时触发(例如旋转方向, 显示区域等发生了变化)  
    struct wl_listener request_state;
    // 当显示屏被拔出(不再可用)时触发
    struct wl_listener destroy;

    struct wlr_scene_output* scene_output; // 保存与场景的连接
    struct wlr_scene_buffer* wallpaper_node; // 此 output 专属的壁纸节点
};


// 定义一个键盘管理对象

struct input_keyboard {
    struct wl_list link;
    struct wlr_keyboard* wlr_keyboard;
    // 当键盘的修饰键按下时触发(Alt, ctrl, shift等)
    struct wl_listener modifiers;
    // 当键盘的任意按键按下时触发
    struct wl_listener key;
    // 当键盘被拔出(不再可用)时触发
    struct wl_listener destroy;
};

// 某个container的分割信息
enum split_info{
    SPLIT_NONE,  // 窗口, 叶子节点, 不进行分割
    SPLIT_H,     // 该容器水平分割(左右)
    SPLIT_V      // 该容器垂直分割(上下)
};

// 某个container的浮动信息, 表示因何种原因而浮动
enum floating_reason{
    NONE,       // 不浮动
    MOVING,     // 因为用户正在移动窗口
    STACKING,   // 因为用户请求将这个窗口变成堆叠显示, 可以堆叠显示在其他窗口上方
};

// 一个container可以代表一个叶子节点(真正的窗口)
// 也可以代表一个容器(用于分割的)
struct area_container{
    enum split_info split;
    enum floating_reason floating;
    
    struct area_container* parent;
    struct area_container* child1;  //这里我们不以上下左右命名, 只用编号, 避免混淆
    struct area_container* child2;

    struct surface_toplevel* toplevel;   //当是叶子节点时, 指向一个真正的窗口
    struct wlr_box geometry;
    float split_ratio;    //容器分割比例。仅在toplevel!=None(是容器)时有效
};

// 定义一个窗口管理对象

struct surface_toplevel{
    struct area_container* container;  //建立一个双向连接, 方便关闭等操作查找
    struct wl_list link;
    struct wlr_xdg_toplevel* xdg_toplevel;
    struct wlr_scene_tree* scene_tree;

    bool pending_configure;   //心跳等待, 当准备配置一个窗口时会先设置为true
    uint32_t last_configure_signal;   //最后一个心跳序列号

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    struct wl_listener ack_configure;  // 收到心跳回复事件

    struct toplevel_decoration decoration;
};

// 定义一个弹出窗口管理对象
struct surface_popup{
    struct wlr_xdg_popup* xdg_popup;
    struct wl_listener commit;
    struct wl_listener destroy;
};

// 覆盖scene_node_type的定义enum, 在C++中使用
enum wlr_scene_node_type_ {
	WLR_SCENE_NODE_TREE_,
	WLR_SCENE_NODE_RECT_,
	WLR_SCENE_NODE_BUFFER_,
};


// 启动参数结构体
struct launch_args{
    bool enable_debug;
    char* startup_cmd;
};


#endif
