#include "TileyWindowStateManager.hpp"
#include "LBitset.h"
#include "LEdge.h"
#include "LSurface.h"
#include "LToplevelMoveSession.h"
#include "LToplevelRole.h"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/Container.hpp"
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

using namespace tiley;
//找底层窗口
static Surface* findFirstParentToplevelSurface(Surface* surface){
    Surface* iterator = surface;
    while(iterator != nullptr && surface->toplevel() == nullptr){
        if(surface == nullptr){
            // TODO: 原因?
            LLog::log("无法找到一个surface的父窗口");
            return nullptr;
        }
        iterator = (Surface*)iterator->parent();
    }
    return iterator;
}

// insert(插入)函数
// 检查清单:
// 父容器是否指向了子容器?
// 子容器是否指向了父容器?
bool TileyWindowStateManager::insertTile(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE splitType, Float32 splitRatio){

    L_UNUSED(workspace);

    if(targetContainer == nullptr){
        LLog::log("目标窗口为空, 停止插入");
        return false;
    }

    if(newWindowContainer == nullptr){
        LLog::log("传入的窗口为空, 停止插入");
        return false;
    }

    if(newWindowContainer == targetContainer){
        LLog::log("容器不能作为自己的子节点, 停止插入");
        return false;
    }

    if(targetContainer->window == nullptr){
        LLog::log("目标位置必须是窗口, 停止插入");
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
        // TODO: 可能有意外嘛?
    }

    // 4. 挂载窗口
    // TODO: 根据鼠标位置不同决定1和2分别是谁
    splitContainer->child1 = targetContainer;
    splitContainer->child2 = newWindowContainer;
    targetContainer->parent = splitContainer;
    newWindowContainer->parent = splitContainer;

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
        LLog::log("只有桌面节点, 仅插入窗口");
        workspaceRoots[workspace]->child1 = newWindowContainer;
        newWindowContainer->parent = workspaceRoots[workspace];
        containerCount += 1;
        return true;
    }

    if(targetContainer){
        LLog::log("鼠标位置有窗口容器");
        Surface* windowSurface = static_cast<Surface*>(targetContainer->window->surface());
        SPLIT_TYPE split = windowSurface->size().w() >= windowSurface->size().h() ? SPLIT_H : SPLIT_V;
        return insertTile(workspace, newWindowContainer, ((ToplevelRole*)windowSurface->toplevel())->container, split, splitRatio);   
    }

    // 如果既不是桌面, 鼠标也没有聚焦到任何窗口, 尝试插入上一个被激活的container之后
    if(activeContainer != nullptr){
        LLog::log("插入到上一个激活的窗口后");
        const LRect& size = activeContainer->window->surface()->size();
        SPLIT_TYPE split = size.w() >= size.h() ? SPLIT_H : SPLIT_V; 
        return insertTile(workspace, newWindowContainer, activeContainer, split, splitRatio);
    }
    // TODO: 其他鼠标没有聚焦的情况

    LLog::log("插入失败, 未知情况。");
    return false;
}

Container* TileyWindowStateManager::detachTile(LToplevelRole* window, FLOATING_REASON reason){
    // 和removeTile逻辑接近, 就是不删除容器。
    // TODO: 考虑和remove合并, 通过一个参数区分是分离还是关闭

    if(window == nullptr){
        LLog::log("要分离的窗口为空, 停止分离");
        return nullptr;
    }

    // 2. 找到这个window对应的container
    Container* containerToDetach = ((ToplevelRole*)window)->container;
    // 同时保存父亲和祖父的指针
    Container* grandParent = containerToDetach->parent->parent;
    Container* parent = containerToDetach->parent;

    if(containerToDetach == nullptr){
        LLog::log("要分离的窗口没有container? 正常情况下不应该发生, 停止分离");
        return nullptr;
    }

    // TODO: 将下面的这行代码移动到一个更好的位置。视图和逻辑应该分开处理。
    window->surface()->raise();

    // 3. 处理只剩一个或两个窗口的情况
    if(grandParent == nullptr){
        Container* sibling = (parent->child1 == containerToDetach) ? parent->child2 : parent->child1;
        if(sibling == nullptr){
            // 3.1 这是最后一个窗口
            LLog::log("分离最后一个窗口");
            parent->child1 = nullptr;
            
        }else{
            // 3.2 屏幕上还剩一个窗口
            LLog::log("分离后仅剩一个窗口");
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

    // 窗口 -1, 分割容器 -1
    containerCount -= 2;

    return containerToDetach;
};

bool TileyWindowStateManager::attachTile(LToplevelRole* window){
    // 直接调用insertTile

    if(!window){
        LLog::log("要合并的窗口为空, 取消合并");
        return false;
    }

    ToplevelRole* windowToAttach = static_cast<ToplevelRole*>(window);

    if(!windowToAttach || !windowToAttach->container){
        LLog::log("要合并的窗口没有容器, 取消合并");
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
        LLog::log("要移除的窗口为空, 停止移除");
        return nullptr;
    }

    // 2. 找到这个window对应的container
    Container* containerToRemove = ((ToplevelRole*)window)->container;
    // 同时保存父亲和祖父的指针
    Container* grandParent = containerToRemove->parent->parent;
    Container* parent = containerToRemove->parent;

    if(containerToRemove == nullptr){
        LLog::log("要移除的窗口没有container? 正常情况下不应该发生, 停止移除");
        return nullptr;
    }

    // 3. 处理只剩一个或两个窗口的情况
    if(grandParent == nullptr){
        Container* sibling = (parent->child1 == containerToRemove) ? parent->child2 : parent->child1;
        if(sibling == nullptr){
            // 3.1 这是最后一个窗口
            LLog::log("关闭最后一个窗口");
            parent->child1 = nullptr;
            // TODO: 找到窗口所在的workspace
            result = workspaceRoots[0];
        }else{
            // 3.2 屏幕上还剩一个窗口
            LLog::log("关闭后仅剩一个窗口");
            // 直接把兄弟节点的内容(它本身应该是个窗口容器)移动到根容器
            parent->child1 = sibling;
            parent->child2 = nullptr;
            sibling->parent = parent; // 更新兄弟节点的父指针
            //分割容器 -1
            containerCount -= 1;
            result = sibling;
        }

        delete containerToRemove;

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
        LLog::log("成功调整容器分割比例");
        return true;
    }
    return false;
}



// // TODO: 移除workspace参数, 自动寻找窗口所在工作区
// bool TileyWindowStateManager::resizeTile(LToplevelRole* window, LBitset<LEdge> edges, LPointF cursorPos){ 


//     if (!window) return false;
//     ToplevelRole* targetWindow = static_cast<ToplevelRole*>(window);
//     Container* container = targetWindow->container;
//     if (!container || container->floating_reason != NONE || !container->parent || !container->parent->child2) {
//         LLog::log("调整大小失败：前置条件不满足。");
//         return false;
//     }

//     // --- 2. 核心逻辑：双目标向上追溯 ---
//     Container* horizontalTarget = nullptr;
//     Container* verticalTarget = nullptr;
//     Container* current = container;
    
//     bool isHorizontalDrag = edges.check(LEdgeLeft) || edges.check(LEdgeRight);
//     bool isVerticalDrag = edges.check(LEdgeTop) || edges.check(LEdgeBottom);

//     // 向上遍历，直到找到所有可能的目标或到达根节点
//     while (current->parent && current->parent != workspaceRoots[CURRENT_WORKSPACE])
//     {
//         Container* parent = current->parent;

//         // 检查水平方向，如果需要且尚未找到
//         if (isHorizontalDrag && !horizontalTarget && parent->splitType == SPLIT_H) {
//             horizontalTarget = parent;
//         }
        
//         // 检查垂直方向，如果需要且尚未找到
//         if (isVerticalDrag && !verticalTarget && parent->splitType == SPLIT_V) {
//             verticalTarget = parent;
//         }

//         // 如果两个目标都找到了，就可以提前结束循环
//         if ((!isHorizontalDrag || horizontalTarget) && (!isVerticalDrag || verticalTarget)) {
//             break;
//         }

//         current = parent;
//     }

//     // --- 3. 分别计算并应用两个方向的分割比例 ---
//     bool resized = false;

//     // 如果找到了水平目标，就调整它
//     if (horizontalTarget) {
//         double totalWidth = horizontalTarget->geometry.w();
//         if (totalWidth >= 1) {
//             double offset = cursorPos.x() - horizontalTarget->geometry.x() - grabPos.x();
//             double newRatio = offset / totalWidth;
//             horizontalTarget->splitRatio = std::min(0.95, std::max(0.05, newRatio));
//             resized = true;
//         }
//     } 
    
//     // 如果找到了垂直目标，也调整它 (注意：这里是 if，不是 else if)
//     if (verticalTarget) {
//         double totalHeight = verticalTarget->geometry.h();
//         if (totalHeight >= 1) {
//             double offset = cursorPos.y() - verticalTarget->geometry.y() - grabPos.y();
//             double newRatio = offset / totalHeight;
//             verticalTarget->splitRatio = std::min(0.95, std::max(0.05, newRatio));
//             resized = true;
//         }
//     }

//     if (resized) {
//         LLog::log("成功调整布局。H-Target: %p, V-Target: %p", horizontalTarget, verticalTarget);
//         return true;
//     }

//     LLog::log("调整大小失败：找不到任何匹配的父容器。");
//     return false;

// }

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
    LLog::log("重新布局");
    // 调试: 打印当前容器树
    //printContainerHierachy(workspace);

    UInt32 accumulateCount = 0;

    _reflow(workspaceRoots[workspace], region, accumulateCount);

    success = (accumulateCount == containerCount);

    LLog::log("容器数量: %d", containerCount);
    LLog::log("布局容器数量: %d", accumulateCount);
    if(!success){
        LLog::log("重新布局失败, 可能是有容器被意外修改");
    }
}

bool TileyWindowStateManager::addWindow(ToplevelRole* window, Container* &container){

    Surface* surface = static_cast<Surface*>(window->surface());

    // 获取屏幕显示区域
    Output* activeOutput = ((Output*)cursor()->output());
    const LRect& availableGeometry = activeOutput->availableGeometry();

    if(!surface){
        LLog::log("目标窗口没有Surface, 是否已经被销毁?");
        return false;
    }

    // 调试: 打印新建窗口的信息
    //surface->printWindowGeometryDebugInfo(activeOutput, availableGeometry);

    // 设置窗口属于当前活动显示器 TODO: 当后面允许静默在其他显示器创建窗口时修改
    window->output = activeOutput;

    switch (window->type) {
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::log("显示了一个尺寸限制/瞬态窗口。surface地址: %d, 层次: %d", surface, surface->layer());
            reapplyWindowState(window);
            break;
        }
        case NORMAL:{
            LLog::log("显示了一个普通窗口。surface地址: %d, 层次: %d", surface, surface->layer());
            Container* newContainer = new Container(window);
            insertTile(CURRENT_WORKSPACE, newContainer, 0.5);
            reapplyWindowState(window);
            container = newContainer;
            break;
        }
        default:
            LLog::log("警告: 显示了一个未知类型的窗口。该窗口将不会被插入管理列表");
    }

    return true;
}

// 移除窗口
bool TileyWindowStateManager::removeWindow(ToplevelRole* window, Container*& container){
    switch(window->type){
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::log("关闭了一个尺寸限制/瞬态窗口。");
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
            LLog::log("警告: 移除窗口失败。该窗口不属于可管理的类型。");
    }

    return false;
}

// 插入窗口时被调用, 配置该窗口的全局状态(显示层次、激活状态等等)
bool TileyWindowStateManager::reapplyWindowState(ToplevelRole* window){
    
    Surface* surface = static_cast<Surface*>(window->surface());

    if(!surface){
        LLog::log("目标窗口没有Surface, 是否已经被销毁?");
        return false;
    }

    if(window->type == RESTRICTED_SIZE || 
       window->type == FLOATING || 
       window->container->floating_reason == STACKING){
        
        if(surface->topmostParent() && surface->topmostParent()->toplevel()){
            surface->topmostParent()->raise();
        }else{
            surface->raise();
        }

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
        LLog::log("切换状态: 切换目标窗口或其容器不能为空。停止操作");
        return false;
    }

    if(window->type != NORMAL){
        LLog::log("切换状态: 切换的窗口不能是必须浮动的窗口, 如尺寸限制窗口。停止操作");
        return false;
    }

    if(window->container->floating_reason == MOVING){
        LLog::log("切换状态: 暂时不支持移动中的窗口切换状态。停止操作");
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

    LLog::log("切换状态: 切换窗口浮动状态失败, 未知原因");
    return false;
}

// 获取某个工作区的第一个窗口
Container* TileyWindowStateManager::getFirstWindowContainer(UInt32 workspace){
    
    // 0. workspace范围必须合理
    if(workspace < 0 || workspace >= WORKSPACES){
        LLog::log("传入的工作区无效, 返回空");
        return nullptr;
    }

    Container* root = workspaceRoots[workspace];
    Container* result = nullptr;

    // 1. 如果只有桌面, 返回空指针
    if(root && !root->child1 && !root->child2){
        LLog::log("工作区没有窗口, 返回空");
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
        LLog::log("无法获取工作区的第一个窗口, 未知原因, 返回空");
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

// 重新计算布局。需要外部在合适的时候手动触发
bool TileyWindowStateManager::recalculate(){

     // TODO: 实现多工作区全部重排, 但考虑性能 vs 准确性
     // 目前就0号工作区
     UInt32 workspace = 0;
     Container* root = workspaceRoots[workspace];

     // 如果工作区没有窗口
     if(!root->child1 && !root->child2){
        LLog::log("工作区没有窗口, 无需重排平铺。");
        return false;
     }

     //printContainerHierachy(workspace);

     Output* rootOutput = static_cast<ToplevelRole*>((getFirstWindowContainer(workspace)->window))->output;

     // TODO: 更为健壮的机制
     if(!rootOutput){
         LLog::log("警告: 重排的工作区: %d, 其第一个窗口节点没有对应的显示器信息, 放弃重排。", workspace);
         return false;
     }

     const LRect& availableGeometry = rootOutput->availableGeometry();

     bool reflowSuccess = false;
     LLog::log("执行重新布局...");
     
     reflow(0, availableGeometry, reflowSuccess);
     if(reflowSuccess){
         LLog::log("重新布局成功。");
         return true;
     }else{
         LLog::log("重新布局失败, 可能有容器被意外修改。");
         return false;
    }

    LLog::log("重新布局失败。未知原因。");
    return false;
}

void TileyWindowStateManager::_reflow(Container* container, const LRect& areaRemain, UInt32& accumulateCount){

    if (container == nullptr) { 
        return;
    }

    container->geometry = areaRemain;

    // 1. 判断我是窗口(叶子)还是分割容器
    if(container->window){
        LLog::log("我是窗口, 我获得的大小是: %dx%d, 位置是:(%d,%d)", areaRemain.w(), areaRemain.h(), areaRemain.x(), areaRemain.y());
        // 我是窗口
        // TODO: 心跳检测
        // 2. 如果我是窗口, 获取areaRemain, 分别调整surface大小/位置
        Surface* surface = static_cast<Surface*>(container->window->surface());
        if(surface->mapped()){
            surface->setPos(areaRemain.x(), areaRemain.y());
            container->window->configureSize(areaRemain.w(), areaRemain.h());
            compositor()->repaintAllOutputs();
        }

        accumulateCount += 1;
    // 3. 如果我是分割容器
    }else if(!container->window){
        LRect area1, area2;
        // 横向分割
        LLog::log("我是分割容器, 我的分割是: %d, 比例是: %f, 尺寸是: %dx%d", container->splitType, container->splitRatio, container->geometry.width(), container->geometry.height());
        if(container->splitType == SPLIT_H){
            Int32 child1Width = (Int32)(areaRemain.width() * (container->splitRatio));
            Int32 child2Width = (Int32)(areaRemain.width() * (1 - container->splitRatio));
            // TODO: 浮点数误差
            area1 = {areaRemain.x(),areaRemain.y(),child1Width,areaRemain.height()};
            area2 = {areaRemain.x() + child1Width, areaRemain.y(), child2Width, areaRemain.height()};
        }else if(container->splitType == SPLIT_V){
            Int32 child1Height = (Int32)(areaRemain.height() * (container->splitRatio));
            Int32 child2Height = (Int32)(areaRemain.height() * (1 - container->splitRatio));
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

    // 一阶段
    if(activeContainer){
        return activeContainer;
    }

    // 二阶段
    Container* root = workspaceRoots[workspace];

    if(!root){
        LLog::log("工作区没有节点, 可能是bug, 停止获取鼠标处的容器");
        return nullptr;
    }

    // 如果工作区为空, 直接返回自己
    if(root->child1 == nullptr && root->child2 == nullptr){
        return root;
    }

    // 这里不需要限制工作区。所有surface的集合>单个工作区的集合, 如果所有surface都找不到, 则说明肯定桌面是空的
    // surface()只包含客户端创建的
    for(LSurface* surface : compositor()->surfaces()){

        if(surface->toplevel()){

            Surface* targetSurface = static_cast<Surface*>(surface);

            // 排除非平铺窗口
            if(targetSurface->tl()->container->floating_reason != NONE){
                continue;
            }

            // 排除正在被移动的窗口, TODO: 和上面的条件合并(确保正在移动的窗口==floating_reason不为NONE的窗口)
            bool flag = true;

            for(LToplevelMoveSession* session : seat()->toplevelMoveSessions()){
                // TODO: 潜在风险: 可能在移动多个窗口吗? 先假设同时最多只有一个窗口能被移动
                if(targetSurface->toplevel() == session->toplevel()){ 
                    flag = false;
                    break;
                }
            }

            if(!flag){
                // 目标位置不是窗口或者是非平铺窗口, 都不符合条件, 下一位
                continue;
            }

            // 计算该surface的范围
            const LRect rect = {
                targetSurface->pos().x(),
                targetSurface->pos().y(),
                targetSurface->pos().x() + targetSurface->size().width(),
                targetSurface->pos().y() + targetSurface->size().height()
            };

            if(rect.containsPoint(cursor()->pos())){
                // 找到了目标窗口
                ToplevelRole* targetWindow = static_cast<ToplevelRole*>(targetSurface->toplevel());
                // 返回容器
                return targetWindow->container;
            }
        }
    }

    // 如果遍历完了都没找到, 说明是桌面
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
    if(window->container && window->container->floating_reason != NONE){
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


TileyWindowStateManager& TileyWindowStateManager::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyWindowStateManager());
    });
    return *INSTANCE;
}

std::unique_ptr<TileyWindowStateManager, TileyWindowStateManager::WindowStateManagerDeleter> TileyWindowStateManager::INSTANCE = nullptr;
std::once_flag TileyWindowStateManager::onceFlag;

TileyWindowStateManager::TileyWindowStateManager(){
    for(int i = 0; i < WORKSPACES; i++){
        workspaceRoots[i] = new Container();
        workspaceRoots[i]->splitType = SPLIT_H;
        workspaceRoots[i]->splitRatio = 1.0; // 设置为1.0表示桌面。桌面只有一个孩子窗口时, 不分割, 全屏显示
    }
    // 添加了根节点
    containerCount += 1;
}
TileyWindowStateManager::~TileyWindowStateManager(){}