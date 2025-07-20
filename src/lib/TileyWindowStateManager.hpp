#pragma once

#include <memory>
#include <mutex>

#include "LNamespaces.h"
#include "types.hpp"

using namespace Louvre;

namespace tiley{
    class TileyWindowStateManager{
        public:
            static TileyWindowStateManager& getInstance();
            // 在给定的region作为最大显示区域的情况下进行重新布局
            void reflow(UInt32 workspace, LSize& region);
            inline void setFocusedContainer(NodeContainer* container){ this->focusedContainer = container; };
        private:
            NodeContainer* focusedContainer;

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