#pragma once

#include "LNamespaces.h"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/types.hpp"
#include "src/test/PerfmonRegistry.hpp"
#include <LOutput.h>
#include <LSolidColorView.h>
#include <LTextureView.h>
#include "src/test/PerformanceMonitor.hpp" 

using namespace Louvre;
using namespace tiley;

namespace tiley{
    class Surface;
}

namespace tiley{
    class Output final : public LOutput{
        public:

            using LOutput::LOutput;

            Output(const void* params) noexcept;

            // a new monitor inserted
            void initializeGL() override;

            // main drawing function, called every frame
            void paintGL() override;

            // monitor is logically relocated to another position
            void moveGL() override;

            // compositor considers that the monitor physical size is changed
            void resizeGL() override;

            // a monitor is no longer available
            void uninitializeGL() override;

            // use table values to represent the gamma curve a client requested 
            void setGammaRequest(LClient* client, const LGammaTable* gamma) override;

            void availableGeometryChanged() override;

            // from Louvre-views: output fade-in animation when plugged in
            LSolidColorView fadeInView{{1.f, 1.f, 1.f}};

            Surface *searchFullscreenSurface() const noexcept;
            bool tryDirectScanout(Surface *surface) noexcept;

            // wallpaper
            void updateWallpaper();
            Louvre::LTextureView& wallpaperView() { return m_wallpaperView; }
            // print wallpaper information
            void printWallpaperInfo();
      
            // testing instrument
            std::string perfTag_;
            PerformanceMonitor* perfMon_ = nullptr; 

        private:
            LTextureView m_wallpaperView{nullptr, &TileyServer::getInstance().layers()[BACKGROUND_LAYER]};
    };
}