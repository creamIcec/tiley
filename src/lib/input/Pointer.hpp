#pragma once

#include "LPointerButtonEvent.h"
#include "LPointerMoveEvent.h"
#include <LPointer.h>

#include <functional>

using namespace Louvre;

namespace tiley{
    class Pointer final : public Louvre::LPointer{
        public:
            using LPointer::LPointer;
            void pointerButtonEvent(const LPointerButtonEvent& event) override;
            void pointerMoveEvent(const LPointerMoveEvent& event) override;
            void focusChanged() override;
            void printPointerPressedSurfaceDebugInfo();
            bool processCompositorKeybind(const LPointerButtonEvent& event);
            void processPointerButtonEvent(const LPointerButtonEvent& event);

            LSurface* surfaceAtWithFilter(const LPoint& point, const std::function<bool (LSurface*)> &filter);
    };
}