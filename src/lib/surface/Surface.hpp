#pragma once

#include <memory>

#include <LSurface.h>
#include <LBaseSurfaceRole.h>
#include <LAnimation.h>

#include "LRegion.h"
#include "LTexture.h"
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

            friend Container;

            ToplevelRole *tl() noexcept{
                return (ToplevelRole*)toplevel();
            }

            LView *getView() noexcept;
            LSurfaceView* getSurfaceView() noexcept;
            bool hasMappedChildSurface() const noexcept;

            void roleChanged(LBaseSurfaceRole *prevRole) override;
            void layerChanged() override;
            void orderChanged() override;
            void mappingChanged() override;
            void minimizedChanged() override;
            LTexture* renderThumbnail(LRegion* transRegion = nullptr);

            void printWindowGeometryDebugInfo(LOutput* activeOutput, const LRect& outputAvailable) noexcept;

        private:
            std::unique_ptr<SurfaceView> view = std::make_unique<SurfaceView>(this);
            bool isFirstMap = false;
            // mapped animation
            LAnimation fadeInAnimation;
            // unmapped animation
            void startUnmappedAnimation() noexcept;
            // avoid duplicated execution of animations
            bool isClosing = false;
            // for window snapshot(e.g. before unmapping a window)
            LRect minimizeStartRect;
    };
}