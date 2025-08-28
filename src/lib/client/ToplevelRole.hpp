#pragma once

#include <LToplevelRole.h>
#include <memory>

#include "LEvent.h"
#include "LNamespaces.h"
#include "src/lib/client/render/SSD.hpp"
#include "src/lib/core/Container.hpp"
#include "src/lib/output/Output.hpp"

#include <LToplevelResizeSession.h>

using namespace Louvre;

namespace tiley{
    class Container;
    class Output;
}

namespace tiley{

    enum TOPLEVEL_TYPE{
        NORMAL,  // normal window: can be tiled
        RESTRICTED_SIZE,    // size of window is restricted: cannot be tiled
        FLOATING   // normal window but is floating: user stacked the window or is moving
    };

    class ToplevelRole final : public LToplevelRole{
        private:
            std::unique_ptr<SSD> ssd;
        public:
            using LToplevelRole::LToplevelRole;

            ToplevelRole(const void *params) noexcept;

            Container* container = nullptr;
            TOPLEVEL_TYPE type = NORMAL;
            // monitor for displaying the window
            Output* output = nullptr;
            // id of workspace in which the window resides
            UInt32 workspaceId;

            void atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev) override;
            void configureRequest() override;

            void startMoveRequest(const LEvent& triggeringEvent) override;
            void startResizeRequest(const LEvent& triggeringEvent, LBitset<LEdge> edge) override;

            bool hasSizeRestrictions();
            
            void assignToplevelType();

            // debug: print window geometry info
            void printWindowGeometryInfo(LToplevelRole* toplevel);
    };
}