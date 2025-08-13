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
            // 打开窗口的动画
            LAnimation fadeInAnimation;
            // 关闭窗口的动画
            void startUnmappedAnimation() noexcept;
            // 标记防止动画重复执行
            bool isClosing = false;
            // 快照用: 窗口在被快照时的矩形区域
            LRect minimizeStartRect;
    };
}