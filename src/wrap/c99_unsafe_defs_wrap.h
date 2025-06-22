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

struct wlr_scene_node *wlr_scene_node_at_(struct wlr_scene_node *node,
		double lx, double ly, double *nx, double *ny);

struct wlr_scene_buffer *wlr_scene_buffer_from_node_(
		struct wlr_scene_node *node);

struct wlr_scene_surface *wlr_scene_surface_try_from_buffer_(
		struct wlr_scene_buffer *scene_buffer);

struct wlr_scene_tree *get_parent(struct wlr_scene_node *node);

struct wlr_surface* get_surface_from_scene_surface(struct wlr_scene_surface* scene_surface);

struct wlr_scene_node* get_wlr_scene_root_node(struct wlr_scene*);

struct wlr_scene_tree* get_wlr_scene_tree(struct wlr_scene* scene);

struct wlr_scene_node* get_wlr_scene_tree_node(struct wlr_scene_tree* tree);

struct wlr_scene_node* get_wlr_scene_rect_node(struct wlr_scene_rect* rect);

void wlr_scene_rect_set_size_(struct wlr_scene_rect *rect, int width, int height);

struct wlr_scene_node* get_toplevel_node(struct surface_toplevel* toplevel);

void set_tree_node_data(struct surface_toplevel* toplevel);

void set_scene_tree_node_data(struct surface_toplevel* toplevel);

void* get_tree_node_data(struct wlr_scene_node* node);

void set_tree(struct wlr_scene_tree* *tree, struct wlr_scene_tree* target);

void wlr_scene_node_reparent_(struct wlr_scene_node *node, struct wlr_scene_tree *new_parent);

struct wlr_scene_tree* wlr_scene_tree_create_(struct wlr_scene_tree *parent);

// 获得根节点的x坐标
int get_scene_tree_node_x(struct surface_toplevel* toplevel);

// 获得根节点的y坐标
int get_scene_tree_node_y(struct surface_toplevel* toplevel);

// 移动一个窗口(节点)
void wlr_scene_node_set_position_(struct wlr_scene_node* node, int x, int y);

void wlr_scene_node_set_enabled_(struct wlr_scene_node *node, bool enabled);

//trick: 绕开C++中的enum限制
enum wlr_scene_node_type_ get_toplevel_node_type_(struct wlr_scene_node* node);

#ifdef __cplusplus
}
#endif

#endif
