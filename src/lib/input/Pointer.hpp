#pragma once

#include "LPointerButtonEvent.h"
#include <LPointer.h>

using namespace Louvre;

namespace tiley{
    class Pointer final : public Louvre::LPointer{
        public:
            using LPointer::LPointer;
            void pointerButtonEvent(const LPointerButtonEvent &event) override;   
    };
}