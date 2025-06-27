#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "include/server.hpp"

void render_toplevel_with_tint(
    struct surface_toplevel* toplevel,
    double render_x, double render_y,
    struct wlr_render_pass *render_pass 
);

#endif