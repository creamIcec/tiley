#pragma once

#include <LNamespaces.h>
#include <vector>
#include "src/lib/types.hpp"

namespace tiley{

    using namespace Louvre;

    template <typename T_container>
    class BaseLayout{
        public:
            static const int WORKSPACES = 10;
            UInt32 CURRENT_WORKSPACE = 0;

            virtual bool insert(UInt32 workspace, T_container* newWindowContainer);
            virtual T_container* remove(LToplevelRole* window);
            virtual T_container* detach(LToplevelRole* window, FLOATING_REASON reason = MOVING);
            virtual bool attach(LToplevelRole* window);
            virtual bool resize(LPointF cursorPos);
            virtual bool recalculate();

            BaseLayout(int workspaceCount);
            ~BaseLayout();
            BaseLayout(BaseLayout const &other);

        protected:
            virtual void reflow(UInt32 workspace, const LRect& region, bool& success);
            T_container* activeContainer;
            std::vector<T_container*> workspaceActiveContainers;
            UInt32 containerCount = 0;
    };
}