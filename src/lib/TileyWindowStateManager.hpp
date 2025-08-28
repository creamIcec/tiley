#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "LBitset.h"
#include "LEdge.h"

#include <LNamespaces.h>
#include "LToplevelRole.h"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/Container.hpp"
#include "types.hpp"
#include <LAnimation.h>

using namespace Louvre;

namespace tiley{
    class Container;
    class ToplevelRole;
}

namespace tiley{
    class TileyWindowStateManager{
        public:
            static const int WORKSPACES = 10;
            static TileyWindowStateManager& getInstance();
            // insertTile: version for inserting when cursor position is determined
            bool insertTile(UInt32 workspace, Container* newWindowContainer, Float32 splitRatio);
            // insertTile: version for inserting at place where cursor not presents
            bool insertTile(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE split, Float32 splitRatio);
            // remove: remove a container when a tiling window is closed
            Container* removeTile(LToplevelRole* window);
            // detach: detach a window for moving, stacking, etc.
            Container* detachTile(LToplevelRole* window, FLOATING_REASON reason = MOVING);
            // switchWorkspace
            bool switchWorkspace(UInt32 target);
            // currentWorkspace
            UInt32 currentWorkspace() const { return CURRENT_WORKSPACE; }
            // attach: Oppsite to what detachTile does
            bool attachTile(LToplevelRole* window);
            // resizeTile: resizing tiling windows affected by user actions
            bool resizeTile(LPointF cursorPos);
            // setupResizeSession: call this when user start to resize windows(including floating ones)
            void setupResizeSession(LToplevelRole* window, LBitset<LEdge> edges, const LPointF& cursorPos);
            // recalculate: re-layout.
            bool recalculate();
            // addWindow: add a window to management, `container` will be the added container if added to tiling layout. 
            bool addWindow(ToplevelRole* window, Container*& container);
            // removeWindow: remove a window from management, `container` will be the removed container if removed from tiling layout. 
            bool removeWindow(ToplevelRole* window, Container*& container);
            // toggleFloatWindow: toggle between stacking/tiling
            bool toggleStackWindow(ToplevelRole* window);
            // isTiledWindow: check if a window is tiled
            bool isTiledWindow(ToplevelRole* window);
            // isStackedWindow: check if a window is stacked(excluding untilable windows)
            bool isStackedWindow(ToplevelRole* window);
            // setActiveContainer: set currently activated container. This container is the only trusted target of cursor/keyboard input, and window insertion.
            void setActiveContainer(Container* container);
            // activatedContainer: get the activated container 
            inline Container* activatedContainer(){ return activeContainer; }
            // getFirstWindowContainer: util method for get the root of a workspace
            Container* getFirstWindowContainer(UInt32 workspace);
            // getWorkspace: get workspace in which the container resides.  
            UInt32 getWorkspace(Container* container) const;
            // getInsertTargetTiledContainer: gracefully find the next insertion target container.
            // This will first call `activatedContainer` and fallback to match surfaces under cursor if failed.
            Container* getInsertTargetTiledContainer(UInt32 workspace);
            // reapplyWindowState: refresh state for a window. This will smartly change states(e.g. floating, activated) according to window conditions.
            bool reapplyWindowState(ToplevelRole* window);
            // printContainerHrerachy: debug method for printing container hierachy
            void printContainerHierachy(UInt32 workspace);
            // 
            void _printContainerHierachy(Container* current);
            // initialize: init the manager.
            void initialize();

        private:
            // reflow: assign regions for windows
            void reflow(UInt32 workspace, const LRect& region, bool& success);
            //
            void _reflow(Container* container, const LRect& areaRemain, UInt32& accumulateCount);
            //
            Container* _getFirstWindowContainer(Container* container);
            // TODO: Ensure CURRENT_WORKSPACE is always the proper workspace for the next user action.
            UInt32 CURRENT_WORKSPACE=0;

            // update order: workspaceActiveContainers -> activeContainer;
            // TOOD: use workspaceActiveContainers[index] only, activeContainer will be deprecated
            Container* activeContainer;
            // array for activated container of every workspace
            std::vector<Container*> workspaceActiveContainers{WORKSPACES};
            // roots for every workspace
            std::vector<Container*> workspaceRoots{WORKSPACES};

            // set visiblity of a window
            void setWindowVisible(ToplevelRole* window, bool visible);
            static UInt32 countContainersOfWorkspace(const Container* root);
            // checksum for recalculating windows count
            UInt32 containerCount = 0;
            // all windows present(not only tiled ones)
            std::vector<ToplevelRole*> windows = {};

            /* resizing parameters */
            LPointF initialCursorPos;
            double initialHorizontalRatio;
            double initialVerticalRatio;
            Container* resizingHorizontalTarget;
            Container* resizingVerticalTarget;

            // workspace switching animation
            std::unique_ptr<LAnimation> m_workspaceSwitchAnimation;
            
            std::list<ToplevelRole*> m_slidingOutWindows;
            std::list<ToplevelRole*> m_slidingInWindows;
            bool m_isSwitchingWorkspace = false;
            int m_switchDirection = 0; // -1 = left slide; 1 = right slide;
            UInt32 m_targetWorkspace = 0;
            struct WindowStateManagerDeleter {
                void operator()(TileyWindowStateManager* p) const {
                    delete p;
                }
            };

            friend struct WindowStateManagerDeleter;

            TileyWindowStateManager();
            ~TileyWindowStateManager();
            static std::once_flag onceFlag;
            static std::unique_ptr<TileyWindowStateManager, WindowStateManagerDeleter> INSTANCE;
            TileyWindowStateManager(const TileyWindowStateManager&) = delete;
            TileyWindowStateManager& operator=(const TileyWindowStateManager&) = delete;
    };
}