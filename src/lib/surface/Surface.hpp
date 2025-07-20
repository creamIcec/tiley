#pragma once

#include <LSurface.h>
#include <LBaseSurfaceRole.h>

#include "SurfaceView.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/types.hpp"

using namespace Louvre;
namespace tiley{
    class Surface final : public LSurface{
        public:
            using LSurface::LSurface;

            Surface(const void* params) noexcept;

            SurfaceView view {this};
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

            // 双向指针其一: Surface(Role=Toplevel) -> NodeContainer
            struct tiley::NodeContainer* container;
    };
}