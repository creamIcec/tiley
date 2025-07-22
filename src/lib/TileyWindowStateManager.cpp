#include "TileyWindowStateManager.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/core/Container.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

#include <LCursor.h>
#include <LSeat.h>
#include <LPointer.h>
#include <LLog.h>
#include <LNamespaces.h>

#include <memory>

using namespace tiley;

// 如何插入(insert)?
// 1. 找到鼠标所在位置的, 是toplevel的surface
// 2. 找到这个surface对应的container
// 3. 计算分割方式
// 4. 插入

// 检查清单:
// 父容器是否指向了子容器?
// 子容器是否指向了父容器?
// 子容器的LayerView是否setParent到父容器的LayerView?

bool TileyWindowStateManager::insert(UInt32 workspace, LToplevelRole* window){

    if(!window){
        LLog::log("传入的窗口为空, 停止插入");
        return false;
    }

    auto windowSurface = static_cast<Surface*>(window->surface());
    
    // 为窗口分配container
    auto newContainer = new Container();
    newContainer->window = windowSurface;
    windowSurface->container = newContainer;

    // 不管因为何种原因后面找不到, 先排除是因为只有桌面的情况
    // 只有桌面的时候, 直接插入

    if(workspaceRoots[workspace]->child1 == nullptr){
        workspaceRoots[workspace]->child1 = newContainer;
        newContainer->parentContainer = workspaceRoots[workspace];

        newContainer->con->setParent(workspaceRoots[workspace]->con.get());

        return true;
    }


    // 不是桌面, 则进一步处理, 找到鼠标位置
    // 1.
    LSurface* surface = seat()->pointer()->surfaceAt(cursor()->pos());

    if(!surface){
        LLog::log("警告: 鼠标位置没有窗口。插入到最近一个打开的窗口之后");
        //TODO
        delete newContainer;
        return false;
    }
    
    // 如果找到的surface不是toplevel, 则需要回溯到最顶层(是toplevel吗?)
    if(!surface->toplevel()){
        surface = surface->topmostParent();            
    }

    if(!surface || !surface->toplevel()){
        // 到这里说明这是一种非常神奇的surface, 我们无法处理
        LLog::log("警告: 插入时出现错误, 没有找到鼠标位置存在的窗口。渲染的对象是什么?");
        delete newContainer;
        return false;
    }

    // 2. 是窗口了!
    auto targetSurface = static_cast<Surface*>(surface);

    if (targetSurface == windowSurface) {
        LLog::warning("目标窗口和要插入的窗口是同一个，这可能是一个竞态条件。已忽略本次插入。");
        // TODO: 这里应该有一个更优雅的回退策略，比如插入到根节点或者最后一个窗口
        delete newContainer; // 不要忘记释放内存
        return false;
    }

    auto targetContainer = targetSurface->container;
    
    // 3. 计算分割: 老办法, 比较宽高
    SPLIT_TYPE split = surface->size().w() >= surface->size().h() ? SPLIT_H : SPLIT_V;
    // 4.
    // 4.1. 创建一个新的Container, 用来装这两个窗口
    auto wrapContainer = new Container();
    newContainer->window = windowSurface;
    
    // 4.2 挂载到父节点上
    wrapContainer->split_type = split;
    // TODO: 根据位置不同选择在哪个部分插入(例如: 上下分割, 鼠标在下面, 则成为child2, 鼠标在上面, 则成为child1)
    auto oldParentContainer = targetContainer->parentContainer;

    wrapContainer->child1 = targetContainer;
    wrapContainer->child2 = newContainer;

    // 4.3 
    targetContainer->parentContainer = wrapContainer;
    newContainer->parentContainer = wrapContainer;

    targetContainer->con->setParent(wrapContainer->con.get());
    newContainer->con->setParent(wrapContainer->con.get());

    // 将新的wrapContainer作为原来window的parent的新子节点
    
    if(targetSurface->container == oldParentContainer->child1){
        oldParentContainer->child1 = wrapContainer;
    }else if(targetSurface->container == oldParentContainer->child2){
        oldParentContainer->child2 = wrapContainer;
    }

    wrapContainer->con->setParent(oldParentContainer->con.get());
    wrapContainer->parentContainer = oldParentContainer;

    return true;
}

// 如何移除(remove)?
// 1. 由于关闭可以连带进行, 我们必须传入目标窗口。只使用鼠标位置是不可靠
// 2. 找到这个window的surface对应的container
// 3. 将这个window的Container移除, 并将包装这个window和他兄弟window的父Container移除, 直接将他的兄弟上移到祖父Container的child 
// 1.
bool TileyWindowStateManager::remove(LToplevelRole* window){
    // 2.
    Surface* surface = (Surface*)window->surface();
    Container* container = surface->container;
    // 3.
    // 3.1 保存兄弟container
    Container* sibling = nullptr;
    if(container == container->parentContainer->child1){
        sibling = container->parentContainer->child2;
    }else if(container == container->parentContainer->child2){
        sibling = container->parentContainer->child1;
    }
    // 3.3 上移
    Container* grandParent = container->parentContainer;
    
    if(container->parentContainer == grandParent->child1){
        grandParent->child1 = sibling;
    }else if(container == container->parentContainer->child2){
        grandParent->child2 = sibling;
    }
    
    delete container->parentContainer;
    delete container;
    return true;
}

// 仍然是动态平铺, 但Louvre的View特性使得我们不需要一棵单独的数据树来布局
// 使用LView的相对坐标系统即可。
void TileyWindowStateManager::reflow(UInt32 workspace, LRect& region){

    L_UNUSED(workspace);  //TODO: 暂时不使用工作区, 默认鼠标所在显示器, 0号工作区
    LLog::log("重新布局");
    _reflow(workspaceRoots[workspace], region);
}

void TileyWindowStateManager::_reflow(Container* container, LRect& areaRemain){

    // 安全检查，防止对空指针进行操作
    if (!container) {
        return;
    }

    container->con->setPos(areaRemain.pos());
    container->con->setSize(areaRemain.size());

    // 1. 判断我是窗口(叶子)还是容器
    if(container->window){
        // 我是窗口
        // TODO: 心跳检测
        // 2. 如果我是窗口, 将我的窗口的父View设置成我的LayerView
        if(container->window->mapped()){
            container->window->getView()->setParent(container->con.get());
            container->window->setPos(0,0);
            if(container->window->toplevel()){
                container->window->toplevel()->configureSize(areaRemain.size());
            }
        }
    
    }else if(container->child1 && container->child2){
        //情况A: 有多于1个窗口
        LRect area1, area2;
        // 横向分割
        if(container->split_type == SPLIT_H){
            area1 = {0,0,areaRemain.width() / 2,areaRemain.height()};
            area2 = {areaRemain.width() / 2,0,areaRemain.width() / 2, areaRemain.height()};
        }else if(container->split_type == SPLIT_V){
            area1 = {0,0, areaRemain.width(), areaRemain.height() / 2};
            area2 = {0,areaRemain.height() / 2,areaRemain.width(), areaRemain.height() / 2};
        }

        _reflow(container->child1, area1);
        _reflow(container->child2, area2);
    }else if(container->child1){
        //情况B: 我只有一个child1: 说明是桌面上唯一窗口
        _reflow(container->child1, areaRemain);
    }else{
        //情况C: 没有child: 说明只有桌面
        return;
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
        workspaceRoots[i]->con->setParent(&server.layers()[TILED_LAYER]);
    }
}
TileyWindowStateManager::~TileyWindowStateManager(){}