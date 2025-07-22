#pragma once

#include <memory>
#include <mutex>

#include "LNamespaces.h"
#include "LToplevelRole.h"
#include "src/lib/core/Container.hpp"
#include "types.hpp"

using namespace Louvre;

namespace tiley{
    class TileyWindowStateManager{
        public:
            static TileyWindowStateManager& getInstance();
            // 在给定的region作为最大显示区域的情况下进行重新布局
            void reflow(UInt32 workspace, LRect& region);
            bool insert(UInt32 workspace, LToplevelRole* window);
            bool remove(LToplevelRole* window);   //移除传入的窗口, 用于关闭窗口
            inline void setFocusedContainer(Container* container){ this->focusedContainer = container; };
            //inline void populateWorkspaceRoot(UInt32 workspace, LToplevelRole* window){ this->workspaceRoots[workspace] }
        private:
            static const int WORKSPACES = 10;
            std::vector<Container*> workspaceRoots{WORKSPACES};
            Container* focusedContainer;
            void _reflow(Container* container, LRect& areaRemain);
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