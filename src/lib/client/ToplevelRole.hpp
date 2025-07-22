#pragma once

#include <LToplevelRole.h>
#include <memory>

#include "src/lib/client/render/SSD.hpp"
#include "src/lib/core/Container.hpp"

using namespace Louvre;

namespace tiley{
    class Container;
}

namespace tiley{
    class ToplevelRole final : public LToplevelRole{
        private:
            std::unique_ptr<SSD> ssd;
        public:
            using LToplevelRole::LToplevelRole;

            // 双向指针其一: Surface(Role=Toplevel) -> NodeContainer
            Container* container;
            void atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev) override;
            void configureRequest() override;
    };
}