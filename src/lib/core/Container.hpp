#pragma once

#include <LNamespaces.h>
#include <LToplevelRole.h>
#include <LLayerView.h>

#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/types.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/client/ToplevelRole.hpp"

using namespace Louvre;

namespace tiley{
    class Surface;
    class TileyWindowStateManager;
    class ToplevelRole;
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
            // 动态平铺: 分割类型
            SPLIT_TYPE splitType;
            // 动态平铺: 浮动原因(用户要移动窗口/用户请求堆叠)
            FLOATING_REASON floating_reason = NONE;
            
            // TODO: LWeak or unique_ptr
            LToplevelRole* window = nullptr;  //双向指针其二: container->toplevel 当是叶子节点时, 指向一个窗口 

            Container* parent = nullptr;
            Container* child1;  //这里我们不以上下左右命名, 只用编号, 避免混淆
            Container* child2;
            
            //范围:[0,1], 表示child1占区域大小的比例。默认0.5, 即对半分。对于窗口而言, splitRatio无效
            Float32 splitRatio = 0.5;

            // 保存这个容器的尺寸和位置信息。在重新布局和调整大小时更新
            LRect geometry;

            // 管理器可以访问私有成员
            friend TileyWindowStateManager;

        public:
            //构造函数: 传入window作为子节点, 传入空指针没有意义(no-op), 会弹出警告并直接销毁这个Container  
            Container(ToplevelRole* window);
            //构造函数: 不传入window, 说明是一个分割容器
            Container();
            ~Container();
    };
}