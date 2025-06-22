#ifndef __DECORATION_H__
#define __DECORATION_H__

#include "include/types.h"

void set_toplevel_decoration_enabled(surface_toplevel* toplevel, bool enabled);

void create_toplevel_decoration(surface_toplevel* toplevel);

void update_toplevel_decoration(surface_toplevel* toplevel);

void destroy_toplevel_decoration(surface_toplevel* toplevel);

#endif