#pragma once

#include "LPointerButtonEvent.h"
#include "LPointerMoveEvent.h"
#include <LPointer.h>

using namespace Louvre;

namespace tiley{
    class Pointer final : public Louvre::LPointer{
        public:
            using LPointer::LPointer;
            void pointerButtonEvent(const LPointerButtonEvent &event) override;
            void pointerMoveEvent(const LPointerMoveEvent& event) override;
            void focusChanged() override;
            void printPointerPressedSurfaceDebugInfo();
    };
}