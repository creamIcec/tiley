#pragma once

#include <LNamespaces.h>
#include <LToplevelRole.h>
#include <LLayerView.h>
#include <memory>

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

    // 一个Container代表一个参与平铺算法管理的叶子节点(真正的窗口)或一个容器(用于分割的)
    class Container{
        public:
            inline const LRect& getGeometry(){return geometry;}
            inline LLayerView* getContainerView(){ return containerView.get(); }
            //构造函数: 传入window作为子节点, 传入空指针没有意义(no-op), 会弹出警告并直接销毁这个Container  
            Container(ToplevelRole* window);
            //构造函数: 不传入window, 说明是一个分割容器
            Container();
            ~Container();
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
            
            // 容器包装视图, 用于包装平铺层的窗口的SurfaceView, 便于当一个窗口在平铺层时添加特效。(例如裁剪窗口, 启用圆角等等, 设置内边距等等)
            // 当一个窗口被用户请求堆叠时, 不删除这个containerView, 而是将对应窗口的surfaceView的parent绕过这层直接设置到上层, 并隐藏containerView的显示
            // 当从堆叠状态切换回平铺状态时, 重新设置surfaceView的parent为这个containerView, 并显示containerView即可。

            // 当是可被平铺的窗口(window.type == NORMAL)时, 该containerView一直存在; 当是容器时, 该containerView = nullptr.

            // 使用unique_ptr, 伴随Container同生同死
            std::unique_ptr<LLayerView> containerView = nullptr;

            // 仅供自己和朋友调用, 实现上述的containerView的状态切换
            void enableContainerView(bool enable);

            // toggle时自动调用, 启用当一个窗口处于平铺层时的特性(例如强制大小裁剪), 使用单独的函数是为了方便管理
            void enableContainerFeatures();
            // 同理, 不处于平铺层时关闭特性
            void disableContainerFeatures();

            // 管理器可以访问私有成员
            friend TileyWindowStateManager;
    };
}