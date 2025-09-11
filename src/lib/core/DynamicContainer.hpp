#pragma once

#include "BaseContainer.hpp"

#include "src/lib/types.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/layout/DynamicLayout.hpp"

namespace tiley{
    class DynamicLayout;
}

namespace tiley{

    // Container tree is consisted of container and mainly used for dynamic tiling algorithms
    class DynamicContainer final : public BaseContainer{
        public:

            DynamicContainer();
            // call constructor with a null window is no-op
            DynamicContainer(ToplevelRole* window);
            ~DynamicContainer();

            inline const LRect& getGeometry(){return geometry;}
            inline LLayerView* getContainerView(){ return containerView.get(); }
            inline LToplevelRole* getWindow(){return window;}
            void printContainerWindowInfo();

        private:

            friend DynamicLayout;
            
            // Dynamic tiling: split Type
            SPLIT_TYPE splitType;
            // Dynamic tiling: why a window is temporarily floating
            FLOATING_REASON floating_reason = NONE;
            
            // TODO: LWeak or unique_ptr
            LToplevelRole* window = nullptr; // nonnull when for a window

            DynamicContainer* parent = nullptr;
            DynamicContainer* child1;
            DynamicContainer* child2;
            
            // Dynamic tiling: used by splitting containers, ignored by window containers
            Float32 splitRatio = 0.5;

            // Isolated geometry data for caching realtime window geometry to avoid chaotic updates
            LRect geometry;
            
            // An invisible wrapper view for window typed containers, convenient for decoration or clipping
            std::unique_ptr<LLayerView> containerView = nullptr;

            void enableContainerView(bool enable);

            void enableContainerFeatures();
            void disableContainerFeatures();

            // Allow manager to directly access container properties
            friend TileyWindowStateManager;
            
            enum class InsertHalf{
                Undefined,
                First,
                Second
            };
            
            InsertHalf insertHalf = InsertHalf::Undefined;
    };
}