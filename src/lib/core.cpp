#include "include/core.hpp"
#include "include/server.hpp"

#include "src/wrap/c99_unsafe_defs_wrap.h"
#include "wlr/util/box.h"
#include "wlr/util/log.h"
#include <memory>
#include <mutex>
#include <iostream>

using namespace tiley;

std::unique_ptr<WindowStateManager, WindowStateManager::WindowStateManagerDeleter> WindowStateManager::INSTANCE = nullptr;
std::once_flag WindowStateManager::onceFlag;

WindowStateManager& WindowStateManager::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new WindowStateManager());
    });

    return *INSTANCE;
}

// 在构造函数中创建每个工作区的根节点: 桌面
// 只有桌面:
// root: parent = nullptr, child1 = nullptr, child2 = nullptr;
// 桌面 + 1个窗口:
// root: parent = nullptr, child1 = <窗口>, child2 = nullptr;
// 桌面 + >= 2个窗口:
// root: parent = nullptr, child1 = <窗口>, child2 = <窗口>
WindowStateManager::WindowStateManager(){
    for (int i = 0; i < tiley::WORKSPACES; i++) {
        this->workspace_roots[i] = new area_container();
        this->workspace_roots[i]->child1 = nullptr;
        this->workspace_roots[i]->child2 = nullptr;
        this->workspace_roots[i]->parent = nullptr;
    }
}

WindowStateManager::~WindowStateManager(){}

// 重新计算窗口布局
void WindowStateManager::reflow(int workspace, wlr_box display_geometry){
    area_container* root = this->workspace_roots[workspace];
    wlr_log(WLR_DEBUG, "获取工作区");
    // 先处理桌面的情况
    // 只有桌面
    if(root->child1 == nullptr && root->child1 == nullptr){
        return;  // 直接停止
    }
    // 桌面 + 1个窗口
    if(root->child1->split == SPLIT_NONE && root->child2 == nullptr){
        wlr_xdg_toplevel_set_size(root->child1->toplevel->xdg_toplevel,display_geometry.width, display_geometry.height);
        wlr_scene_node_set_position_(get_wlr_scene_tree_node(root->child1->toplevel->scene_tree), display_geometry.x, display_geometry.y);
        return;
    }
    // 桌面 + >= 2个窗口
    this->_reflow(root->child1, display_geometry);
}

// 重新计算窗口布局内部函数(递归)
void WindowStateManager::_reflow(area_container* container, wlr_box remaining_area){

    // 1. 判断我是叶子还是容器
    if(container->toplevel != nullptr){
        wlr_log(WLR_DEBUG, "是叶子");
        // 2. 如果我是叶子, 说明我是窗口, 设置大小和位置
        wlr_xdg_toplevel_set_size(container->toplevel->xdg_toplevel, remaining_area.width, remaining_area.height);
        wlr_scene_node_set_position_(get_wlr_scene_tree_node(container->toplevel->scene_tree), remaining_area.x, remaining_area.y);
        wlr_log(WLR_DEBUG, "设置完成");
        return;
    }
    // 3. 如果我是容器, 计算两个子节点的区域
    struct wlr_box area1, area2;   //分别给child1, child2
    if(container->split == SPLIT_H){   // 如果是横向分割
        wlr_log(WLR_DEBUG, "是容器, 横向分割");
        int split_point = remaining_area.x + (remaining_area.width * 0.5);  //按照hyprland等的惯例, 对半分
        // 给child1, 左边
        area1 = {remaining_area.x, remaining_area.y, split_point - remaining_area.x, remaining_area.height};
        // 给child2, 右边
        area2 = {split_point, remaining_area.y, remaining_area.width - (split_point - remaining_area.x), remaining_area.height};
    }else if(container->split == SPLIT_V){  // 纵向分割
        wlr_log(WLR_DEBUG, "是容器, 纵向分割");
        int split_point = remaining_area.y + (remaining_area.height * 0.5);
        // 给child1, 上边
        area1 = {remaining_area.x, remaining_area.y, remaining_area.width, split_point - remaining_area.y};
        // 给child2, 下边
        area2 = {remaining_area.x, split_point, remaining_area.width, remaining_area.height - (split_point - remaining_area.y)};
    }

    // 递归
    _reflow(container->child1, area1);
    _reflow(container->child2, area2);
}

// 为一个toplevel创建新的container(下称win2), 类型为叶子节点
// 1. 创建一个新的container
// 2. 将创建的位置原本的窗口节点替换成容器节点split, 并将原来的叶子节点win1移动到容器的child1子节点
// 3. 将该节点win2插入到容器节点split的child2子节点
area_container* WindowStateManager::create_toplevel_container(surface_toplevel* toplevel){
    area_container* container = new area_container(); //谨防内存泄漏: 在WindowStateManager中删除
    container->toplevel = toplevel;  //是一个窗口
    container->split = SPLIT_NONE;  //窗口没有分割信息
    return container;
}

// 将一个container插入container树中
// container: 要插入的节点, oldleaf: 被分割的老窗口, 在该函数中换成容器, 并将自身插入到容器下方
// 1. 判断原先是不是桌面的几种情况, 是则进入3, 否则进入2;
// 2. 判断old_leaf是不是容器, 是则插入失败, 否则进入4;
// 3. 创建新的容器作为根节点, 并将container作为child1插入, 设置desktop_root为水平分割, 插入成功;
// 4. 继续create_toplevel_container的第2步。根据传入的分割方式设置分割方式(外部根据鼠标位置计算), 插入成功。
bool WindowStateManager::insert(area_container* container, area_container* target_leaf, enum split_info split){


    // FIXME: 暂时用0号显示器
    int workspace = 0;
    
    // 1
    if(target_leaf->parent != nullptr){  // 不是桌面
        // 2
        if(target_leaf->split != SPLIT_NONE){  // 并且是容器
            return false; //插入失败
        }
    }

    // 4
    // 遍历整棵树, 防止重复插入
    if(this->find(this->workspace_roots[workspace], container)){
        return false;
    }

    area_container* new_leaf = nullptr;
    
    if(target_leaf->parent != nullptr){  //上一个窗口不是桌面
        // 将以前的叶子窗口变成新的节点
        new_leaf = this->create_toplevel_container(target_leaf->toplevel);
        // 改变以前的类型
        target_leaf->toplevel = nullptr;
        target_leaf->split = split;

        // 插入到新的容器的子节点中
        target_leaf->child1 = new_leaf;
        target_leaf->child2 = container;

        new_leaf->parent = target_leaf;
    } else {   //上一个窗口是桌面
        target_leaf->child1 = container;
    }
    
    container->parent = target_leaf;

    return true;
}

static bool pos_in_range(int lx, int ly, int sx, int sy, int width, int height){
    return lx >= sx && lx <= sx + width && ly >= sy && ly <= sy + height;
}

//以坐标获取容器
area_container* WindowStateManager::desktop_container_at(int lx, int ly, int workspace){
    // 获取工作区root
    area_container* root = this->workspace_roots[workspace];
    return this->_desktop_container_at(lx,ly,root); 
}

area_container* WindowStateManager::_desktop_container_at(int lx, int ly, area_container* container){

    std::cout << "桌面对象地址(内部):" << container << std::endl;
    
    if(container == nullptr){
        return nullptr;
    }

    // 1. 先序遍历, 找出所有不是容器的窗口
    if(container->toplevel != nullptr){
        wlr_box bb = container->toplevel->xdg_toplevel->base->geometry;
        if(pos_in_range(lx, ly, bb.x, bb.y,bb.width, bb.height)){
            return container;
        }
    }

    area_container* found = _desktop_container_at(lx, ly, container->child1);
    if(found){
        return found;
    }
    found = _desktop_container_at(lx, ly, container->child2);
    if(found){
        return found;
    }

    // 这种情况不太可能发生(平铺式总是会占满空间), 安全起见, 我们还是返回原来的容器。
    return container;
}

bool WindowStateManager::find(area_container* as_root, area_container* target){   //以传入的节点作为根节点先序遍历整棵树
    if(as_root == nullptr){
        return false;  // 为空, 找不到
    }

    if(as_root == target){
        return true;
    }

    if(as_root->child1 != nullptr){
        this->find(as_root->child1, target);
    }

    if(as_root->child2 != nullptr){
        this->find(as_root->child2, target);
    }
    
    return false;  //最后仍然没找到

}