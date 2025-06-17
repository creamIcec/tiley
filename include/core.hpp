#include "types.h"
#include "wlr/util/box.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

namespace tiley{

    // 暂时写死的工作区数量
    static const int WORKSPACES = 10;

    // 某个container的分割信息
    enum split_info{
        SPLIT_NONE,  // 窗口, 叶子节点, 不进行分割
        SPLIT_H,     // 该容器水平分割(左右)
        SPLIT_V      // 该容器垂直分割(上下)
    };

    // 一个屏幕对应一个workspace
    struct display_workspace_pair{
        struct output_display* display;
        int workspace;
    };

    // 一个container可以代表一个叶子节点(真正的窗口)
    // 也可以代表一个容器(用于分割的)
    struct area_container{
        enum split_info split;
        
        struct area_container* parent;
        struct area_container* child1;  //这里我们不以上下左右命名, 只用编号, 避免混淆
        struct area_container* child2;
    
        struct surface_toplevel* toplevel;   //当是叶子节点时, 指向一个真正的窗口
    };

    class WindowStateManager{
        public:
            static WindowStateManager& getInstance();
            void reflow(int workspace, wlr_box display_geometry); // 刷新布局(重新流动~)
            area_container* create_toplevel_container(surface_toplevel* toplevel);   //创建一个新的container用来装toplevel
            bool insert(area_container* container, area_container* old_leaf, enum split_info split);  // 内部方法: 将一个container插入到container树中, 如果container已经存在, 则不能插入, 返回false; 如果不存在, 则插入, 返回true
            bool find(area_container* as_root, area_container* target);   //以传入的节点作为根节点遍历整棵树, 查找目标
            area_container* desktop_container_at(int lx, int ly, int workspace);  //以坐标获取容器
            struct output_display* get_display(int workspace);  //根据workspace编号获得对应的屏幕(暂时保留)
            struct area_container* get_workspace_root(int workspace){
                return this->workspace_roots[workspace];
            }  // 调试: 查看一个工作区的根节点
        private:
            // 当前工作区(目前只使用一个)
            int current_workspace = 0;

            std::vector<area_container*> workspace_roots{WORKSPACES};
            
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
    };

}