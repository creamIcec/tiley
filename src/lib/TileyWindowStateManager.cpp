#include "TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/views/SurfaceView.hpp"
#include "src/lib/core/Container.hpp"
#include "src/lib/input/Pointer.hpp"
#include "src/lib/ipc/IPCManager.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"
#include "src/lib/output/Output.hpp"

#include <LCursor.h>
#include <LSeat.h>
#include <LPointer.h>
#include <LLog.h>
#include <LNamespaces.h>
#include <LOutput.h>
#include <LScene.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <functional> 

using namespace tiley;

UInt32 TileyWindowStateManager::getWorkspace(Container* container) const{
    if(!container){
        LLog::debug("container is null, return default workspace");
        return CURRENT_WORKSPACE;
    }

    // trace up to root
    Container* root = container;
    while(root->parent){
        root = root->parent;
    }

    for(UInt32 w = 0; w < WORKSPACES; w++){
        if(workspaceRoots[w] == root){
            return w;
        }
    }

    LLog::error("Error: Cannot find a root of target container. This may be a critical bug, please report.");
    return CURRENT_WORKSPACE;

}


void TileyWindowStateManager::setActiveContainer(Container* container){
    if(!container){
        activeContainer = container;
        return;
    }

    UInt32 workspace = getWorkspace(container);
    if(workspace >= 0 && workspace < WORKSPACES){
        workspaceActiveContainers[workspace] = container;
    }

    // compatibility: assign activeContainer to currently activated container.
    // TODO: replace all `activeContainer` access to `workspaceActiveContainers` only, 
    activeContainer = workspaceActiveContainers[workspace];
}

bool TileyWindowStateManager::insertTile(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE splitType, Float32 splitRatio){

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
    
    auto splitContainer = new Container();
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
    LLog::debug("%d, %d, %d, %d",geo.x(),geo.y(),geo.w(),mouse.x());

    // accumulate trace amount
    containerCount += 2;

    return true;
}

bool TileyWindowStateManager::insertTile(UInt32 workspace, Container* newWindowContainer, Float32 splitRatio){

    Container* targetContainer = getInsertTargetTiledContainer(workspace);

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
        Surface* windowSurface = static_cast<Surface*>(targetContainer->window->surface());
        SPLIT_TYPE split = windowSurface->size().w() >= windowSurface->size().h() ? SPLIT_H : SPLIT_V;
        return insertTile(workspace, newWindowContainer, ((ToplevelRole*)windowSurface->toplevel())->container, split, splitRatio);   
    }

    // try inserting after last activated container if failed to find targetContainer
    if(activeContainer != nullptr){
        LLog::debug("insert after last activated container");
        const LRect& size = activeContainer->window->surface()->size();
        SPLIT_TYPE split = size.w() >= size.h() ? SPLIT_H : SPLIT_V; 
        return insertTile(workspace, newWindowContainer, activeContainer, split, splitRatio);
    }
    // Any other situations?

    LLog::error("[insertTile]: failed to insert window");
    return false;
}

Container* TileyWindowStateManager::detachTile(LToplevelRole* window, FLOATING_REASON reason){

    // TODO: merge identical logical parts of `detachTile` and `removeTile`

    if(window == nullptr){
        LLog::debug("[detachTile]: target window is null, stop detaching");
        return nullptr;
    }

    Container* containerToDetach = ((ToplevelRole*)window)->container;
    // Save parent and grandParent pointers for later use
    Container* grandParent = containerToDetach->parent->parent;
    Container* parent = containerToDetach->parent;

    if(containerToDetach == nullptr){
        LLog::warning("[detachTile]: target window is not inside a container? This may be a bug, please report");
        return nullptr;
    }

    // TODO: Move to a better place for separation of data and environment
    window->surface()->raise();

    if(grandParent == nullptr){
        Container* sibling = (parent->child1 == containerToDetach) ? parent->child2 : parent->child1;
        if(sibling == nullptr){

            LLog::debug("[detachTile]: detach last window");
            parent->child1 = nullptr;
        
        }else{

            LLog::debug("[detachTile]: only one window remaining after detaching");
            parent->child1 = sibling;
            parent->child2 = nullptr;
            sibling->parent = parent;
            containerCount -= 1;

        }

        // Accumulating checksum count
        containerCount -= 1;

        containerToDetach->parent = nullptr;
        containerToDetach->floating_reason = reason;
        return containerToDetach;
    }
    
    Container* sibling = (parent->child1 == containerToDetach) ? parent->child2 : parent->child1;

    if (grandParent->child1 == parent) {
        grandParent->child1 = sibling;
    } else {
        grandParent->child2 = sibling;
    }

    if (sibling != nullptr) {
        sibling->parent = grandParent;
    }

    containerToDetach->parent = nullptr;
    containerToDetach->floating_reason = reason;
    delete parent;
    parent = nullptr;

    // Accumulating checksum count
    containerCount -= 2;

    return containerToDetach;
};

bool TileyWindowStateManager::attachTile(LToplevelRole* window){

    // Call `insertTile` directly cause logics are nearly the same

    if(!window){
        LLog::debug("[attachTile]: target window is null, stop attaching");
        return false;
    }

    ToplevelRole* windowToAttach = static_cast<ToplevelRole*>(window);

    if(!windowToAttach || !windowToAttach->container){
        LLog::debug("[attachTile]: target window is not inside a container, stop attaching");
        return false;
    }

    bool inserted = insertTile(CURRENT_WORKSPACE, windowToAttach->container, 0.5);
    if(inserted){
        windowToAttach->container->floating_reason = NONE;
    }

    return inserted;
}

Container* TileyWindowStateManager::removeTile(LToplevelRole* window){

    Container* result = nullptr;

    if(!window){
        LLog::debug("[removeTile]: target window is null, stop removing");
        return nullptr;
    }

    Container* containerToRemove = ((ToplevelRole*)window)->container;
    
    if(containerToRemove->floating_reason != NONE){
        LLog::debug("[removeTile]: removing a floating window");
        delete containerToRemove;
        containerToRemove = nullptr;
        return nullptr;
    }
    
    Container* grandParent = containerToRemove->parent->parent;
    Container* parent = containerToRemove->parent;

    if(containerToRemove == nullptr){
        LLog::warning("[removeTile]: target window is not inside a container? This may be a bug, please report");
        return nullptr;
    }

    if(grandParent == nullptr){
        Container* sibling = (parent->child1 == containerToRemove) ? parent->child2 : parent->child1;
        if(sibling == nullptr){
            LLog::debug("[removeTile]: removing last existing window");
            parent->child1 = nullptr;
            result = workspaceRoots[getWorkspace(containerToRemove)];
        }else{
            LLog::debug("[removeTile]: only one window remains after removing");
            parent->child1 = sibling;
            parent->child2 = nullptr;
            sibling->parent = parent;
            // Accumulating checksum count
            containerCount -= 1;
            result = sibling;
        }

        delete containerToRemove;
        containerToRemove = nullptr;

        // Accumulating checksum count
        containerCount -= 1;

        return result;
    }
    
    Container* sibling = (parent->child1 == containerToRemove) ? parent->child2 : parent->child1;

    // parent reclaims its children 
    if (grandParent->child1 == parent) {
        grandParent->child1 = sibling;
    } else {
        grandParent->child2 = sibling;
    }

    if (sibling != nullptr) {
        sibling->parent = grandParent;
        result = sibling;
    }

    containerToRemove->parent = nullptr;
    delete parent;
    delete containerToRemove;
    parent = nullptr;
    containerToRemove = nullptr;

    // Accumulating checksum count
    containerCount -= 2;

    return result;

}

bool TileyWindowStateManager::resizeTile(LPointF cursorPos){

    // the direction of adjustment is set up generally by where the cursor drags

    bool resized = false;
    LPointF mouseDelta = cursorPos - initialCursorPos;

    // cursor dragging horizontally
    if (resizingHorizontalTarget) {
        double totalWidth = resizingHorizontalTarget->geometry.w();
        if (totalWidth >= 1) {
            double ratioDelta = mouseDelta.x() / totalWidth;
            double newRatio = initialHorizontalRatio + ratioDelta;
            resizingHorizontalTarget->splitRatio = std::min(0.95, std::max(0.05, newRatio));
            resized = true;
        }
    }

    // cursor dragging vertically
    if (resizingVerticalTarget) {
        double totalHeight = resizingVerticalTarget->geometry.h();
        if (totalHeight >= 1) {
            double ratioDelta = mouseDelta.y() / totalHeight;
            double newRatio = initialVerticalRatio + ratioDelta;
            resizingVerticalTarget->splitRatio = std::min(0.95, std::max(0.05, newRatio));
            resized = true;
        }
    }

    if (resized) {
        LLog::debug("[resizeTile] successfully resized tiled windows");
        return true;
    }
    return false;
}

void TileyWindowStateManager::printContainerHierachy(UInt32 workspace){

    if(workspace < 0 || workspace >= WORKSPACES){
        LLog::log("[printContainerHierachy]: Target workspace id is out of range: %d, stop printing", workspace);
        return;
    }

    LLog::log("***************Container Hierachy of workspace: %d***************", workspace);
    auto current = workspaceRoots[workspace];
    _printContainerHierachy(current);
    LLog::log("***************************************");
}

void TileyWindowStateManager::_printContainerHierachy(Container* current){
    
    if(!current){
        return;
    }
    LLog::log("container: %d, type: %s, parent: %d, child1: %d, child2: %d, active monitor: %d", 
                        current, 
                        current->window != nullptr ? "window" : "container", 
                        current->parent, 
                        current->child1, 
                        current->child2,
                        current->window ? ((ToplevelRole*)current->window)->output : nullptr
    );

    if(current->child1){
        _printContainerHierachy(current->child1);
    }
    if(current->child2){
        _printContainerHierachy(current->child2);
    }
}


void TileyWindowStateManager::reflow(UInt32 workspace, const LRect& region, bool& success){
    LLog::debug("[reflow]: recalculate geometry of tiled windows");
    // 调试: 打印当前容器树
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

void TileyWindowStateManager::_reflow(Container* container, const LRect& areaRemain, UInt32& accumulateCount){

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

bool TileyWindowStateManager::addWindow(ToplevelRole* window, Container* &container){

    if(!window){
        LLog::debug("[addWindow]: Attempt to add a null window, stop adding");
        return false;
    }

    Surface* surface = static_cast<Surface*>(window->surface());

    LLog::debug("[addWindow]: adding window with surface position: (%d,%d)", surface->pos().x(), surface->pos().y());
    for(LSurface* surf : surface->children()){
        LLog::debug("[addWindow]: position of children surfaces: (%d,%d)", surf->pos().x(), surf->pos().y());
    }

    Output* activeOutput = ((Output*)cursor()->output());
    const LRect& availableGeometry = activeOutput->availableGeometry();

    if(!surface){
        LLog::debug("[addWindow]: surface of target window is null, is it destroyed?");
        return false;
    }

    // print geometry debug info
    //surface->printWindowGeometryDebugInfo(activeOutput, availableGeometry);

    // TODO: Allow add window to other inactive outputs
    window->output = activeOutput;

    switch (window->type) {
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::debug("[addWindow]: added a restricted window, address of surface object: %d, layer: %d", surface, surface->layer());
            reapplyWindowState(window);
            break;
        }
        case NORMAL:{
            LLog::debug("[addWindow]: added a common window, address of surface object: %d, layer: %d", surface, surface->layer());
            Container* newContainer = new Container(window);
            insertTile(CURRENT_WORKSPACE, newContainer, 0.5);
            reapplyWindowState(window);
            container = newContainer;
            break;
        }
        default:
            LLog::warning("[addWindow]: warning: added a unknown window, this will not be handled by manager");
    }

    window->workspaceId = CURRENT_WORKSPACE;

    return true;
}

bool TileyWindowStateManager::removeWindow(ToplevelRole* window, Container*& container){
    switch(window->type){
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::debug("[removeWindow]: removed a restricted window");
            break;
        }
        case NORMAL:{
            Container* lastActiveContainer = removeTile(window);
            if(lastActiveContainer != nullptr){
                container = lastActiveContainer;
                return true;
            }
            break;
        }
        default:
            LLog::warning("[removeWindow]: failed to remove a window that is unmanaged.");
    }

    return false;
}

bool TileyWindowStateManager::reapplyWindowState(ToplevelRole* window){
    
    Surface* surface = static_cast<Surface*>(window->surface());

    if(!surface){
        LLog::warning("[reapplyWindowState]: surface of target window is null, is it destroyed?");
        return false;
    }

    if(window->type == RESTRICTED_SIZE || window->type == FLOATING){
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }
    }

    if(!window->container){
        return false;
    }
    
    if(window->container->floating_reason == STACKING){
        LLog::debug("[reapplyWindowState]: toggle window to 'stack' mode, temporarily disable containerView");
        window->container->enableContainerView(false);
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }
    }else if(window->container->floating_reason == MOVING){
        LLog::debug("[reapplyWindowState]: window is moving, temporarily disable containerView");
        window->container->enableContainerView(false);
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }
    }else{
        LLog::debug("[reapplyWindowState]: window is in 'tiled' mode, enable containerView");
        window->container->enableContainerView(true);
    }

    // activate window
    window->configureState(window->pendingConfiguration().state | LToplevelRole::Activated);
    seat()->pointer()->setFocus(window->surface());
    seat()->keyboard()->setFocus(window->surface());
    
    return true;
}

bool TileyWindowStateManager::toggleStackWindow(ToplevelRole* window){

    if(!window || !window->container){
        LLog::debug("[toggleStackWindow]: target window and it's container should not be null, stop toggling");
        return false;
    }

    if(window->type != NORMAL){
        LLog::debug("[toggleStackWindow]: target window must be tilable, stop toggling");
        return false;
    }

    if(window->container->floating_reason == MOVING){
        LLog::debug("[toggleStackWindow]: target window is moving, stop toggling");
        return false;
    }

    if(window->container->floating_reason == NONE){
        Container* detachedContainer = detachTile(window, STACKING);
        if(detachedContainer){
            reapplyWindowState(window);
            recalculate();
        }
        return detachedContainer != nullptr;
    }else if(window->container->floating_reason == STACKING){
        bool attached = attachTile(window);
        // TODO: Allow inserting to other inactive workspaces
        if(attached){
            reapplyWindowState(window);
            recalculate();
        }
        return attached;
    }

    LLog::error("[toggleStackWindow]: failed to toggle window stacking state, this may be a bug, please report");
    return false;
}

Container* TileyWindowStateManager::getFirstWindowContainer(UInt32 workspace){
    
    if(workspace < 0 || workspace >= WORKSPACES){
        LLog::warning("[getFirstWindowContainer]: target workspace id is out of range, stop fetching");
        return nullptr;
    }

    Container* root = workspaceRoots[workspace];
    Container* result = nullptr;

    if(root && !root->child1 && !root->child2){
        LLog::debug("[getFirstWindowContainer]: no window exists in target workspace, return nullptr");
        return nullptr;
    // if only one child presents
    }else if(root && root->child1 && root->child1->window){
        return root->child1;
    // if more than one children presents
    }else if(root && root->child1 && !root->child1->window){
        result =  _getFirstWindowContainer(root);
    }

    if(result == nullptr){
        LLog::debug("[getFirstWindowContainer]: cannot find root container of workspace: %d, returning nullptr", workspace);
    }
    return result;
}

Container* TileyWindowStateManager::_getFirstWindowContainer(Container* container){
    
    if(container->child1 != nullptr){
        return _getFirstWindowContainer(container->child1);
    }
    if(container->child1 == nullptr && container->child2 == nullptr && container->window){
        return container;
    }
    if(container->child2 != nullptr){
        return _getFirstWindowContainer(container->child2);
    }

    return nullptr;
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


UInt32 TileyWindowStateManager::countContainersOfWorkspace(const Container* root) {
    if (!root) return 0;

    UInt32 n = 0;
    std::function<void(const Container*)> dfs = [&](const Container* c){
        if (!c) return;
        
        // count container whether it is a 'container' or a 'window'
        ++n;
        dfs(c->child1);
        dfs(c->child2);
    };

    dfs(root);
    return n;
}




// 获取下一个要显示的平铺窗口的插入目标容器
// 该函数调用时将静态保存鼠标位置和"锁定"目标显示器, 也就是存储目标容器为一个内部状态
// 因此, 该函数推荐在要插入新的窗口时紧跟着调用。如果不这样的话, 可能会出现目标和期望不一致的情况
// 比如: 鼠标目前在这个位置, 但因为调用该函数过早/过晚, 导致鼠标位置和插入位置不一样的情况
// 当然, 如果目标就是不跟随鼠标的(例如后期通过配置), 可以随时使用该函数而无碍
Container* TileyWindowStateManager::getInsertTargetTiledContainer(UInt32 workspace){

    // 函数分为两阶段逻辑: 一阶段直接返回由各种来源设置的"上一个活动容器"(通过setActiveContainer), 如果该容器不存在则进入二阶段回退, 计算鼠标坐标处的容器。

    // 特殊: 桌面根节点是所有的fallback
    Container* root = workspaceRoots[workspace];

    if(!root){
        LLog::error("[getInsertTargetTiledContainer]: 工作区没有节点, 可能是bug, 停止获取鼠标处的容器");
        return nullptr;
    }

    // 如果工作区为空, 直接返回自己
    if(root->child1 == nullptr && root->child2 == nullptr){
        LLog::debug("[getInsertTargetTiledContainer]: 返回工作区根节点");
        return root;
    }

    // 一阶段, 命中缓存
    Container* activeContainer = workspaceActiveContainers[workspace];
    if(activeContainer){
        LLog::debug("[getInsertTargetTiledContainer]: 返回工作区 %u 上一个活动的Container", workspace);
        return activeContainer;
    }

    // 二阶段, 未命中缓存, 回退到鼠标位置查找
    auto filter = [this](LSurface* s){
        
        auto surface = static_cast<Surface*>(s);

        // 条件1: 必须是一个窗口
        if(!surface->toplevel()){
            return false;
        }

        // 条件2: 其视图必须可见
        if(!surface->getView() || !surface->getView()->visible()){
            return false;
        }

        // 条件3: 必须是可被平铺窗口和平铺中的非浮动窗口
        auto window = static_cast<ToplevelRole*>(surface->toplevel());
        if(window->type != NORMAL || (window->container && window->container->floating_reason != NONE)){
            return false;
        }

        // 条件 4: 健壮性: 确保该窗口没有正在被移动
        for (LToplevelMoveSession* session : seat()->toplevelMoveSessions()) {
            if (s->toplevel() == session->toplevel()) {
                return false;
            }
        }

        // 检查完全通过!
        return true;

    };

    LSurface* targetSurface = static_cast<Pointer*>(seat()->pointer())->surfaceAtWithFilter(cursor()->pos(), filter);
    
    if (targetSurface) {
        // 找到了目标窗口
        ToplevelRole* targetWindow = static_cast<ToplevelRole*>(targetSurface->toplevel());
        LLog::debug("[getInsertTargetTiledContainer]: 返回鼠标下的窗口");
        return targetWindow->container;
    }

    // 三阶段, 鼠标位置也没有, 返回根节点
    LLog::debug("[getInsertTargetTiledContainer]: 返回桌面容器");
    return root;

};


void TileyWindowStateManager::setupResizeSession(LToplevelRole* window, LBitset<LEdge> edges, const LPointF& cursorPos)
{
    // 清空旧的上下文
    resizingHorizontalTarget = nullptr;
    resizingVerticalTarget = nullptr;
    initialCursorPos = cursorPos;

    Container* container = static_cast<ToplevelRole*>(window)->container;
    if (!container || !container->parent) return;

    Container* current = container;
    bool needsHorizontal = edges.check(LEdgeLeft) || edges.check(LEdgeRight);
    bool needsVertical = edges.check(LEdgeTop) || edges.check(LEdgeBottom);

    while (current->parent && current->parent != workspaceRoots[CURRENT_WORKSPACE])
    {
        Container* parent = current->parent;
        if (needsHorizontal && !resizingHorizontalTarget && parent->splitType == SPLIT_H) {
            resizingHorizontalTarget = parent;
            initialHorizontalRatio = parent->splitRatio; // 记录初始比例
        }
        if (needsVertical && !resizingVerticalTarget && parent->splitType == SPLIT_V) {
            resizingVerticalTarget = parent;
            initialVerticalRatio = parent->splitRatio; // 记录初始比例
        }
        if ((!needsHorizontal || resizingHorizontalTarget) && (!needsVertical || resizingVerticalTarget)) {
            break;
        }
        current = parent;
    }
}


bool TileyWindowStateManager::isTiledWindow(ToplevelRole* window){

    // 优先级1: 是一个普通窗口, 但是由于正在移动/被用户浮动而脱离了平铺层, 返回false
    if(window->type == NORMAL && window->container && window->container->floating_reason != NONE){
        return false;
    }

    // 优先级2: 是一个在平铺层的普通窗口
    if(window->type == NORMAL){
        return true;
    }

    // 优先级3: 其他情况
    return false;
}

bool TileyWindowStateManager::isStackedWindow(ToplevelRole* window){
    if(window->container && window->container->floating_reason == STACKING){
        return true;
    }

    return false;
}

// 递归设置某个窗口及其子窗口的可见性
void TileyWindowStateManager::setWindowVisible(ToplevelRole* window, bool visible) {
    // 入参检查
    if (!window || !window->surface()){
        LLog::warning("要修改可见性的窗口是空指针, 或者其surface不存在, 停止修改");
        return;
    }

    // 1. 设置window自身的surfaceView的可见性(对于平铺窗口而言是双重保障, 对于浮动窗口而言就是唯一手段)
    auto windowSurface = static_cast<Surface*>(window->surface());
    if(windowSurface && windowSurface->getView()){
        windowSurface->getView()->setVisible(visible);
    }

    // 2. 如果是可平铺窗口, 设置其containerView可见性
    if (window->container && window->container->getContainerView() && window->container->getContainerView()->mapped()){
        window->container->getContainerView()->setVisible(visible);
    }

    // 3. 再设置子Surface的可见性
    for(auto s : window->surface()->children()){
        auto surface = static_cast<Surface*>(s);
        surface->getView()->setVisible(visible);
    }

    // 4. 最后, 关闭所有弹出菜单
    seat()->dismissPopups();
}

// 切换工作区
bool TileyWindowStateManager::switchWorkspace(UInt32 target) {

    // 如果正在切换,或者目标无效,则直接返回
    if (m_isSwitchingWorkspace || target >= WORKSPACES || target == CURRENT_WORKSPACE) {
        return false;
    }

    LLog::debug("开始切换工作区 %u -> %u", CURRENT_WORKSPACE, target);

    // 设置动画状态
    m_isSwitchingWorkspace = true;
    m_targetWorkspace = target;
    m_switchDirection = (target > CURRENT_WORKSPACE) ? -1 : 1; // 目标 > 当前,向左滑

    // 清空上次动画的残留（以防万一）
    m_slidingOutWindows.clear();
    m_slidingInWindows.clear();

    // 重新计算一次布局,确保所有窗口的 targetRect 是正确的
    recalculate();

    // 填充要滑出和滑入的窗口列表
    for(auto* surface : Louvre::compositor()->surfaces()){
        if(surface->toplevel()){
            auto* window = static_cast<ToplevelRole*>(surface->toplevel());
            if (window->workspaceId == CURRENT_WORKSPACE) {
                m_slidingOutWindows.push_back(window);
            } else if (window->workspaceId == target) {
                m_slidingInWindows.push_back(window);
                // 【关键】让即将滑入的窗口提前可见,但它们的位置会在动画开始时被设置到屏幕外
                setWindowVisible(window, true);
            }
        }
    }

    // 配置并启动动画
    m_workspaceSwitchAnimation->setDuration(250); // 250ms 动画时长
    m_workspaceSwitchAnimation->start();

    return true;

}

void TileyWindowStateManager::initialize(){
    
    m_workspaceSwitchAnimation = std::make_unique<LAnimation>();
    // 初始化切换工作区动画
    m_workspaceSwitchAnimation->setOnUpdateCallback([this](Louvre::LAnimation* anim) {
            // 1. 获取线性的动画进度 (0.0 to 1.0)
        const Float64 linearValue = anim->value();

        // 2. 【核心】应用 EaseOut (正弦) 缓动函数
        //    这会将线性的进度转换为非线性的、开始快结束慢的平滑曲线
        const Float64 easedValue = sin(linearValue * M_PI / 2.0);

        // 获取主输出设备
        auto* output = Louvre::compositor()->outputs().front();
        if (!output) return;

        const int screenWidth = output->size().w();

        // 3. 在所有位置计算中使用我们处理过的 easedValue
        
        // 更新滑出窗口的位置
        for (auto* window : m_slidingOutWindows) {
            if (window->container && window->container->getContainerView()) {
                const auto& originalRect = window->container->geometry;
                int newX = originalRect.x() + (m_switchDirection * screenWidth * easedValue); // <-- 使用 easedValue
                window->container->getContainerView()->setPos(newX, originalRect.y());
            }
        }

        // 更新滑入窗口的位置
        for (auto* window : m_slidingInWindows) {
            if (window->container && window->container->getContainerView()) {
                const auto& targetRect = window->container->geometry;
                int startX = targetRect.x() - (m_switchDirection * screenWidth);
                int newX = startX + (m_switchDirection * screenWidth * easedValue); // <-- 使用 easedValue
                window->container->getContainerView()->setPos(newX, targetRect.y());
            }
        }
        
        output->repaint();
    });

    m_workspaceSwitchAnimation->setOnFinishCallback([this](Louvre::LAnimation*) {
        // 动画结束,执行最终的状态切换和清理工作
        
        // 1. 隐藏所有滑出的窗口,并重置它们的位置
        for (auto* window : m_slidingOutWindows) {
            setWindowVisible(window, false);
            if (window->container && window->container->getContainerView()) {
                window->container->getContainerView()->setPos(window->container->geometry.pos());
            }
        }

        // 2. 确保所有滑入的窗口在它们的最终位置
        for (auto* window : m_slidingInWindows) {
             if (window->container && window->container->getContainerView()) {
                window->container->getContainerView()->setPos(window->container->geometry.pos());
            }
        }

        // 3. 更新工作区状态 (这部分逻辑从旧的 switchWorkspace 移过来)
        CURRENT_WORKSPACE = m_targetWorkspace;
        activeContainer = workspaceActiveContainers[CURRENT_WORKSPACE];

        auto seat = Louvre::seat();
        if(activeContainer && activeContainer->window){
            /*if(!seat->keyboard()->grab()){
                seat->keyboard()->setFocus(activeContainer->window->surface());
            }*/
        } else {
            seat->keyboard()->setFocus(nullptr);
        }

        // 4. 发送 IPC 消息
        IPCManager::getInstance().broadcastWorkspaceUpdate(CURRENT_WORKSPACE, WORKSPACES);
        
        // 5. 清理状态
        m_isSwitchingWorkspace = false;
        m_slidingOutWindows.clear();
        m_slidingInWindows.clear();

        LLog::debug("切换到工作区 %u 动画完成", CURRENT_WORKSPACE);
    });
}

TileyWindowStateManager& TileyWindowStateManager::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyWindowStateManager());
    });
    return *INSTANCE;
}

std::unique_ptr<TileyWindowStateManager, TileyWindowStateManager::WindowStateManagerDeleter> TileyWindowStateManager::INSTANCE = nullptr;
std::once_flag TileyWindowStateManager::onceFlag;

TileyWindowStateManager::TileyWindowStateManager()
  : workspaceRoots(WORKSPACES, nullptr){
    // 为每个工作区创建一个根容器,并初始化为“桌面”状态
    for (int i = 0; i < WORKSPACES; ++i) {
        Container* root = new Container();
        root->splitType = SPLIT_H;
        root->splitRatio = 1.0f;
        workspaceRoots[i] = root;
    }
    containerCount += 1;
    
}

//删除对应根节点
TileyWindowStateManager::~TileyWindowStateManager(){
    for (auto root : workspaceRoots) {
        delete root;
    }
}