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
            Surface(const void *params);

            std::unique_ptr<SurfaceView> view;

            ToplevelRole *tl() noexcept{
                return (ToplevelRole*)toplevel();
            }

            LView *getView() noexcept;
            bool hasMappedChildSurface() const noexcept;

            void roleChanged(LBaseSurfaceRole *prevRole) override;
            void layerChanged() override;
            void orderChanged() override;
            // 对应迁移wlroots: xdg_toplevel_map(不止toplevel, 任何一个surface的map状态改变都会触发该函数)
            void mappingChanged() override;
            void minimizedChanged() override;
            void printWindowGeometryDebugInfo(LOutput* activeOutput, const LRect& outputAvailable) noexcept;
    };
}