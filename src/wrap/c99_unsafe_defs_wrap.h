#ifndef __C99_UNSAFE_DEFS_WRAP__
#define __C99_UNSAFE_DEFS_WRAP__

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

struct wlr_scene;
struct wlr_scene_output_layout;
struct wlr_scene_rect;
struct surface_toplevel;

struct wlr_scene_rect* wlr_scene_rect_create_(struct wlr_scene_tree *parent,
    int width, int height, const float* color);

struct wlr_scene_output_layout* wlr_scene_attach_output_layout_(struct wlr_scene *scene,
	struct wlr_output_layout *output_layout);

struct wlr_scene_output *wlr_scene_output_create_(struct wlr_scene *scene,
	struct wlr_output *output);

void wlr_scene_output_layout_add_output_(struct wlr_scene_output_layout *sol,
	struct wlr_output_layout_output *lo, struct wlr_scene_output *so);

struct wlr_scene* wlr_scene_create_(void);

void wlr_scene_node_destroy_(struct wlr_scene_node*);

struct wlr_scene_tree *wlr_scene_xdg_surface_create_(
	struct wlr_scene_tree *parent, struct wlr_xdg_surface *xdg_surface);

void wlr_scene_node_raise_to_top_(struct wlr_scene_node* node);

struct wlr_scene_output* wlr_scene_get_scene_output_(struct wlr_scene* scene, struct wlr_output* output);

void wlr_scene_output_commit_(struct wlr_scene_output* scene_output, const struct wlr_scene_output_state_options *options);

void wlr_scene_output_send_frame_done(struct wlr_scene_output *scene_output,
		struct timespec *now);

struct wlr_scene_node* get_wlr_scene_root_node(struct wlr_scene*);

struct wlr_scene_tree* get_wlr_scene_tree(struct wlr_scene* scene);

struct wlr_scene_node* get_toplevel_node(struct surface_toplevel* toplevel);



void set_tree_node_data(struct surface_toplevel* toplevel);

void set_scene_tree_node_data(struct surface_toplevel* toplevel);


#ifdef __cplusplus
}
#endif

#endif
