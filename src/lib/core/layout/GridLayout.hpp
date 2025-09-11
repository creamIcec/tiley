#pragma once

#include "LNamespaces.h"
#include "src/lib/core/layout/BaseLayout.hpp"
#include "src/lib/core/GridContainer.hpp"
#include <vector>

namespace tiley{
    class GridContainer;
}

namespace tiley{
    class GridLayout final : public BaseLayout<GridContainer>{
        public:
            bool insert(UInt32 workspace, GridContainer* newWindowContainer) override;
            GridContainer* remove(LToplevelRole* window) override;
            GridContainer* detach(LToplevelRole* window, FLOATING_REASON reason = MOVING) override;
            bool attach(LToplevelRole* window) override;
            bool resize(LPointF cursorPos) override;
            bool recalculate() override;

            GridLayout(int workspaceCount);
            ~GridLayout();
            GridLayout(GridLayout const& other);
        private:
            void increaseRowAndColumn();
            void reflow(UInt32 workspace, const LRect& region, bool& success) override;
            std::map<UInt32, std::vector<GridContainer*>> containers;
            UInt32 row_next, column_next;
            UInt32 row_max, column_max;
    };
}