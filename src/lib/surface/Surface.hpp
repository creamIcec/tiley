#ifndef __SURFACE_H__
#define __SURFACE_H__

#include <LSurface.h>
#include <LBaseSurfaceRole.h>
#include "SurfaceView.hpp"
#include "src/lib/client/ToplevelRole.hpp"

using namespace Louvre;

class Surface final : public LSurface{
    public:
        using LSurface::LSurface;
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
};

#endif  // __SURFACE_H__