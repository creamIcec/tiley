#include "TileyWindowStateManager.hpp"
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

#include <cassert>
#include <memory>

using namespace tiley;

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
    // 找到鼠标现在聚焦的surface
    Surface* surface = static_cast<Surface*>(seat()->pointer()->focus());
    if(surface){
        LLog::log("鼠标位置有Surface");
        // 找到这个surface的顶层窗口
        Surface* windowSurface = findFirstParentToplevelSurface(surface);
        if(windowSurface && windowSurface->toplevel()){
            LLog::log("鼠标位置有toplevel");
            SPLIT_TYPE split = windowSurface->size().w() >= windowSurface->size().h() ? SPLIT_H : SPLIT_V;
            return insertTile(workspace, newWindowContainer, ((ToplevelRole*)windowSurface->toplevel())->container, split, splitRatio);   
        }else{
            LLog::log("鼠标位置没有窗口, 无法插入。");
            return false;
        }
    }else{
        // 可能是桌面(没有任何窗口)
        if(workspaceRoots[workspace]->child1 == nullptr && workspaceRoots[workspace]->child2 == nullptr){
            // 实锤
            // 只有桌面的时候, 直接插入到桌面节点
            LLog::log("只有桌面节点, 仅插入窗口");
            workspaceRoots[workspace]->child1 = newWindowContainer;
            newWindowContainer->parent = workspaceRoots[workspace];

            containerCount += 1;
            return true;
        }
        // 如果既不是桌面, 鼠标也没有聚焦到任何窗口, 尝试插入上一个被激活的container之后
        if(activeContainer != nullptr){
            const LRect& size = activeContainer->window->surface()->size();
            SPLIT_TYPE split = size.w() >= size.h() ? SPLIT_H : SPLIT_V; 
            return insertTile(workspace, newWindowContainer, activeContainer, split, splitRatio);
        }
        // TODO: 其他鼠标没有聚焦的情况
    }

    LLog::log("插入失败, 未知情况。");
    return false;
}

// 如何移除(remove)?
// 1. 由于关闭可以连带进行, 我们必须传入目标窗口。只使用鼠标位置是不可靠
Container* TileyWindowStateManager::removeTile(LToplevelRole* window){

    Container* result = nullptr;

    if(window == nullptr){
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

void TileyWindowStateManager::printContainerHierachy(UInt32 workspace){
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
    printContainerHierachy(workspace);

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
    surface->printWindowGeometryDebugInfo(activeOutput, availableGeometry);

    // 设置窗口属于当前活动显示器 TODO: 当后面允许静默在其他显示器创建窗口时修改
    window->output = activeOutput;

    switch (window->type) {
        case FLOATING:
        case RESTRICTED_SIZE: {
            LLog::log("创建了一个尺寸限制/瞬态窗口。surface地址: %d, 层次: %d", surface, surface->layer());
            rearrangeWindowLayer(window);
            break;
        }
        case NORMAL:{
            LLog::log("创建了一个普通窗口。surface地址: %d, 层次: %d", surface, surface->layer());
            Container* newContainer = new Container(window);
            insertTile(0, newContainer, 0.5);
            rearrangeWindowLayer(window);
            container = newContainer;
            break;
        }
        default:
            LLog::log("警告: 创建了一个未知类型的窗口。该窗口将不会被插入管理列表");
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

// 插入窗口时被调用, 将一个窗口放到该放的"层次"
bool TileyWindowStateManager::rearrangeWindowLayer(ToplevelRole* window){
    
    Surface* surface = static_cast<Surface*>(window->surface());

    if(!surface){
        LLog::log("目标窗口没有Surface, 是否已经被销毁?");
        return false;
    }

    if(window->type == RESTRICTED_SIZE || window->type == FLOATING){
        surface->raise();
    }

    return true;
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

     printContainerHierachy(workspace);

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
        }

        accumulateCount += 1;
    // 3. 如果我是分割容器
    }else if(!container->window){
        LRect area1, area2;
        // 横向分割
        LLog::log("我是分割容器, 我的分割是: %d, 比例是: %f", container->splitType, container->splitRatio);
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