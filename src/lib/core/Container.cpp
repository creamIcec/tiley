#include "Container.hpp"
#include "LLayerView.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/types.hpp"

#include <LSurfaceView.h>
#include <LLog.h>
#include <memory>

using namespace tiley;

// Constructor for splitting containers
Container::Container(){
    this->child1 = nullptr;
    this->child2 = nullptr;
    this->splitType = SPLIT_NONE;
}

// Constructor for windows
Container::Container(ToplevelRole* window) : Container::Container(){
    this->window = window;
    window->container = this;

    // set containerView to child of APPLICATION_LAYER
    containerView = std::make_unique<LLayerView>(&TileyServer::getInstance().layers()[APPLICATION_LAYER]);

    // align containerView and surfaceView first to avoid later problem
    if(window->surface() && window->surface()->mapped()){
        LLog::debug("Initialize containerView, align surface");
        LLog::debug("position of surface: (%d,%d), size: (%dx%d)", window->surface()->pos().x(), window->surface()->pos().y(), window->surface()->size().width(), window->surface()->size().height());
        containerView->setPos(window->surface()->pos());
        containerView->setSize(window->surface()->size());
        LLog::debug("containerView: nativePos: (%d,%d)", containerView->nativePos().x(), containerView->nativePos().y());
        LLog::debug("containerView: pos: (%d,%d)", containerView->pos().x(), containerView->pos().y());
    }
}

void Container::printContainerWindowInfo(){
    if(this->window){

        ToplevelRole* win = static_cast<ToplevelRole*>(this->window);

        LLog::log("window->type: %d", win->type);
        LLog::log("object address of window->container: %d", win->container);
        LLog::log("window->container->floating_reason: %d", win->container->floating_reason);
    }
}

// enable when container is tiled otherwise disable
void Container::enableContainerView(bool enable){

    if(!window){
        LLog::debug("[enableContainerView]: the container inside this view is not for window, stop action");
        return;
    }

    Surface* surface = static_cast<Surface*>(window->surface());

    if(!surface){
        LLog::warning("[enableContainerView]: The surface of window is null, is it destroyed? stop action");
        return;
    }

    if(!enable){

        disableContainerFeatures();
        surface->view->setParent(containerView->parent());
        containerView->setVisible(false);
        surface->view->enableCustomPos(false);

    }else if(enable){

        surface->view->setParent(containerView.get());
        containerView->setVisible(true);

        surface->view->enableCustomPos(true);
        //surface->view->setCustomPos(0,0);

        enableContainerFeatures();
    }

    surface->view->repaint();

}

// 2 functions for enable/disable container features should be modified in pair
void Container::enableContainerFeatures(){
    for(LView* child : containerView->children()){
        child->enableParentClipping(true);
        // new features going here
    }
}

void Container::disableContainerFeatures(){
    for(LView* child : containerView->children()){
        child->enableParentClipping(false);
        // new features going here
    }
}

Container::~Container(){}