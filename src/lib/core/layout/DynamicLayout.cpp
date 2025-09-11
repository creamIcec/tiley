#include <LLog.h>
#include <LCursor.h>
#include <tuple>

#include "DynamicLayout.hpp"
#include "LNamespaces.h"
#include "src/lib/core/layout/BaseLayout.hpp"

#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/types.hpp"

using namespace tiley;

DynamicLayout::DynamicLayout(int workspaceCount) : BaseLayout(workspaceCount){}

auto DynamicLayout::getLayoutData(Surface* surface){
    SPLIT_TYPE split = surface->size().w() >= surface->size().h() ? SPLIT_H : SPLIT_V;
    std::tuple<SPLIT_TYPE, Float32> data{ split, 0.5 };
    return data;
}

bool DynamicLayout::insert(UInt32 workspace, DynamicContainer* newWindowContainer){
    DynamicContainer* targetContainer = getInsertTargetTiledContainer(workspace);

    // if targetContainer is root, insert directly after desktop node
    if(targetContainer == workspaceRoots[workspace]){
        LLog::debug("No window presents, insert after desktop container");
        workspaceRoots[workspace]->child1 = newWindowContainer;
        newWindowContainer->parent = workspaceRoots[workspace];
        containerCount += 1;
        return true;
    }

    if(targetContainer){
        LLog::debug("Found window at cursor position");
        Surface* targetSurface = static_cast<Surface*>(targetContainer->window->surface());
        auto [splitType, splitRatio] = getLayoutData(targetSurface);
        DynamicContainer* targetContainer = static_cast<DynamicContainer*>(((ToplevelRole*)targetSurface->toplevel())->container);
        return insert(workspace, newWindowContainer, targetContainer, splitType, splitRatio);   
    }

    // try inserting after last activated container if failed to find targetContainer
    if(activeContainer != nullptr){
        LLog::debug("insert after last activated container");
        Surface* targetSurface = static_cast<Surface*>(activeContainer->window->surface());
        auto [splitType, splitRatio] = getLayoutData(targetSurface);
        return insert(workspace, newWindowContainer, activeContainer, splitType, splitRatio);
    }

    // Any other situations?

    LLog::error("[insertTile]: failed to insert window");
    return false;
}

bool DynamicLayout::insert(UInt32 workspace, DynamicContainer* newWindowContainer, DynamicContainer* targetContainer, SPLIT_TYPE splitType, Float32 splitRatio){

    L_UNUSED(workspace);

    if(targetContainer == nullptr){
        LLog::debug("Target container is null, stop inserting");
        return false;
    }

    if(newWindowContainer == nullptr){
        LLog::debug("New container is null, stop inserting");
        return false;
    }

    if(newWindowContainer == targetContainer){
        LLog::debug("New container and target container is identical, stop inserting");
        return false;
    }

    if(targetContainer->window == nullptr){
        LLog::debug("Target container must be typed window, stop inserting");
        return false;
    }

    auto parent = targetContainer->parent;
    
    auto splitContainer = new DynamicContainer();
    splitContainer->splitType = splitType;
    splitContainer->splitRatio = splitRatio;

    if(parent->child1 == targetContainer){
        parent->child1 = splitContainer;
        splitContainer->parent = parent;
    }else if(parent->child2 == targetContainer){
        parent->child2 = splitContainer;
        splitContainer->parent = parent;
    }else{
        // Any exception?
    }

    // TODO: move cursor access to where before calling `insertTile` for separation of data and environment
    const LRect& geo = targetContainer->geometry;
    LPointF mouse = cursor()->pos();
    bool before;

    if (splitType == SPLIT_H) {
        float midX = geo.x() + geo.w() * splitRatio;
        before = (mouse.x() < midX);
    } else {
        float midY = geo.y() + geo.h() * splitRatio;
        before = (mouse.y() < midY);
    }

    if (before) {
        splitContainer->child1 = newWindowContainer;
        splitContainer->child2 = targetContainer;
        //LLog::debug("Cursor at first part");
    } else {
        splitContainer->child1 = targetContainer;
        splitContainer->child2 = newWindowContainer;
        //LLog::debug("Cursor at second part");
    }
    
    newWindowContainer->parent = splitContainer;
    targetContainer->parent = splitContainer;
    LLog::debug("%d, %d, %d, %f", geo.x(), geo.y(), geo.w(), mouse.x());

    // accumulate trace amount
    containerCount += 2;

    return true;

}

bool TileyWindowStateManager::recalculate(){

    UInt32 workspace = CURRENT_WORKSPACE;
    Container* root = workspaceRoots[workspace];

    if (!root) {
        LLog::warning("[recalculate]: warning: root container of workspace id %u is null, this may be a bug, please report", workspace);
        return false;
    }

    if (!root->child1 && !root->child2) {
        LLog::debug("[recalculate]: no window exists in workspace id: %u, unable to recalculate layout", workspace);
        return false;
    }

    LLog::log("Currently recalculate layout for workspace id: %d", workspace);

    // Get root container of a workspace
    Output* rootOutput = nullptr;
    if (auto* first = getFirstWindowContainer(workspace)) {
        rootOutput = static_cast<ToplevelRole*>(first->window)->output;
    }
    if (!rootOutput) {
        rootOutput = static_cast<Output*>(cursor()->output());
    }
    if (!rootOutput) {
        LLog::warning("[recalculate]: no monitor available for workspace id %u, giving up", workspace);
        return false;
    }

    const LRect& availableGeometry = rootOutput->availableGeometry();

    containerCount = countContainersOfWorkspace(root);   

    bool reflowSuccess = false;
    LLog::debug("[recalculate]: executing reflow... ws=%u, nodes=%u", workspace, containerCount);

    reflow(workspace, availableGeometry, reflowSuccess);
    if (reflowSuccess){
        LLog::debug("[recalculate]: reflow layout successfully");
        return true;
    } else {
        LLog::debug("[recalculate]: failed to reflow workspace");
        return false;
    }
}

void DynamicLayout::reflow(UInt32 workspace, const LRect& region, bool& success){
    LLog::debug("[reflow]: recalculate geometry of tiled windows");

    // debug: print current container tree state
    //printContainerHierachy(workspace);

    UInt32 accumulateCount = 0;

    _reflow(workspaceRoots[workspace], region, accumulateCount);

    success = (accumulateCount == containerCount);

    LLog::debug("Amount of containers: %d", containerCount);
    LLog::debug("Recalculated containers: %d", accumulateCount);
    if(!success){
        LLog::error("[reflow]: Warning: count of recalculated containers is not equal to total containers");
    }
}

void DynamicLayout::_reflow(DynamicContainer* container, const LRect& areaRemain, UInt32& accumulateCount){
    if (container == nullptr) { 
        return;
    }

    container->geometry = areaRemain;

    // window visual gap
    const Int32 GAP = 5;

    if(container->window){
        LLog::debug("[reflow]: window recalculated, size: %dx%d, pos: (%d,%d)", areaRemain.w(), areaRemain.h(), areaRemain.x(), areaRemain.y());
        
        Surface* surface = static_cast<Surface*>(container->window->surface());
        
        // TODO: clients heartbeat detection
        /*
        ToplevelRole* toplevel = static_cast<ToplevelRole*>(container->window);
        if (toplevel->pendingConfiguration().serial != 0 &&
            toplevel->serial() != toplevel->pendingConfiguration().serial){
            LLog::debug("Client still processes last serial: %u, skip configuration", toplevel->pendingConfiguration().serial);
            return;
        }
        */
        
        if(surface->mapped()){
            const LRect& areaWithGaps = areaRemain;

            LRect areaForWindow = {
                areaWithGaps.x() + GAP,
                areaWithGaps.y() + GAP,
                areaWithGaps.w() - GAP * 2,
                areaWithGaps.h() - GAP * 2
            };

            // avoid negative geometry of windows too small
            if (areaForWindow.w() < 50) areaForWindow.setW(50);
            if (areaForWindow.h() < 50) areaForWindow.setH(50);

            container->containerView->setPos(areaForWindow.pos());
            container->containerView->setSize(areaForWindow.size());

            surface->setPos(areaRemain.pos());
            container->window->configureSize(areaForWindow.size());
            container->window->setExtraGeometry({GAP, GAP, GAP, GAP});
            
            LLog::debug("[reflow]: children of containerView: %zu", container->containerView->children().size());
            SurfaceView* surfaceView = static_cast<SurfaceView*>(container->containerView->children().front());

            const LRect& windowGeometry = container->window->windowGeometry();

            // if window does not support server side decorations and window draws its own decorations
            if(!container->window->supportServerSideDecorations() && (windowGeometry.x() > 0 || windowGeometry.y() > 0)){
                // set a custom position to align main part of window to upper-left corner
                surfaceView->setCustomPos(-windowGeometry.x(), -windowGeometry.y());
            }

            compositor()->repaintAllOutputs();
        }

        accumulateCount += 1;

    }else if(!container->window){

        LRect area1, area2;
        LLog::debug("[reflow]: container recalculated, type: %d, ratio: %f, size: %dx%d", container->splitType, container->splitRatio, container->geometry.width(), container->geometry.height());
        if(container->splitType == SPLIT_H){
            Int32 child1Width = (Int32)(areaRemain.width() * (container->splitRatio));
            Int32 child2Width = (Int32)(areaRemain.width() - child1Width);
            // potential rounding errors?
            area1 = {areaRemain.x(),areaRemain.y(),child1Width,areaRemain.height()};
            area2 = {areaRemain.x() + child1Width, areaRemain.y(), child2Width, areaRemain.height()};
        }else if(container->splitType == SPLIT_V){
            Int32 child1Height = (Int32)(areaRemain.height() * (container->splitRatio));
            Int32 child2Height = (Int32)(areaRemain.height() - child1Height);
            area1 = {areaRemain.x(),areaRemain.y(), areaRemain.width(), child1Height};
            area2 = {areaRemain.x(),areaRemain.y() + child1Height,areaRemain.width(), child2Height};
        }

        accumulateCount += 1;

        _reflow(container->child1, area1, accumulateCount);
        _reflow(container->child2, area2, accumulateCount);
    }
}
