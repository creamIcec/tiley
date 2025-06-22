#ifndef __WINDOW_UTIL_H__
#define __WINDOW_UTIL_H__

#include "server.hpp"
#include "wlr/util/box.h"

wlr_box get_target_geometry_box(area_container* target_container, uint32_t display_width, uint32_t display_height);

wlr_box get_display_geometry_box(tiley::TileyServer& server);

#endif