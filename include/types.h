#ifndef __TYPES_H__
#define __TYPES_H__

#include <wayland-server-core.h>
#include <wayland-util.h>

// 定义一个显示屏管理对象

struct display_output {
    struct wl_list link;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;
};

#endif