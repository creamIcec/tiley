#include <wlr/types/wlr_scene.h>
#include "c99_unsafe_defs_wrap.h"

struct wlr_scene_rect* wlr_scene_rect_create_(struct wlr_scene_tree *parent,
    int width, int height, const float* color){
        return wlr_scene_rect_create(parent, width, height, color);
}

struct wlr_scene_output_layout* wlr_scene_attach_output_layout_(struct wlr_scene *scene,
		struct wlr_output_layout *output_layout){
        return wlr_scene_attach_output_layout(scene, output_layout);                   
}

struct wlr_scene* wlr_scene_create_(void){
        return wlr_scene_create();
}

void wlr_scene_node_destroy_(struct wlr_scene_node *node){
        wlr_scene_node_destroy(node);
}

struct wlr_scene_output *wlr_scene_output_create_(struct wlr_scene *scene,
	struct wlr_output *output){
        return wlr_scene_output_create(scene, output);
}

void wlr_scene_output_layout_add_output_(struct wlr_scene_output_layout *sol,
	struct wlr_output_layout_output *lo, struct wlr_scene_output *so){
        wlr_scene_output_layout_add_output(sol, lo, so);
}

// 包装函数: 由于c/cpp隔离, 我们无法从cpp中直接获取到结构体中的内容, 需要一个辅助函数
struct wlr_scene_node* get_wlr_scene_node(struct wlr_scene* scene){
        return &scene->tree.node;
}
