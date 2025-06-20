#include "server.hpp"
#include "types.h"
#include "wlr/util/box.h"
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <random>
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
            area_container* desktop_container_at(int lx, int ly, int workspace);  //以坐标获取容器
            struct output_display* get_display(int workspace);  //根据workspace编号获得对应的屏幕
            inline struct area_container* get_workspace_root(int workspace){
                return this->workspace_roots[workspace];
            }  // 调试: 查看一个工作区的根节点
            inline void insert_display(output_display* new_display){ 
                 //用于在有新的显示屏插入时添加到管理
                std::lock_guard<std::mutex>lock(insert_mutex);
                int workspace = display_to_workspace_map.size();
                const std::string name = new_display->wlr_output->name;
                //获取显示器的名字
                if(display_to_workspace_map.find(name)!=display_to_workspace_map.end()){
                 std::cout<<"请勿重复添加"<<name<<"显示器"<<std::endl;
                 return;
                //size = 下标 + 1 ，检查是否重复添加该显示器，这样起码在增加显示器时不会出现一个工作区多个显示器的情况
                }
                if (workspace >= WORKSPACES) {
                std::cout << "工作区已满，无法添加更多显示器。" << std::endl;
                return;
               }//目前工作区上限写死。

            display_to_workspace_map[name] = workspace;
                std::cout << "插入显示屏" << std::endl;
                std::cout << "display: " << name << " workspace: " << workspace  << std::endl;

            }
                
                /*
                else{    
                 display_to_workspace_map[name] = workspace; 
                }
*/

            
            inline bool remove_display(output_display* removed_display){   //用于在显示屏不再可用时从管理中移除
               std::lock_guard<std::mutex>lock(remove_mutex);
                const std::string name = removed_display->wlr_output->name;
                std::map<std::string, int>::iterator it = display_to_workspace_map.find(name);
                if(it == display_to_workspace_map.end()){
                    return false;  //移除失败, 不存在这个显示屏
                }
                display_to_workspace_map.erase(name);
                //int workspace = it->second;
                //std::pair<std::string, int> workspace_key(name, workspace);
               // workspace_to_display_map.erase(workspace_key);
                //TODO: 检查内存, 极小的可能性一个display对应到了多个工作区, 但是也要避免
                return true;
            }
      inline bool move_display_to_workspace(output_display* display, int target_workspace) {
            const std::string name = display->wlr_output->name;
            std::map<std::string, int>::iterator it = display_to_workspace_map.find(name);
            if (it == display_to_workspace_map.end()) {
                return false;  // 移动失败, 显示器不存在
            }

            // 确保目标工作区没有其他显示器
           // std::pair<std::string, int> workspace_key(name, target_workspace);
            if (std::find_if(display_to_workspace_map.begin(), display_to_workspace_map.end(), 
                             [target_workspace](const std::pair<std::string, int>& item) { return item.second == target_workspace; }) != display_to_workspace_map.end()) {
                std::cout << "目标工作区已被映射到显示器，无法移动显示器。" << std::endl;
                return false;
            }

            int current_workspace = it->second;
            display_to_workspace_map[name] = target_workspace;
            //workspace_to_display_map[workspace_key] = name;  // 更新工作区到显示器的映射

            // 从当前工作区移除显示器
            //std::pair<std::string, int> current_workspace_key(name, current_workspace);
           // workspace_to_display_map.erase(current_workspace_key);
            std::cout << "移动显示器至工作区" << target_workspace << std::endl;
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
            void print_container_tree(int workspace);  //打印容器树, 用于调试。
        private:
            // 当前工作区(目前只使用一个)
            int current_workspace = 0;
            //std::map<std::pair<std::string, int>, std::string> workspace_to_display_map;  // 工作区到显示器的映射,TODO，这里我感觉我好像为了盘醋，包了饺子
            //增加显示器移除显示器的锁；
            std::mutex insert_mutex;
            std::mutex remove_mutex;
            // 工作区
            std::vector<area_container*> workspace_roots{WORKSPACES};

            // 显示屏到工作区的对应关系, 可以移动
            std::map<std::string, int> display_to_workspace_map;

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