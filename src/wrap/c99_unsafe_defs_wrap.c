#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include "c99_unsafe_defs_wrap.h"
#include "types.h"

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

struct wlr_scene_tree *wlr_scene_xdg_surface_create_(
	struct wlr_scene_tree *parent, struct wlr_xdg_surface *xdg_surface){
        return wlr_scene_xdg_surface_create(parent, xdg_surface);
}

struct wlr_scene_output* wlr_scene_get_scene_output_(struct wlr_scene* scene, struct wlr_output* output){
        return wlr_scene_get_scene_output(scene, output);
}

struct wlr_scene_node *wlr_scene_node_at_(struct wlr_scene_node *node,
		double lx, double ly, double *nx, double *ny){
        return wlr_scene_node_at(node, lx, ly, nx, ny);
}

struct wlr_scene_buffer *wlr_scene_buffer_from_node_(
		struct wlr_scene_node *node){
        return wlr_scene_buffer_from_node(node);
}

struct wlr_scene_surface *wlr_scene_surface_try_from_buffer_(
		struct wlr_scene_buffer *scene_buffer){
        return wlr_scene_surface_try_from_buffer(scene_buffer);
}

struct wlr_surface* get_surface_from_scene_surface(struct wlr_scene_surface* scene_surface){
        return scene_surface->surface;
}

struct wlr_scene_tree *get_parent(struct wlr_scene_node *node){
        return node->parent;
}

void wlr_scene_output_send_frame_done_(struct wlr_scene_output *scene_output,
		struct timespec *now){
        wlr_scene_output_send_frame_done(scene_output, now);
}

void wlr_scene_output_commit_(struct wlr_scene_output* scene_output, const struct wlr_scene_output_state_options *options){
        wlr_scene_output_commit(scene_output, options);
}

void wlr_scene_node_raise_to_top_(struct wlr_scene_node* node){
        wlr_scene_node_raise_to_top(node);
}

// 包装函数: 由于c/cpp隔离, 我们无法从cpp中直接获取到结构体中的内容, 需要一个辅助函数

// 获取到场景根节点的node
struct wlr_scene_node* get_wlr_scene_root_node(struct wlr_scene* scene){
        return &scene->tree.node;
}

struct wlr_scene_tree* get_wlr_scene_tree(struct wlr_scene* scene){
        return &scene->tree;
}

struct wlr_scene_node* get_wlr_scene_tree_node(struct wlr_scene_tree* tree){
        return &tree->node;
}

struct wlr_scene_node* get_wlr_scene_rect_node(struct wlr_scene_rect* rect){
        return &rect->node;
}

// 获得根节点的x坐标
int get_scene_tree_node_x(struct surface_toplevel* toplevel){
        return toplevel->scene_tree->node.x;
}

// 获得根节点的y坐标
int get_scene_tree_node_y(struct surface_toplevel* toplevel){
        return toplevel->scene_tree->node.y;
}

void wlr_scene_node_set_position_(struct wlr_scene_node* node, int x, int y){
        wlr_scene_node_set_position(node, x, y);
}

// 移动到一个场景树
void wlr_scene_node_reparent_(struct wlr_scene_node *node, struct wlr_scene_tree *new_parent){
        wlr_scene_node_reparent(node, new_parent);
}

// 创建场景树(可以理解成Photoshop中的图层)
struct wlr_scene_tree* wlr_scene_tree_create_(struct wlr_scene_tree *parent){
        return wlr_scene_tree_create(parent);
}

// 获取一个窗口在场景中的节点
struct wlr_scene_node* get_toplevel_node(struct surface_toplevel* toplevel){
        return &toplevel->scene_tree->node;
}

// 获取节点类型信息
enum wlr_scene_node_type_ get_toplevel_node_type_(struct wlr_scene_node* node){
        return (enum wlr_scene_node_type_)(node->type);
}

// 设置节点启用状态
void wlr_scene_node_set_enabled_(struct wlr_scene_node *node, bool enabled){
        wlr_scene_node_set_enabled(node, enabled);
}

// 设置矩形大小
void wlr_scene_rect_set_size_(struct wlr_scene_rect *rect, int width, int height){
        wlr_scene_rect_set_size(rect, width, height);
}

struct wlr_scene_node* get_scene_buffer_node(struct wlr_scene_buffer* buffer){
        return &buffer->node;
}

struct wlr_scene_buffer* wlr_scene_buffer_create_(struct wlr_scene_tree *parent, struct wlr_buffer *buffer){
        return wlr_scene_buffer_create(parent, buffer);
}

// 设置窗口对象的逻辑数据
// 这个数据非常关键, 是我们保存平铺式管理的特殊数据的地方.
// 例如, 我们使用KD树进行窗口的坐标分配和插入, 那么元素的KD树相关信息就应该
// 保存在data中.
void set_tree_node_data(struct surface_toplevel* toplevel){
        toplevel->scene_tree->node.data = toplevel;
}

void* get_tree_node_data(struct wlr_scene_node* node){
        return node->data;
}

void set_tree(struct wlr_scene_tree* *tree, struct wlr_scene_tree* target){
        *tree = target;
}
