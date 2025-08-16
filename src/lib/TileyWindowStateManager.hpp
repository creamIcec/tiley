#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "LBitset.h"
#include "LEdge.h"

#include <LNamespaces.h>
#include "LToplevelRole.h"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/Container.hpp"
#include "types.hpp"
#include <LAnimation.h>

using namespace Louvre;

namespace tiley{
    class Container;
    class ToplevelRole;
}

namespace tiley{
    class TileyWindowStateManager{
        public:

            // 默认工作区初始数量
            static const int WORKSPACES = 10;

            static TileyWindowStateManager& getInstance();
            // insertTile: 只传入待插入的容器的版本, 是一般情况: 在鼠标位置插入。
            bool insertTile(UInt32 workspace, Container* newWindowContainer, Float32 splitRatio);
            // insertTile: 另外传入目标容器的版本, 是特殊情况: 在非鼠标位置插入(例如在其他工作区打开窗口, 批量打开窗口)。
            bool insertTile(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE split, Float32 splitRatio);
            // remove: 移除容器, 用于关闭平铺的窗口。
            Container* removeTile(LToplevelRole* window);
            // detach: 将一个容器从容器树中分离, 用于浮动窗口/移动窗口等操作。
            Container* detachTile(LToplevelRole* window, FLOATING_REASON reason = MOVING);
            //切换到指定工作区(0-10)
            bool switchWorkspace(UInt32 target);
            // 获取当前工作区
            UInt32 currentWorkspace() const { return CURRENT_WORKSPACE; }
            // attach: 将一个被分离的容器重新插入容器树中, 例如将浮动的窗口合并回平铺层, 或者停止移动窗口等。
            bool attachTile(LToplevelRole* window);
            // resizeTile: 调整当前活动的(由setupResizeSession设置)平铺容器的平铺比例。
            bool resizeTile(LPointF cursorPos);
            // setupResizeSession: 在开始调整窗口大小时调用, 用于建立调整的上下文。包括被调整的窗口
            void setupResizeSession(LToplevelRole* window, LBitset<LEdge> edges, const LPointF& cursorPos);
            // recalculate: 重新布局。
            bool recalculate();
            // addWindow: 添加窗口。如果添加成功, 并且添加到了平铺层, 会用container带回。
            bool addWindow(ToplevelRole* window, Container*& container);
            // removeWindow: 移除窗口。如果移除成功, 并且移除的是平铺层的窗口, 会用container带回。
            bool removeWindow(ToplevelRole* window, Container*& container);
            // toggleFloatWindow: 切换窗口平铺/浮动状态
            bool toggleStackWindow(ToplevelRole* window);
            // 检查一个窗口是否是平铺层窗口(不包括正在移动的/用户浮动的)
            bool isTiledWindow(ToplevelRole* window);
            // 检查一个窗口是否被堆叠
            bool isStackedWindow(ToplevelRole* window);
            // 设置上一个活动容器。调用时机: 新窗口显示(设置成新窗口)/鼠标或键盘聚焦变化(设置成聚焦窗口)/没有任何聚焦(设置成nullptr)
            // 注意: 该函数和鼠标/键盘不一定同步(比如可能在另一个工作区)。不要依赖该函数的container反向获得的window来当作聚焦窗口。仅供插入机制使用。
            void setActiveContainer(Container* container);
            // 获取上一个活动容器。如果是nullptr, 表示没有满足活动条件的容器。
            inline Container* activateContainer(){ return activeContainer; }
            // 获取一个工作区的第一个窗口对应的容器
            Container* getFirstWindowContainer(UInt32 workspace);
            // 传入容器返回工作区根节点
            UInt32 getWorkspace(Container* container) const;
            // 获取一个工作区中, 当前鼠标所在位置的平铺容器, 这个函数是为了方便插入使用: 当工作区为空时, 直接返回工作区根节点, 否则返回所在位置的窗口的容器(而不是分割容器)
            // TODO: 找到一个更好的方法仅在平铺区的窗口中查找目标窗口
            Container* getInsertTargetTiledContainer(UInt32 workspace);
            // 重新分配窗口层级。将根据每个窗口(toplevel)的类型(type)刷新显示层级
            bool reapplyWindowState(ToplevelRole* window);
            // 调试: 打印某个工作区当前容器树层次信息
            void printContainerHierachy(UInt32 workspace);
            // printContainerHierachy的递归函数
            void _printContainerHierachy(Container* current);
            // 全局记录工作区, 再也不用分散在各处了
            // TODO: 我们需要这个成员始终反映用户意图。也就是说, 无论在哪儿调用, 当前工作区始终是那个函数想要的。怎么做?
           // UInt32 CURRENT_WORKSPACE = DEFAULT_WORKSPACE;

        private:
            // reflow: 在给定的region作为最大显示区域的情况下进行重新布局
            void reflow(UInt32 workspace, const LRect& region, bool& success);
            // reflow的递归函数
            void _reflow(Container* container, const LRect& areaRemain, UInt32& accumulateCount);
            // 获取第一个窗口的递归函数。
            Container* _getFirstWindowContainer(Container* container);
            //当前工作区的索引
            UInt32 CURRENT_WORKSPACE=0;

            // 单一来源: workspaceActiveContainers. 更新顺序: workspaceActiveContainers -> activeContainer;
            // 访问当前工作区的仍然是activeContainer
            // TOOD: 仅使用workspaceActiveContainers[index], 删除activeContainer
            // 注意: 需要保持和workspaceActiveContainers的同步
            Container* activeContainer;
            // 各个工作区最后活动的Container, 作为平铺算法在指定工作区下的操作依据
            std::vector<Container*> workspaceActiveContainers{WORKSPACES};
            // 各个工作区的平铺根节点Container, 作为平铺算法仅仅需要获取根节点时的操作依据
            std::vector<Container*> workspaceRoots{WORKSPACES};

            // 递归去把一个窗口及其子窗口切换可见性（切换工作区）
            void setWindowVisible(ToplevelRole* window, bool visible);
            static UInt32 countContainersOfWorkspace(const Container* root);
            // 目前一共的container数量, 作为校验使用, 可以检测出平铺过程中出现意外导致container数量不一致。TODO: 自动重新计算数量
            UInt32 containerCount = 0;
            // 窗口缓存区。包含所有窗口, 不只是平铺的
            std::vector<ToplevelRole*> windows = {};

            // 拖拽改变大小时使用
            LPointF initialCursorPos;            // 开始拖拽时鼠标的初始位置
            double initialHorizontalRatio;       // 水平目标的初始分割比例
            double initialVerticalRatio;         // 垂直目标的初始分割比例
            Container* resizingHorizontalTarget; // 正在调整的水平目标
            Container* resizingVerticalTarget;   // 正在调整的垂直目标
            // 切换工作区动画
            LAnimation m_workspaceSwitchAnimation;
            std::list<ToplevelRole*> m_slidingOutWindows;
            std::list<ToplevelRole*> m_slidingInWindows;
            bool m_isSwitchingWorkspace = false;
            int m_switchDirection = 0; // -1 表示向左滑, 1 表示向右滑
            UInt32 m_targetWorkspace = 0;
            struct WindowStateManagerDeleter {
                void operator()(TileyWindowStateManager* p) const {
                    delete p;
                }
            };

            friend struct WindowStateManagerDeleter;

            TileyWindowStateManager();
            ~TileyWindowStateManager();
            static std::once_flag onceFlag;
            static std::unique_ptr<TileyWindowStateManager, WindowStateManagerDeleter> INSTANCE;
            TileyWindowStateManager(const TileyWindowStateManager&) = delete;
            TileyWindowStateManager& operator=(const TileyWindowStateManager&) = delete;
    };
}