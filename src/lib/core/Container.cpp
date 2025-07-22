#include "Container.hpp"
#include "LLayerView.h"
#include "src/lib/types.hpp"

#include <LSurfaceView.h>
#include <LLog.h>
#include <memory>

using namespace tiley;

Container::Container(){
    con = std::make_unique<LLayerView>();
    // 默认设置
    this->child1 = nullptr;
    this->child2 = nullptr;
    this->split_type = SPLIT_NONE;
}

Container::~Container(){
    con.reset();
}