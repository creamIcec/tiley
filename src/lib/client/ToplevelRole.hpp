#pragma once

#include <LToplevelRole.h>
#include <memory>

#include "src/lib/client/render/SSD.hpp"

using namespace Louvre;

namespace tiley{
    class ToplevelRole final : public LToplevelRole{
        private:
            std::unique_ptr<SSD> ssd;
        public:
            using LToplevelRole::LToplevelRole;
            void atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev) override;
            void configureRequest() override;
    };
}