#pragma once

#include "LNamespaces.h"
#include "src/lib/core/layout/BaseLayout.hpp"
#include "src/lib/core/DynamicContainer.hpp"
#include "src/lib/types.hpp"

namespace tiley{
    class DynamicContainer;
}

namespace tiley{
    
    class DynamicLayout : public BaseLayout<DynamicContainer>{
        public:
            bool insert(UInt32 workspace, DynamicContainer* newWindowContainer) override;
            DynamicContainer* remove(LToplevelRole* window) override;
            DynamicContainer* detach(LToplevelRole* window, FLOATING_REASON reason = MOVING) override;
            bool attach(LToplevelRole* window) override;
            bool resize(LPointF cursorPos) override;
            bool recalculate() override;

            DynamicLayout(int workspaceCount);
            ~DynamicLayout();
            DynamicLayout(DynamicLayout const& other);
        private:
            std::vector<DynamicContainer*> workspaceRoots{ WORKSPACES };
            DynamicContainer* getInsertTargetTiledContainer(UInt32 workspace);
            auto getLayoutData(Surface* surface);
            void reflow(UInt32 workspace, const LRect& region, bool& success) override;
            void _reflow(DynamicContainer* container, const LRect& areaRemain, UInt32& accumulateCount);
            bool insert(UInt32 workspace, DynamicContainer* newWindowContainer, DynamicContainer* targetContainer, SPLIT_TYPE type, Float32 splitRatio);
    };
}