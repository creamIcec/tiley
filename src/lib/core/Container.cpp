#include "Container.hpp"
#include "LLayerView.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/types.hpp"

#include <LSurfaceView.h>
#include <LLog.h>
#include <memory>

using namespace tiley;

// 创建的是分割容器
Container::Container(){
    // 默认设置
    this->child1 = nullptr;
    this->child2 = nullptr;
    this->splitType = SPLIT_NONE;
}

// 创建的是窗口
Container::Container(ToplevelRole* window) : Container::Container(){
    this->window = window;
    window->container = this;

    // 设置csrc/lib/core/Container.hppontainerView的父级为应用层
    containerView = std::make_unique<LLayerView>(&TileyServer::getInstance().layers()[APPLICATION_LAYER]);

    // 先对齐一遍containerView和surfaceView, 避免初始化问题
    if(window->surface() && window->surface()->mapped()){
        LLog::log("初始化容器View, 对齐surface");
        LLog::log("surface位置: (%d,%d), 大小: (%dx%d)", window->surface()->pos().x(), window->surface()->pos().y(), window->surface()->size().width(), window->surface()->size().height());
        containerView->setPos(window->surface()->pos());
        containerView->setSize(window->surface()->size());
        LLog::log("containerView: nativePos: (%d,%d)", containerView->nativePos().x(), containerView->nativePos().y());
        LLog::log("containerView: pos: (%d,%d)", containerView->pos().x(), containerView->pos().y());
    }
}

// 该函数应该在操作平铺层之前调用。
void Container::enableContainerView(bool enable){
    // 1. 获取window的父级情况, 如果是containerView, 则设置成containerView的parent(更上面一层, 比如APPLICATION_LAYER); 否则设置成containerView
    // 2. 同样根据上面的情况, 如果window父级是containerView, 则显示containerView; 否则隐藏

    if(!window){
        LLog::log("该容器不是窗口容器, 无法切换包装状态, 停止操作。");
        return;
    }

    Surface* surface = static_cast<Surface*>(window->surface());

    if(!surface){
        LLog::log("窗口的surface不存在, 是否已被销毁? 无法切换包装状态, 停止操作。");
        return;
    }

    // 如果是surfaceView的父级是containerView
    if(!enable){
        disableContainerFeatures();
        // 禁用
        surface->view->setParent(containerView->parent());
        containerView->setVisible(false);
        // 关闭自定义位置, 恢复默认逻辑, surface控制位置
        surface->view->enableCustomPos(false);

    }else if(enable){
        // 否则启用
        surface->view->setParent(containerView.get());
        containerView->setVisible(true);
        // 启用自定义位置, 全权交给我们的containerView控制位置
        surface->view->enableCustomPos(true);
        //surface->view->setCustomPos(0,0);

        enableContainerFeatures();
    }

    // 立即触发重绘
    surface->view->repaint();

}

void Container::enableContainerFeatures(){
    for(LView* child : containerView->children()){
        // 启用父级裁剪
        child->enableParentClipping(true);
        // 未来有什么新的特性就在这里写...
        // 别忘了同步disable. 之所以没有用一个bool值控制, 是因为有些特性可能不是一个简单的true/false就能控制的, 可能需要更多的状态修改代码, 因此分成了两个函数
    }
}

void Container::disableContainerFeatures(){
    for(LView* child : containerView->children()){
        // 关闭父级裁剪
        child->enableParentClipping(false);
    }
}

Container::~Container(){}