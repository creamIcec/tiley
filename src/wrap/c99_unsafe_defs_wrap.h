#ifndef __C99_UNSAFE_DEFS_WRAP__
#define __C99_UNSAFE_DEFS_WRAP__


#ifdef __cplusplus
extern "C" {
#endif

struct wlr_scene;
struct wlr_scene_output_layout;
struct wlr_scene_rect;

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

struct wlr_scene_node* get_wlr_scene_node(struct wlr_scene*);

#ifdef __cplusplus
}
#endif

#endif
