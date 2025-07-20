#pragma once

#include <LBox.h>

using namespace Louvre;

namespace tiley{
    class Surface;
}

namespace tiley{
    // 顺序: 从底层到顶层, 不能随意改变哦
    enum TILEY_LAYERS{
        BACKGROUND_LAYER,
        TILED_LAYER,
        FLOATING_LAYER,
        LOCKSCREEN_LAYER,
        OVERLAY_LAYER, // 用于显示一些整个显示器相关的内容, 例如显示器插入时的桌面淡入动画
    };

    // 某个container的分割信息
    enum SplitInfo{
        SPLIT_NONE,  // 窗口, 叶子节点, 不进行分割
        SPLIT_H,     // 该容器水平分割(左右)
        SPLIT_V      // 该容器垂直分割(上下)
    };

    // 某个container的浮动信息, 表示因何种原因而浮动
    enum FloatingReason{
        NONE,       // 不浮动
        MOVING,     // 因为用户正在移动窗口
        STACKING,   // 因为用户请求将这个窗口变成堆叠显示, 可以堆叠显示在其他窗口上方
    };

    // 一个NodeContainer可以代表一个叶子节点(真正的窗口)
    // 也可以代表一个容器(用于分割的)
    struct NodeContainer{
        enum SplitInfo split;
        enum FloatingReason floating;

        struct NodeContainer* parent;
        struct NodeContainer* child1;   //这里我们不以上下左右命名, 只用编号, 避免混淆
        struct NodeContainer* child2;
        Surface* window;   //双向指针其二: NodeContainer->Surface 当是叶子节点时, 指向一个窗口
        LBox box;
        float splitRatio;  //容器分割比例。仅在window==nullptr(是容器)时有效
    };
}