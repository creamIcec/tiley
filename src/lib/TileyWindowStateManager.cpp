#include "TileyWindowStateManager.hpp"
#include "LBitset.h"
#include "LEdge.h"
#include "LSurface.h"
#include "LToplevelMoveSession.h"
#include "LToplevelRole.h"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/client/views/SurfaceView.hpp"
#include "src/lib/core/Container.hpp"
#include "src/lib/input/Pointer.hpp"
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

//找底层窗口
static Surface* findFirstParentToplevelSurface(Surface* surface){
    Surface* iterator = surface;
    while(iterator != nullptr && surface->toplevel() == nullptr){
        if(surface == nullptr){
            // TODO: 原因?
             //那这里不应该用iterator进行判断吗
            /*
             while (iterator) {
        // 如果这个 Surface 已经是顶层了，就直接返回
        if (iterator->toplevel()) {
            return iteratorr;
        }
        // 否则沿着 parent() 指针往上找
        iterator = static_cast<Surface*>(iterator->parent());
    }
    // 找到顶层前就走到根了，打个日志，返回 nullptr
    LLog::log("无法找到一个surface的父窗口");
    return nullptr;
}
            
            */
            LLog::log("无法找到一个surface的父窗口");
            return nullptr;
        }
        iterator = (Surface*)iterator->parent();
    }
    return iterator;
}

UInt32 TileyWindowStateManager::getWorkspace(Container* container) const{
    if(!container){
        LLog::debug("传入的容器为空, 返回当前工作区");
        return CURRENT_WORKSPACE;
    }

    // 一路回溯到根节点
    Container* root = container;
    while(root->parent){
        root = root->parent;
    }

    for(UInt32 w = 0; w < WORKSPACES; w++){
        if(workspaceRoots[w] == root){
            return w;
        }
    }

    LLog::error("错误: 无法找到一个容器所在树的根节点, 可能存在bug, 请报告!");
    return CURRENT_WORKSPACE;

}


void TileyWindowStateManager::setActiveContainer(Container* container){
    if(!container){
        activeContainer = container;  //设置为空容器
        return;
    }

    UInt32 workspace = getWorkspace(container);
    if(workspace >= 0 && workspace < WORKSPACES){
        workspaceActiveContainers[workspace] = container;
    }

    // 同步更新, 非常重要, 在我们没有完全迁移过来之前, 需要随时更新activeContainer和workspaceActiveContainers, 让他们保持同步
    activeContainer = workspaceActiveContainers[workspace];
}

// insert(插入)函数
// 检查清单:
// 父容器是否指向了子容器?
// 子容器是否指向了父容器?
bool TileyWindowStateManager::insertTile(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE splitType, Float32 splitRatio){

    L_UNUSED(workspace);

    if(targetContainer == nullptr){
        LLog::debug("目标窗口为空, 停止插入");
        return false;
    }

    if(newWindowContainer == nullptr){
        LLog::debug("传入的窗口为空, 停止插入");
        return false;
    }

    if(newWindowContainer == targetContainer){
        LLog::debug("容器不能作为自己的子节点, 停止插入");
        return false;
    }

    if(targetContainer->window == nullptr){
        LLog::debug("目标位置必须是窗口, 停止插入");
        return false;
    }

    // 1. 获取父容器
    auto parent = targetContainer->parent;
    
    // 2. 创建新的分割容器
    auto splitContainer = new Container();
    splitContainer->splitType = splitType;
    splitContainer->splitRatio = splitRatio;

    // 3. 判断目标容器是父容器的child几, 挂载分割容器上去
    if(parent->child1 == targetContainer){
        parent->child1 = splitContainer;
        splitContainer->parent = parent;
    }else if(parent->child2 == targetContainer){
        parent->child2 = splitContainer;
        splitContainer->parent = parent;
    }else{
        // TODO: 可能有意外嘛?   这里测试的时候可以看看，逻辑上感觉没问题
    }

    // 4. 挂载窗口
    // TODO: 根据鼠标位置不同决定1和2分别是谁    鼠标位置？计算坐标即可。完成。
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
    // 鼠标在“前半区”：新窗口 child1，原窗口 child2
    splitContainer->child1 = newWindowContainer;
    splitContainer->child2 = targetContainer;
    //LLog::debug("前半区");
    } else {
    // 鼠标在“后半区”：原窗口 child1，新窗口 child2
    splitContainer->child1 = targetContainer;
    splitContainer->child2 = newWindowContainer;
    //LLog::debug("后半区");
    }
    newWindowContainer->parent = splitContainer;
    targetContainer->parent = splitContainer;
    LLog::debug("%d, %d, %d, %d",geo.x(),geo.y(),geo.w(),mouse.x());
    //LLog::debug("确实在调用这个函数");
    /*
    splitContainer->child1 = targetContainer;
    splitContainer->child2 = newWindowContainer;
    targetContainer->parent = splitContainer;
    newWindowContainer->parent = splitContainer;
*/
    // 增加追踪数量(+1 分割容器, +1 窗口)
    containerCount += 2;

    return true;
}


// insert(插入)函数
bool TileyWindowStateManager::insertTile(UInt32 workspace, Container* newWindowContainer, Float32 splitRatio){

    // 找到鼠标所在位置的平铺Container
    Container* targetContainer = getInsertTargetTiledContainer(workspace);

    // 如果是根节点, 表示是桌面
    if(targetContainer == workspaceRoots[workspace]){
        // 只有桌面的时候, 直接插入到桌面节点
        LLog::debug("只有桌面节点, 仅插入窗口");
        workspaceRoots[workspace]->child1 = newWindowContainer;
        newWindowContainer->parent = workspaceRoots[workspace];
        containerCount += 1;
        return true;
    }

    if(targetContainer){
        LLog::debug("鼠标位置有窗口容器");
        Surface* windowSurface = static_cast<Surface*>(targetContainer->window->surface());
        SPLIT_TYPE split = windowSurface->size().w() >= windowSurface->size().h() ? SPLIT_H : SPLIT_V;
        return insertTile(workspace, newWindowContainer, ((ToplevelRole*)windowSurface->toplevel())->container, split, splitRatio);   
    }

    // 如果既不是桌面, 鼠标也没有聚焦到任何窗口, 尝试插入上一个被激活的container之后
    if(activeContainer != nullptr){
        LLog::debug("插入到上一个激活的窗口后");
        const LRect& size = activeContainer->window->surface()->size();
        SPLIT_TYPE split = size.w() >= size.h() ? SPLIT_H : SPLIT_V; 
        return insertTile(workspace, newWindowContainer, activeContainer, split, splitRatio);
    }
    // TODO: 其他鼠标没有聚焦的情况，比如呢？
    //

    LLog::error("[insertTile]: 插入失败, 未知情况。");
    return false;
}

Container* TileyWindowStateManager::detachTile(LToplevelRole* window, FLOATING_REASON reason){
    // 和removeTile逻辑接近, 就是不删除容器。
    // TODO: 考虑和remove合并, 通过一个参数区分是分离还是关闭

    if(window == nullptr){
        LLog::debug("要分离的窗口为空, 停止分离");
        return nullptr;
    }

    // 2. 找到这个window对应的container
    Container* containerToDetach = ((ToplevelRole*)window)->container;
    // 同时保存父亲和祖父的指针
    Container* grandParent = containerToDetach->parent->parent;
    Container* parent = containerToDetach->parent;

    if(containerToDetach == nullptr){
        LLog::warning("[detachTile]: 要分离的窗口没有container? 正常情况下不应该发生, 停止分离");
        return nullptr;
    }

    // TODO: 将下面的这行代码移动到一个更好的位置。视图和逻辑应该分开处理。
    window->surface()->raise();

    // 3. 处理只剩一个或两个窗口的情况
    if(grandParent == nullptr){
        Container* sibling = (parent->child1 == containerToDetach) ? parent->child2 : parent->child1;
        if(sibling == nullptr){
            // 3.1 这是最后一个窗口
            LLog::debug("分离最后一个窗口");
            parent->child1 = nullptr;
            
        }else{
            // 3.2 屏幕上还剩一个窗口
            LLog::debug("分离后仅剩一个窗口");
            // 直接把兄弟节点的内容(它本身应该是个窗口容器)移动到根容器
            parent->child1 = sibling;
            parent->child2 = nullptr;
            sibling->parent = parent; // 更新兄弟节点的父指针
            //分割容器 -1
            containerCount -= 1;
        }
        // 窗口 -1
        containerCount -= 1;

        containerToDetach->parent = nullptr;
        containerToDetach->floating_reason = reason;
        return containerToDetach;
    }
    
    // 4. 不止一个窗口时, 保存兄弟container
    Container* sibling = (parent->child1 == containerToDetach) ? parent->child2 : parent->child1;

    // 4.1 让祖父节点收养兄弟节点
    if (grandParent->child1 == parent) {
        grandParent->child1 = sibling;
    } else { // grandParent->child2 == parent
        grandParent->child2 = sibling;
    }

    // 4.2 更新兄弟节点的父指针
    if (sibling != nullptr) { // 如果被删除的窗口有兄弟
        sibling->parent = grandParent;
    }

    // 清理所有被移除的容器
    containerToDetach->parent = nullptr; // 断开连接，好习惯
    containerToDetach->floating_reason = reason;
    delete parent; // 删除旧的分割容器
    parent = nullptr;

    // 窗口 -1, 分割容器 -1
    containerCount -= 2;

    return containerToDetach;
};

bool TileyWindowStateManager::attachTile(LToplevelRole* window){
    // 直接调用insertTile

    if(!window){
        LLog::debug("要合并的窗口为空, 取消合并");
        return false;
    }

    ToplevelRole* windowToAttach = static_cast<ToplevelRole*>(window);

    if(!windowToAttach || !windowToAttach->container){
        LLog::debug("要合并的窗口没有容器, 取消合并");
        return false;
    }

    bool inserted = insertTile(CURRENT_WORKSPACE, windowToAttach->container, 0.5);
    if(inserted){
        windowToAttach->container->floating_reason = NONE;
    }

    return inserted;
}

// 如何移除(remove)?
// 1. 由于关闭可以连带进行, 我们必须传入目标窗口。只使用鼠标位置是不可靠
Container* TileyWindowStateManager::removeTile(LToplevelRole* window){

    Container* result = nullptr;

    if(!window){
        LLog::debug("要移除的窗口为空, 停止移除");
        return nullptr;
    }

    // 2. 找到这个window对应的container
    Container* containerToRemove = ((ToplevelRole*)window)->container;
    // 同时保存父亲和祖父的指针
    Container* grandParent = containerToRemove->parent->parent;
    Container* parent = containerToRemove->parent;

    if(containerToRemove == nullptr){
        LLog::warning("[removeTile]: 要移除的窗口没有container? 正常情况下不应该发生, 停止移除");
        return nullptr;
    }

    // 3. 处理只剩一个或两个窗口的情况
    if(grandParent == nullptr){
        Container* sibling = (parent->child1 == containerToRemove) ? parent->child2 : parent->child1;
        if(sibling == nullptr){
            // 3.1 这是最后一个窗口
            LLog::debug("关闭最后一个窗口");
            parent->child1 = nullptr;
            // TODO: 找到窗口所在的workspace
            result = workspaceRoots[0];
        }else{
            // 3.2 屏幕上还剩一个窗口
            LLog::debug("关闭后仅剩一个窗口");
            // 直接把兄弟节点的内容(它本身应该是个窗口容器)移动到根容器
            parent->child1 = sibling;
            parent->child2 = nullptr;
            sibling->parent = parent; // 更新兄弟节点的父指针
            //分割容器 -1
            containerCount -= 1;
            result = sibling;
        }

        delete containerToRemove;
        containerToRemove = nullptr;

        // 窗口 -1
        containerCount -= 1;

        return result;
    }
    
    // 4. 不止一个窗口时, 保存兄弟container
    Container* sibling = (parent->child1 == containerToRemove) ? parent->child2 : parent->child1;

    // 4.1 让祖父节点收养兄弟节点
    if (grandParent->child1 == parent) {
        grandParent->child1 = sibling;
    } else { // grandParent->child2 == parent
        grandParent->child2 = sibling;
    }

    // 4.2 更新兄弟节点的父指针
    if (sibling != nullptr) { // 如果被删除的窗口有兄弟
        sibling->parent = grandParent;
        result = sibling;
    }

    // 清理所有被移除的容器
    containerToRemove->parent = nullptr; // 断开连接，好习惯
    delete parent; // 删除旧的分割容器
    delete containerToRemove; // 删除被关闭窗口的容器
    parent = nullptr;
    containerToRemove = nullptr;

    // 窗口 -1, 分割容器 -1
    containerCount -= 2;

    return result;

}

// 这是你的 resizeTile 的最终形态
bool TileyWindowStateManager::resizeTile(LPointF cursorPos){

    // 计算机坐标系从屏幕左上角开始向右为x正方向, 向下为y正方向。在平铺式管理器中, 我们遵循一条法则:
    // 调整该窗口所在容器A的分割比例和其祖父容器B(A的父容器)的分割比例即可。(Hyprland的行为)
    // 例如现在有如下容器树(H=horizontal, 横向分割; V=vertical, 纵向分割):
    //       root
    //        |
    //        H
    //       / \
    //      1   V
    //         / \
    //        2   3
    // 如果我们调整窗口2的大小, 则会调整V的纵向分割比例和H的横向分割比例; 调整3同理;
    // 如果我们调整窗口1的大小, 则会调整H的横向分割比例, 但由于其祖父容器为桌面, 就只能调整横向分割比例了。
    // 此外, 如果有更多层:
    //       root
    //        |
    //        H1
    //       / \
    //      1   V
    //         / \
    //        2   H2
    //           /  \
    //          3    4
    // 此时调整4的大小, 只会影响到H2和V的比例, 而不会影响H1的比例, 调整3同理。
    bool resized = false;
    LPointF mouseDelta = cursorPos - initialCursorPos;

    // 如果有水平目标，根据鼠标水平移动量调整
    if (resizingHorizontalTarget) {
        double totalWidth = resizingHorizontalTarget->geometry.w();
        if (totalWidth >= 1) {
            double ratioDelta = mouseDelta.x() / totalWidth;
            double newRatio = initialHorizontalRatio + ratioDelta;
            resizingHorizontalTarget->splitRatio = std::min(0.95, std::max(0.05, newRatio));
            resized = true;
        }
    }

    // 如果有垂直目标，根据鼠标垂直移动量调整
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
        LLog::debug("成功调整容器分割比例");
        return true;
    }
    return false;
}

void TileyWindowStateManager::printContainerHierachy(UInt32 workspace){

    if(workspace < 0 || workspace >= WORKSPACES){
        LLog::log("目标工作区超出范围: 工作区%d, 停止打印容器树", workspace);
        return;
    }

    LLog::log("***************打印容器树***************");
    auto current = workspaceRoots[workspace];
    _printContainerHierachy(current);
    LLog::log("***************************************");
}

void TileyWindowStateManager::_printContainerHierachy(Container* current){
    
    if(!current){
        return;
    }
    LLog::log("容器: %d, 类型: %s, parent: %d, child1: %d, child2: %d, 活动显示器: %d", 
                        current, 
                        current->window != nullptr ? "窗口" : "容器", 
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
    LLog::debug("重新布局");
    // 调试: 打印当前容器树
    //printContainerHierachy(workspace);

    UInt32 accumulateCount = 0;

    _reflow(workspaceRoots[workspace], region, accumulateCount);

    success = (accumulateCount == containerCount);

    LLog::debug("容器数量: %d", containerCount);
    LLog::debug("布局容器数量: %d", accumulateCount);
    if(!success){
        LLog::error("[reflow]: 重新布局失败, 可能是有容器被意外修改");
    }
}

bool TileyWindowStateManager::addWindow(ToplevelRole* window, Container* &container){

    if(!window){
        LLog::debug("尝试添加空指针窗口, 停止操作");
        return false;
    }

    Surface* surface = static_cast<Surface*>(window->surface());

    LLog::debug("添加窗口, surface位置: (%d,%d)", surface->pos().x(), surface->pos().y());
    for(LSurface* surf : surface->children()){
        LLog::debug("子surface位置: (%d,%d)", surf->pos().x(), surf->pos().y());
    }

    // 获取屏幕显示区域
    Output* activeOutput = ((Output*)cursor()->output());
    const LRect& availableGeometry = activeOutput->availableGeometry();

    if(!surface){
        LLog::debug("目标窗口没有Surface, 是否已经被销毁?");
        return false;
    }

    // 调试: 打印新建窗口的信息
    //surface->printWindowGeometryDebugInfo(activeOutput, availableGeometry);

    // 设置窗口属于当前活动显示器 TODO: 当后面允许静默在其他显示器创建窗口时修改
    window->output = activeOutput;

    switch (window->type) {
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::debug("显示了一个尺寸限制/瞬态窗口。surface地址: %d, 层次: %d", surface, surface->layer());
            reapplyWindowState(window);
            break;
        }
        case NORMAL:{
            LLog::debug("显示了一个普通窗口。surface地址: %d, 层次: %d", surface, surface->layer());
            Container* newContainer = new Container(window);
            insertTile(CURRENT_WORKSPACE, newContainer, 0.5);
            reapplyWindowState(window);
            container = newContainer;
            break;
        }
        default:
            LLog::warning("[addWindow]: 显示了一个未知类型的窗口。该窗口将不会被插入管理列表");
    }

    // 无论如何, 向窗口添加工作区信息
    window->workspaceId = CURRENT_WORKSPACE;

    return true;
}

// 移除窗口
bool TileyWindowStateManager::removeWindow(ToplevelRole* window, Container*& container){
    switch(window->type){
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::debug("关闭了一个尺寸限制/瞬态窗口。");
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
            LLog::warning("[removeWindow]: 移除窗口失败。该窗口不属于可管理的类型。");
    }

    return false;
}

// 改变平铺布局后调用, 配置该窗口的全局状态(显示层次、激活状态等等)
bool TileyWindowStateManager::reapplyWindowState(ToplevelRole* window){
    
    Surface* surface = static_cast<Surface*>(window->surface());

    if(!surface){
        LLog::warning("[reapplyWindowState]: 目标窗口没有Surface, 是否已经被销毁?");
        return false;
    }

    if(window->type == RESTRICTED_SIZE || window->type == FLOATING){
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }
    }else if(window->container->floating_reason == STACKING){
        LLog::debug("堆叠窗口: 暂时禁用containerView");
        window->container->enableContainerView(false);
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }
    }else if(window->container->floating_reason == MOVING){
        LLog::debug("移动窗口: 暂时禁用containerView");
        window->container->enableContainerView(false);
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }
    }else{
        LLog::debug("窗口进入平铺: 启用containerView");
        window->container->enableContainerView(true);
    }

    // 激活
    window->configureState(window->pendingConfiguration().state | LToplevelRole::Activated);
    return true;
}

// 切换一个窗口的平铺/堆叠状态
bool TileyWindowStateManager::toggleStackWindow(ToplevelRole* window){

    //1. 拿到toplevel的container
    //1.1 入参检查
    if(!window || !window->container){
        LLog::debug("切换状态: 切换目标窗口或其容器不能为空。停止操作");
        return false;
    }

    if(window->type != NORMAL){
        LLog::debug("切换状态: 切换的窗口不能是必须浮动的窗口, 如尺寸限制窗口。停止操作");
        return false;
    }

    if(window->container->floating_reason == MOVING){
        LLog::debug("切换状态: 暂时不支持移动中的窗口切换状态。停止操作");
        return false;
    }

    // 如果是平铺状态
    if(window->container->floating_reason == NONE){
        // 从管理器分离
        Container* detachedContainer = detachTile(window, STACKING);
        if(detachedContainer){
            // 如果分离成功, 重新组织并重新布局
            reapplyWindowState(window);
            recalculate();
        }
        return detachedContainer != nullptr;
    }else if(window->container->floating_reason == STACKING){
       // 如果是浮动状态
       // 插入管理器
        bool attached = attachTile(window);
        // TODO: 允许跨屏幕插入
        if(attached){
            // 如果插入成功, 重新组织并重新布局
            reapplyWindowState(window);
            recalculate();
        }
        return attached;
    }

    LLog::error("[toggleStackWindow]: 切换窗口浮动状态失败, 未知原因");
    return false;
}

// 获取某个工作区的第一个窗口
Container* TileyWindowStateManager::getFirstWindowContainer(UInt32 workspace){
    
    // 0. workspace范围必须合理
    if(workspace < 0 || workspace >= WORKSPACES){
        LLog::warning("[getFirstWindowContainer]: 传入的工作区无效, 返回空");
        return nullptr;
    }

    Container* root = workspaceRoots[workspace];
    Container* result = nullptr;

    // 1. 如果只有桌面, 返回空指针
    if(root && !root->child1 && !root->child2){
        LLog::debug("工作区没有窗口, 返回空");
        return nullptr;
    // 2. 如果只有一个窗口
    }else if(root && root->child1 && root->child1->window){
        return root->child1;
    // 3. 如果不止一个窗口
    }else if(root && root->child1 && !root->child1->window){
        // 递归
        result =  _getFirstWindowContainer(root);
    }

    if(result == nullptr){
        LLog::debug("无法获取工作区的第一个窗口, 未知原因, 返回空");
    }
    return result;
}

Container* TileyWindowStateManager::_getFirstWindowContainer(Container* container){
    
    // 中序查找
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
/*
// 重新计算布局。需要外部在合适的时候手动触发
bool TileyWindowStateManager::recalculate(){

     // TODO: 实现多工作区全部重排, 但考虑性能 vs 准确性
     // 目前就0号工作区
     UInt32 workspace = CURRENT_WORKSPACE;
     Container* root = workspaceRoots[workspace];

     // 如果工作区没有窗口
     if(!root->child1 && !root->child2){
        LLog::debug("工作区没有窗口, 无需重排平铺。");
        return false;
     }

     //printContainerHierachy(workspace);

     Output* rootOutput = static_cast<ToplevelRole*>((getFirstWindowContainer(workspace)->window))->output;

     // TODO: 更为健壮的机制
     if(!rootOutput){
         LLog::warning("[recalculate]: 重排的工作区: %d, 其第一个窗口节点没有对应的显示器信息, 放弃重排。", workspace);
         return false;
     }

     const LRect& availableGeometry = rootOutput->availableGeometry();

     bool reflowSuccess = false;
     LLog::debug("执行重新布局...");
     
     reflow(0, availableGeometry, reflowSuccess);
     if(reflowSuccess){
         LLog::debug("重新布局成功。");
         return true;
     }else{
         LLog::debug("重新布局失败, 可能有容器被意外修改。");
         return false;
    }

    LLog::debug("重新布局失败。未知原因。");
    return false;
}*/
bool TileyWindowStateManager::recalculate(){
    // 用当前工作区
    UInt32 workspace = CURRENT_WORKSPACE;
    Container* root = workspaceRoots[workspace];

    if (!root) {
        LLog::warning("[recalculate]: 工作区 %u 的根节点为空。", workspace);
        return false;
    }

    if (!root->child1 && !root->child2) {
        LLog::debug("工作区 %u 没有窗口, 无需重排平铺。", workspace);
        return false;
    }

    LLog::log("当前重新布局工作区: %d", workspace);

    // 选一个可用的输出：优先取该工作区第一个窗口的输出；没有就退回到鼠标所在输出
    Output* rootOutput = nullptr;
    if (auto* first = getFirstWindowContainer(workspace)) {
        rootOutput = static_cast<ToplevelRole*>(first->window)->output;
    }
    if (!rootOutput) {
        rootOutput = static_cast<Output*>(cursor()->output());
    }
    if (!rootOutput) {
        LLog::warning("[recalculate]: 工作区 %u 找不到有效输出，放弃重排。", workspace);
        return false;
    }

    const LRect& availableGeometry = rootOutput->availableGeometry();

    // 当前工作区的期望节点数
    containerCount = countContainersOfWorkspace(root);   

    bool reflowSuccess = false;
    LLog::debug("执行重新布局... ws=%u, nodes=%u", workspace, containerCount);

    reflow(workspace, availableGeometry, reflowSuccess);
    if (reflowSuccess){
        LLog::debug("重新布局成功。");
        return true;
    } else {
        LLog::debug("重新布局失败, 可能有容器被意外修改。");
        return false;
    }
}


UInt32 TileyWindowStateManager::countContainersOfWorkspace(const Container* root) {
    if (!root) return 0;

    UInt32 n = 0;
    std::function<void(const Container*)> dfs = [&](const Container* c){
        if (!c) return;
        // 和 _reflow 对齐：无论是窗口容器还是分割容器，节点本身都计数
        ++n;
        dfs(c->child1);
        dfs(c->child2);
    };

    dfs(root);
    return n;
}


void TileyWindowStateManager::_reflow(Container* container, const LRect& areaRemain, UInt32& accumulateCount){

    if (container == nullptr) { 
        return;
    }

    container->geometry = areaRemain;

    // 间隔大小
    const Int32 GAP = 5;

    // 1. 判断我是窗口(叶子)还是分割容器
    if(container->window){
        LLog::debug("我是窗口, 我获得的大小是: %dx%d, 位置是:(%d,%d)", areaRemain.w(), areaRemain.h(), areaRemain.x(), areaRemain.y());
        // 我是窗口
        
        // 2. 如果我是窗口, 获取areaRemain, 分别调整surface大小/位置
        
        Surface* surface = static_cast<Surface*>(container->window->surface());
        
        // TODO: 心跳检测逻辑问题
        /*
        ToplevelRole* toplevel = static_cast<ToplevelRole*>(container->window);
        if (toplevel->pendingConfiguration().serial != 0 &&
            toplevel->serial() != toplevel->pendingConfiguration().serial){
            LLog::debug("客户端仍在处理序列号 %u, 本次跳过配置", toplevel->pendingConfiguration().serial);
            return;
        }
        */
        
        if(surface->mapped()){
            // 窗口空隙实现
            const LRect& areaWithGaps = areaRemain;

            LRect areaForWindow = {
                areaWithGaps.x() + GAP,
                areaWithGaps.y() + GAP,
                areaWithGaps.w() - GAP * 2,
                areaWithGaps.h() - GAP * 2
            };

            // 如果窗口太小, 就不留空隙了, 避免负数宽高
            if (areaForWindow.w() < 50) areaForWindow.setW(50);
            if (areaForWindow.h() < 50) areaForWindow.setH(50);

            // 3. 显示部分调整为“内部区域”
            container->containerView->setPos(areaForWindow.pos());
            container->containerView->setSize(areaForWindow.size());

            // 3. 请求客户端也调整为“内部区域”
            surface->setPos(areaRemain.pos());
            container->window->configureSize(areaForWindow.size());
            container->window->setExtraGeometry({GAP, GAP, GAP, GAP});
            
            LLog::debug("containerView的children数量: %zu", container->containerView->children().size());
            SurfaceView* surfaceView = static_cast<SurfaceView*>(container->containerView->children().front());

            const LRect& windowGeometry = container->window->windowGeometry();

            // 如果窗口不支持服务端装饰, 并且windowGeometry有偏移(确实画了客户端装饰)
            if(!container->window->supportServerSideDecorations() && (windowGeometry.x() > 0 || windowGeometry.y() > 0)){
                // 则说明它大概率会无论如何都要画自己的装饰, 我们移动customPos, 将装饰部分移动到外边(不影响布局的位置)
                surfaceView->setCustomPos(-windowGeometry.x(), -windowGeometry.y());
            }

            compositor()->repaintAllOutputs();
        }

        accumulateCount += 1;
    // 3. 如果我是分割容器
    }else if(!container->window){

        LRect area1, area2;
        // 横向分割
        LLog::debug("我是分割容器, 我的分割是: %d, 比例是: %f, 尺寸是: %dx%d", container->splitType, container->splitRatio, container->geometry.width(), container->geometry.height());
        if(container->splitType == SPLIT_H){
            Int32 child1Width = (Int32)(areaRemain.width() * (container->splitRatio));
            Int32 child2Width = (Int32)(areaRemain.width() - child1Width);
            // TODO: 浮点数误差
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
    auto filter = [this](LSurface* s) -> bool{
        
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
    if (target >= WORKSPACES || target == CURRENT_WORKSPACE) {
        LLog::debug("switchWorkspace: 无效目标 %u 或与当前相同", target);
        return false;
    }

    // 隐藏所有属于旧工作区的窗口, 并显示新工作区的所有窗口
    for(auto surface : compositor()->surfaces()){
        if(surface->toplevel()){
            auto window = static_cast<ToplevelRole*>(surface->toplevel());
            bool shouldBeVisible = (window->workspaceId == target);
            // 一键切换可见性
            setWindowVisible(window, shouldBeVisible);
        }
    }

    //更新索引
    CURRENT_WORKSPACE = target;
    activeContainer = workspaceActiveContainers[CURRENT_WORKSPACE];

    auto seat = Louvre::seat();
    // 如果目标工作区活动窗口存在, 则直接切换焦点过去, 不用先清空再设置到新的哦
    if(activeContainer && activeContainer->window){
        if (seat->keyboard()) seat->keyboard()->setFocus(activeContainer->window->surface());
        if (seat->pointer())  seat->pointer()->setFocus(activeContainer->window->surface());
    }else{
        if (seat->keyboard()) seat->keyboard()->setFocus(nullptr);
        if (seat->pointer())  seat->pointer()->setFocus(nullptr);
    }

    // 重排与重绘
    // 只对当前区的布局执行一次 recalculate()，TODO:这里为什么会报错（重新布局失败）呢，有点不理解
    recalculate();
    // repaint 所有输出，重新绘制
    for (auto out : Louvre::compositor()->outputs())
        out->repaint();

    LLog::debug("切换到工作区 %u 完成", CURRENT_WORKSPACE);
    return true;
}

TileyWindowStateManager& TileyWindowStateManager::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyWindowStateManager());
    });
    return *INSTANCE;
}

std::unique_ptr<TileyWindowStateManager, TileyWindowStateManager::WindowStateManagerDeleter> TileyWindowStateManager::INSTANCE = nullptr;
std::once_flag TileyWindowStateManager::onceFlag;
/*
TileyWindowStateManager::TileyWindowStateManager(){
    for(int i = 0; i < WORKSPACES; i++){
        workspaceRoots[i] = new Container();
        workspaceRoots[i]->splitType = SPLIT_H;
        workspaceRoots[i]->splitRatio = 1.0; // 设置为1.0表示桌面。桌面只有一个孩子窗口时, 不分割, 全屏显示
    }
    // 添加了根节点
    containerCount += 1;
}*/
TileyWindowStateManager::TileyWindowStateManager()
  : workspaceRoots(WORKSPACES, nullptr)
{
    // 为每个工作区创建一个根容器，并初始化为“桌面”状态
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