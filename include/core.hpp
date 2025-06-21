#ifndef __CORE_H__
#define __CORE_H__

#include "server.hpp"
#include "types.h"
#include "wlr/util/box.h"
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <iostream>

namespace tiley{

    // 暂时写死的工作区数量
    static const int WORKSPACES = 10;

    class WindowStateManager{
        public:
            static WindowStateManager& getInstance();
            void reflow(int workspace, wlr_box display_geometry); // 刷新布局(重新流动~)
            area_container* create_toplevel_container(surface_toplevel* toplevel);   //创建一个新的container用来装toplevel
            bool insert(area_container* container, area_container* old_leaf, enum split_info split);  // 内部方法: 将一个container插入到container树中, 如果container已经存在, 则不能插入, 返回false; 如果不存在, 则插入, 返回true
            bool find(area_container* as_root, area_container* target);   //以传入的节点作为根节点遍历整棵树, 查找目标
            bool remove(area_container* container);   //移除传入的窗口节点, 用于关闭窗口
            bool detach(area_container* container, floating_reason reason);   //将窗口暂时分离, 用于移动, 浮动等目的
            bool attach(area_container* container, area_container* target, enum split_info split);  // 合入detach暂时分离的窗口
            area_container* desktop_container_at(int lx, int ly, int workspace);  //以坐标获取容器
            struct output_display* get_display(int workspace);  //根据workspace编号获得对应的屏幕
            inline struct area_container* get_workspace_root(int workspace){
                return this->workspace_roots[workspace];
            }  // 调试: 查看一个工作区的根节点
            inline void insert_display(output_display* new_display){  //用于在有新的显示屏插入时添加到管理
                int workspace = display_to_workspace_map.size();
                const std::string name = new_display->wlr_output->name;
                display_to_workspace_map[name] = workspace;  //size = 下标 + 1 
                std::cout << "插入显示屏" << std::endl;
                std::cout << "display: " << name << " workspace: " << workspace  << std::endl;
            }
            inline bool remove_display(output_display* removed_display){   //用于在显示屏不再可用时从管理中移除
                const std::string name = removed_display->wlr_output->name;
                std::map<std::string, int>::iterator it = display_to_workspace_map.find(name);
                if(it == display_to_workspace_map.end()){
                    return false;  //移除失败, 不存在这个显示屏
                }
                display_to_workspace_map.erase(name);
                //TODO: 检查内存, 极小的可能性一个display对应到了多个工作区, 但是也要避免
                return true;
            }
            inline bool move_display_to_workspace(output_display* display, int target_workspace){   //用于将显示屏移动到某个工作区
                const std::string name = display->wlr_output->name;
                std::map<std::string, int>::iterator it = display_to_workspace_map.find(name);
                if(it == display_to_workspace_map.end()){
                    return false;  //移动失败, 不存在这个显示屏
                }
                display_to_workspace_map[name] = target_workspace;
                return true;
            }
            inline int get_workspace_by_output_display(output_display* display){
                const std::string name = display->wlr_output->name;
                std::cout << "target display: " << name << std::endl;
                std::map<std::string, int>::iterator it = display_to_workspace_map.find(name);
                if(it == display_to_workspace_map.end()){
                    std::cout << "display: " << it->first << " workspace: " << it->second << std::endl;
                    return -1;  //查找失败, 不存在这个显示屏
                }
                return it->second;  //查找成功
            }
            inline area_container* moving_container(){  // 用户正在移动窗口
                return this->moving_container_;
            }
            inline void stop_moving(){   //打断移动。目前用于在移动时切换到悬浮
                this->moving_container_->floating = NONE;
                this->moving_container_ = nullptr;
            }
            inline void set_decorating(bool decorating){
                this->is_decorating = decorating;
            }
            inline bool get_decorating(){
                return this->is_decorating;
            }
            inline area_container* get_focused_container(){
                return this->focused_container_;
            }
            inline void set_focused_container(area_container* container){
                this->focused_container_ = container;
            }
            void print_container_tree(int workspace);  //打印容器树, 用于调试。
            bool is_alt_down = false;  //暂时写死alt键作为modifier
        private:
            // 当前工作区(目前只使用一个)
            int current_workspace = 0;

            // 工作区
            std::vector<area_container*> workspace_roots{WORKSPACES};

            // 显示屏到工作区的对应关系, 可以移动
            std::map<std::string, int> display_to_workspace_map;
            
            // 正在移动的容器
            area_container* moving_container_;

            // 目前聚焦的容器
            area_container* focused_container_;

            // 是否协商边框处理
            bool is_decorating = false;

            struct WindowStateManagerDeleter{
                
                // 后序遍历递归删除树节点
                void delete_node_recursive(area_container* node) const{
                    if(node->child1 != nullptr){
                        delete_node_recursive(node->child1);
                    }
                    if(node->child2 != nullptr){
                        delete_node_recursive(node->child2);
                    }
                    delete node;  //谨防toplevel指针内存泄漏。我们在关闭窗口时会释放toplevel, 所以这里无需操作
                }
                
                void operator()(WindowStateManager* p) const {

                    std::for_each(p->workspace_roots.cbegin(), p->workspace_roots.cend(), [this](area_container* root){
                        delete_node_recursive(root);
                    });

                    delete p;
                }
            };

            friend struct WindowStateManagerDeleter;

            WindowStateManager();
            ~WindowStateManager();

            WindowStateManager(const WindowStateManager&) = delete;
            WindowStateManager& operator=(const WindowStateManager&) = delete;
            
            static std::unique_ptr<WindowStateManager,WindowStateManagerDeleter> INSTANCE;
            static std::once_flag onceFlag;

            area_container* _desktop_container_at(int sx, int sy, area_container* container);
            void _reflow(area_container* container, wlr_box remaining_area);
            void _print_container_tree(area_container* container);
    };

}

#endif