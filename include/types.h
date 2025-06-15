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

// 定义一个窗口管理对象

struct surface_toplevel{
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

#endif