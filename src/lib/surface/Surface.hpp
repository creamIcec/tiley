#pragma once

#include <LSurface.h>
#include <LBaseSurfaceRole.h>
#include <memory>

#include "src/lib/client/views/SurfaceView.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/Container.hpp"

using namespace Louvre;

namespace tiley{
    class SurfaceView;
    class Container;
    class ToplevelRole;
}

namespace tiley{
    class Surface final : public LSurface{
        public:
            using LSurface::LSurface;

            friend Container;

            ToplevelRole *tl() noexcept{
                return (ToplevelRole*)toplevel();
            }

            LView *getView() noexcept;
            bool hasMappedChildSurface() const noexcept;

            void roleChanged(LBaseSurfaceRole *prevRole) override;
            void layerChanged() override;
            void orderChanged() override;
            
            // 当surface尺寸发生变化时, 如果是窗口, 我们先保证
            
            void mappingChanged() override;
            void minimizedChanged() override;
            void printWindowGeometryDebugInfo(LOutput* activeOutput, const LRect& outputAvailable) noexcept;
        private:
            std::unique_ptr<SurfaceView> view = std::make_unique<SurfaceView>(this);
    };
}