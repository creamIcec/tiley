#pragma once

#include "BaseContainer.hpp"

#include "LNamespaces.h"
#include "LPoint.h"
#include "src/lib/types.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/layout/GridLayout.hpp"

namespace tiley{
    class GridLayout;
}

namespace tiley{

    using namespace Louvre;

    enum FLOW_DIRECTION{
        HORIZONTAL,
        VERTICAL
    };

    // Container tree is consisted of container and mainly used for dynamic tiling algorithms
    class GridContainer final : public BaseContainer{
        public:

            GridContainer();
            // call constructor with a null window is no-op
            GridContainer(ToplevelRole* window);
            ~GridContainer();

            inline const LRect& getGeometry(){return geometry;}
            inline LLayerView* getContainerView(){ return containerView.get(); }
            inline LToplevelRole* getWindow(){return window;}
            void printContainerWindowInfo();

        private:

            using LPointU = LPointTemplate<UInt32>;
            using LSizeU = LPointU;

            friend GridLayout;

            // How next window locates: horizontal means the grid will be horizontally filled first(left-right then top-down),
            // whereas vertical means vertically filled first(top-down then left-right)
            FLOW_DIRECTION flow;
            
            // Dynamic tiling: why a window is temporarily floating
            FLOATING_REASON floating_reason = NONE;
            // TODO: LWeak or unique_ptr
            LToplevelRole* window = nullptr; // nonnull when for a window

            // Grid order of the container. e.g. (0,0) means the container is at top-left corner of available region,
            // (2,2) means the container is at right-down corner.
            LPointU anchor;

            // Size available for the container. Represented in grid size, e.g. (1,1) means 1 cell wide and 1 cell high,
            // (1,2) means 1 cell wide and 2 cells high
            LSizeU size;

            // Isolated geometry data for caching realtime window geometry to avoid massive chaotic updates
            LRect geometry;
            
            // An invisible wrapper view for window typed containers, convenient for decoration or clipping
            std::unique_ptr<LLayerView> containerView = nullptr;

            void enableContainerView(bool enable);

            void enableContainerFeatures();
            void disableContainerFeatures();

            // Allow manager to directly access container properties
            friend TileyWindowStateManager;
    };
}