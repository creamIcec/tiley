#include "TileyWindowStateManager.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/Container.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

#include <LCursor.h>
#include <LSeat.h>
#include <LPointer.h>
#include <LLog.h>
#include <LNamespaces.h>

#include <cassert>
#include <memory>

using namespace tiley;

static Surface* findFirstParentToplevelSurface(Surface* surface){
    Surface* iterator = surface;
    while(surface->toplevel() == nullptr){
        if(surface == nullptr){
            // TODO: 原因?
            LLog::log("无法找到一个surface的父窗口");
            return nullptr;
        }
        iterator = (Surface*)surface->parent();
    }
    return iterator;
}

// insert(插入)函数
// 检查清单:
// 父容器是否指向了子容器?
// 子容器是否指向了父容器?
bool TileyWindowStateManager::insert(UInt32 workspace, Container* newWindowContainer, Container* targetContainer, SPLIT_TYPE splitType, Float32 splitRatio){

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

    return true;
}


// insert(插入)函数
bool TileyWindowStateManager::insert(UInt32 workspace, Container* newWindowContainer, SPLIT_TYPE split, Float32 splitRatio){
    // 找到鼠标现在聚焦的surface
    Surface* surface = static_cast<Surface*>(seat()->pointer()->focus());
    if(surface){
        LLog::log("鼠标位置有Surface");
        // 找到这个surface的顶层窗口
        Surface* windowSurface = findFirstParentToplevelSurface(surface);
        if(windowSurface && windowSurface->toplevel()){
            LLog::log("鼠标位置有toplevel");
            return insert(workspace, newWindowContainer, ((ToplevelRole*)windowSurface->toplevel())->container, split, splitRatio);   
        }else{
            LLog::log("鼠标位置没有窗口, 无法插入。");
            return false;
        }
    }else{
        // 可能是只有桌面
        if(workspaceRoots[workspace]->child1 == nullptr){
            // 实锤
            // 只有桌面的时候, 直接插入到桌面节点
            LLog::log("只有桌面节点, 直接插入");
            workspaceRoots[workspace]->child1 = newWindowContainer;
            newWindowContainer->parent = workspaceRoots[workspace];
            return true;
        }
        // TODO: 其他鼠标没有聚焦的情况
    }

    LLog::log("插入失败, 未知情况。");
    return false;
}

// 如何移除(remove)?
// 1. 由于关闭可以连带进行, 我们必须传入目标窗口。只使用鼠标位置是不可靠
// 2. 找到这个window的surface对应的container
// 3. 将这个window的Container移除, 并将包装这个window和他兄弟window的父Container移除, 直接将他的兄弟上移到祖父Container的child 
// 1.
bool TileyWindowStateManager::remove(LToplevelRole* window){
    // 2.
    Surface* surface = (Surface*)window->surface();
    Container* container = ((ToplevelRole*)window)->container;
    // 3.
    // 3.1 保存兄弟container
    Container* sibling = nullptr;
    if(container == container->parent->child1){
        sibling = container->parent->child2;
    }else if(container == container->parent->child2){
        sibling = container->parent->child1;
    }
    // 3.3 上移
    Container* grandParent = container->parent;
    
    if(container->parent == grandParent->child1){
        grandParent->child1 = sibling;
    }else if(container == container->parent->child2){
        grandParent->child2 = sibling;
    }
    
    delete container->parent;
    delete container;
    return true;
}

// 仍然是动态平铺, 但Louvre的View特性使得我们不需要一棵单独的数据树来布局
// 使用LView的相对坐标系统即可。
void TileyWindowStateManager::reflow(UInt32 workspace, const LRect& region){
    LLog::log("重新布局");
    _reflow(workspaceRoots[workspace], region);
}

void TileyWindowStateManager::_reflow(Container* container, const LRect& areaRemain){

    if (!container) {        
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
    // 3. 如果我是分割容器
    }else if(!container->window){
        LRect area1, area2;
        // 横向分割
        LLog::log("我是分割容器, 我的分割是: %d, 比例是: %f", container->splitType, container->splitRatio);
        if(container->splitType == SPLIT_H){
            Int32 child1Width = (Int32)(areaRemain.width() * (container->splitRatio));
            Int32 child2Width = (Int32)(areaRemain.width() * (1 - container->splitRatio));
            // TODO: 浮点数误差
            area1 = {0,0,child1Width,areaRemain.height()};
            area2 = {child1Width, 0, child2Width, areaRemain.height()};
        }else if(container->splitType == SPLIT_V){
            Int32 child1Height = (Int32)(areaRemain.height() * (container->splitRatio));
            Int32 child2Height = (Int32)(areaRemain.height() * (1 - container->splitRatio));
            area1 = {0,0, areaRemain.width(), child1Height};
            area2 = {0,child1Height,areaRemain.width(), child2Height};
        }

        _reflow(container->child1, area1);
        _reflow(container->child2, area2);
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
    TileyServer& server = TileyServer::getInstance();
    for(int i = 0; i < WORKSPACES; i++){
        workspaceRoots[i] = new Container();
        workspaceRoots[i]->splitType = SPLIT_H;
        workspaceRoots[i]->splitRatio = 1.0; // 设置为1.0表示桌面, 只有一个孩子时。不分割, 全屏显示
    }
}
TileyWindowStateManager::~TileyWindowStateManager(){}