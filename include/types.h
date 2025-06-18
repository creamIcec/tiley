#ifndef __TYPES_H__
#define __TYPES_H__


#include <wayland-server-core.h>
#include <wayland-util.h>

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

// 一个container可以代表一个叶子节点(真正的窗口)
// 也可以代表一个容器(用于分割的)
struct area_container{
    enum split_info split;
    
    struct area_container* parent;
    struct area_container* child1;  //这里我们不以上下左右命名, 只用编号, 避免混淆
    struct area_container* child2;

    struct surface_toplevel* toplevel;   //当是叶子节点时, 指向一个真正的窗口
};

// 定义一个窗口管理对象

struct surface_toplevel{
    struct area_container* container;  //建立一个双向连接, 方便关闭等操作查找
    struct wl_list link;
    struct wlr_xdg_toplevel* xdg_toplevel;
    struct wlr_scene_tree* scene_tree;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
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

#endif