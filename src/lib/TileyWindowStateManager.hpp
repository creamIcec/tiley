#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "LNamespaces.h"
#include "LToplevelRole.h"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/Container.hpp"
#include "types.hpp"

using namespace Louvre;

namespace tiley{
    class Container;
    class ToplevelRole;
}

namespace tiley{
    class TileyWindowStateManager{
        public:
            static TileyWindowStateManager& getInstance();
            // insertTile: 只传入待插入的容器的版本, 是一般情况: 在鼠标位置插入。
            bool insertTile(UInt32 workspace, Container* newWindowContainer, Float32 splitRatio);
            // insertTile: 另外传入目标容器的版本, 是特殊情况: 在非鼠标位置插入(例如在其他工作区打开窗口, 批量打开窗口)。
            bool insertTile(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE split, Float32 splitRatio);
            // remove: 移除容器, 用于关闭平铺的窗口。
            Container* removeTile(LToplevelRole* window);
            // recalculate: 重新布局。
            bool recalculate();
            // addWindow: 添加窗口。如果添加成功, 并且添加到了平铺层, 会用container带回。
            bool addWindow(ToplevelRole* window, Container*& container);
            // removeWindow: 移除窗口。如果移除成功, 并且移除的是平铺层的窗口, 会用container带回。
            bool removeWindow(ToplevelRole* window, Container*& container);
            // 设置上一个插入时活动的窗口。每次插入窗口后应该调用。   
            inline void setActiveContainer(Container* container){ this->activeContainer = container; };
            // 获取上一个插入时活动的窗口。推荐在找不到应该插入哪个窗口位置时使用。
            inline Container* activateContainer(){return activeContainer;}
            // 获取一个工作区的第一个窗口对应的容器。
            Container* getFirstWindowContainer(UInt32 workspace);
            // 调试: 打印某个工作区当前容器树层次信息
            void printContainerHierachy(UInt32 workspace);
            // printContainerHierachy的递归函数
            void _printContainerHierachy(Container* current);
        private:
            // reflow: 在给定的region作为最大显示区域的情况下进行重新布局
            void reflow(UInt32 workspace, const LRect& region, bool& success);
            // reflow的递归函数
            void _reflow(Container* container, const LRect& areaRemain, UInt32& accumulateCount);
            // 获取第一个窗口的递归函数。
            Container* _getFirstWindowContainer(Container* container);
            // 重新组织窗口层级, 在insert时调用
            bool rearrangeWindowLayer(ToplevelRole* window);
            // 工作区最大数量
            static const int WORKSPACES = 10;
            // 每个工作区的平铺根节点
            std::vector<Container*> workspaceRoots{WORKSPACES};
            // 目前活动的Container, 用于当某次insert找不到应该放在哪儿时作为fallback使用
            Container* activeContainer;
            // 目前一共的container数量, 作为校验使用, 可以检测出平铺过程中出现意外导致container数量不一致。TODO: 自动重新计算数量
            UInt32 containerCount = 0;
            // 窗口缓存区。包含所有窗口, 不只是平铺的
            std::vector<ToplevelRole*> windows = {};
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