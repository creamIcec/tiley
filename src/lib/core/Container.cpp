#include "Container.hpp"
#include "src/lib/types.hpp"

#include <LSurfaceView.h>
#include <LLog.h>
using namespace tiley;

Container::Container(){
    // 默认设置
    this->child1 = nullptr;
    this->child2 = nullptr;
    this->splitType = SPLIT_NONE;
}

Container::Container(ToplevelRole* window) : Container::Container(){
    this->window = window;
    window->container = this;
}


Container::~Container(){}