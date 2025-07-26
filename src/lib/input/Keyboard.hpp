#pragma once

#include <LKeyboard.h>

using namespace Louvre;

namespace tiley{
    class Keyboard final : public LKeyboard{
        public:
            using LKeyboard::LKeyboard;
            void keyEvent(const LKeyboardKeyEvent& event) override;
    };
}