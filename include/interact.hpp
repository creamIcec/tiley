#ifndef __INTERACT_H__
#define __INTERACT_H__

#include "include/server.hpp"
#include "include/core.hpp"

bool focus_toplevel(struct surface_toplevel* toplevel);
void toggle_overhang_toplevel(struct area_container *container, tiley::WindowStateManager& manager, tiley::TileyServer& server, int workspace);
struct surface_toplevel* desktop_toplevel_at(tiley::TileyServer& server, double lx, double ly, struct wlr_surface* *surface, double* sx, double* sy);
void process_cursor_resize_floating(tiley::TileyServer& server);
#endif