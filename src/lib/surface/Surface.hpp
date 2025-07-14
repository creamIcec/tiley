#ifndef __SURFACE_H__
#define __SURFACE_H__

#include <LSurface.h>
#include "LBaseSurfaceRole.h"
#include "SurfaceView.hpp"
#include "src/lib/toplevel/ToplevelRole.hpp"

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
        void mappingChanged() override;
        void minimizedChanged() override;
};

#endif  // __SURFACE_H__