#pragma once

#include <LNamespaces.h>
#include <LToplevelRole.h>
#include <LLayerView.h>
#include <memory>

#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/types.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/client/ToplevelRole.hpp"

using namespace Louvre;

namespace tiley{
    class Surface;
    class TileyWindowStateManager;
    class ToplevelRole;
}

namespace tiley{
    
    // A container represents a parent for splitting or a window.
    // Container tree is consisted of container and mainly used for dynamic tiling algorithms
    class Container{
        public:
            inline const LRect& getGeometry(){return geometry;}
            inline LLayerView* getContainerView(){ return containerView.get(); }
            inline LToplevelRole* getWindow(){return window;}
            void printContainerWindowInfo();
            // call constructor with a null window is no-op
            Container(ToplevelRole* window);
            Container();
            ~Container();
        private:
            // Dynamic tiling: split Type
            SPLIT_TYPE splitType;
            // Dynamic tiling: why a window is temporarily floating
            FLOATING_REASON floating_reason = NONE;
            
            // TODO: LWeak or unique_ptr
            LToplevelRole* window = nullptr; // nonnull when for a window

            Container* parent = nullptr;
            Container* child1;
            Container* child2;
            
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