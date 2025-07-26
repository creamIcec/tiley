#pragma once

#include <LToplevelRole.h>
#include <memory>

#include "LEvent.h"
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
        NORMAL,  // 正常窗口
        RESTRICTED_SIZE,    // 窗口被客户端限了尺寸。这种窗口不应该参加平铺, 因为他们有自己的尺寸限制
        FLOATING   // 被用户指定应该"浮动"的窗口或本身是一个子窗口
    };

    class ToplevelRole final : public LToplevelRole{
        private:
            std::unique_ptr<SSD> ssd;
        public:
            using LToplevelRole::LToplevelRole;

            // 双向指针其一: Surface(Role=Toplevel) -> NodeContainer
            Container* container;
            // 一个窗口的自定义类型。默认是正常窗口。类型见TOPLEVEL_TYPE
            TOPLEVEL_TYPE type = NORMAL;
            // 属于的显示屏
            Output* output = nullptr;
            void atomsChanged(LBitset<AtomChanges> changes, const Atoms &prev) override;
            void configureRequest() override;

            void startMoveRequest(const LEvent& triggeringEvent) override;
            void startResizeRequest(const LEvent& triggeringEvent, LBitset<LEdge> edge) override;

            bool hasSizeRestrictions();
            
            void assignToplevelType();
    };
}