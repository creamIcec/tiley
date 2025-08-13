#pragma once

#include <LSurfaceView.h>
#include "src/lib/test/PerfmonRegistry.hpp"
#include "src/lib/surface/Surface.hpp"
using namespace Louvre;

namespace tiley {
    class Surface;
}

namespace tiley{
    class SurfaceView final : public LSurfaceView{
        public:
            using LSurfaceView::LSurfaceView;

            SurfaceView(Surface* surface) noexcept;
            void pointerButtonEvent (const LPointerButtonEvent &event) override;
            void pointerEnterEvent 	(const LPointerEnterEvent &event) override;
            void paintEvent(const PaintEventParams& params) noexcept override;
            const LRegion * translucentRegion() const noexcept override;
    };
   PerformanceMonitor& perfmon(); 

}