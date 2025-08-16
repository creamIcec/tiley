#pragma once

#include <LSurfaceView.h>
#include "src/test/PerfmonRegistry.hpp"
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
            ~SurfaceView() noexcept;

            void paintEvent(const PaintEventParams& params) noexcept override;
            const LRegion * translucentRegion() const noexcept override;
    };
   PerformanceMonitor& perfmon(); 

}