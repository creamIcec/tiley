#pragma once

#include "LNamespaces.h"
#include "LSurfaceView.h"
#include "src/lib/types.hpp"
#include "src/lib/surface/Surface.hpp"
#include <LLayerView.h>
#include <memory>

using namespace Louvre;

namespace tiley{
    class Surface;
}

namespace tiley{
    // 容器类。代替我们在wlroots版本中实现的area_container.
    // 该容器类包含一个LLayerView, 不要被名字迷惑哦。LLayerView非常适合作为"容器", 因为他自身不会被渲染, 并且用来装别的SurfaceView.
    // 虽然我们用他作为"层次", 不过也可以用在这里。

    // 使用Louvre提供的View是为了方便框架计算坐标。这可以避免我们手动计算绝对坐标
    // 保证位置的相对性(子surface一定会跟着父surface相对移动)
    // position: relative;)

    // 一个Container可以代表一个叶子节点(真正的窗口)
    // 也可以代表一个容器(用于分割的)
    class Container{
        private:
        public:
            // 容器
            std::unique_ptr<LLayerView> con;
            // 动态平铺: 分割类型
            SPLIT_TYPE split_type;
            // 动态平铺: 浮动原因(用户要移动窗口/用户请求堆叠)
            FLOATING_REASON floating_reason = NONE;
            
            Container* parentContainer = nullptr;

            // TODO: LWeak or unique_ptr
            Surface* window = nullptr;  //双向指针其二: NodeContainer->Surface 当是叶子节点时, 指向一个窗口 

            Container* child1;  //这里我们不以上下左右命名, 只用编号, 避免混淆
            Container* child2;
            
            Float32 splitRatio = 0.5;  //默认对半分
            Container();
            ~Container();
    };
}