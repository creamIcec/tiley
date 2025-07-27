#pragma once

#include "LNamespaces.h"
#include <LBox.h>

using namespace Louvre;

namespace tiley{
    class Surface;
}

namespace tiley{

    // 启动参数。和wlroots时期相同
    struct LaunchArgs{
        bool enableDebug;  // 启用调试。设置为true则打印各种非错误调试信息, false则只打印错误
        char* startupCMD;  // 启动时要运行的linux指令
    };

    // 顺序: 从底层到顶层, 不能随意改变哦
    enum TILEY_LAYERS{
        BACKGROUND_LAYER,    //背景层。没有任何东西显示时显示的层, 例如壁纸、纯色填充等等
        DESKTOP_LAYER,  //桌面层。放置一些桌面的小部件, 例如时钟等
        APPLICATION_LAYER,   //应用相关窗口, 包括他的弹出窗口, 里面的特殊区域(视频区域、画布区域)等
        POPUP_LAYER,    // 输入法窗口, 系统级的提示窗口等
        OVERLAY_LAYER,  // 用于显示一些整个显示器相关的内容, 例如锁屏, 显示器插入时的桌面淡入动画
    };

    // 某个container的分割类型
    enum SPLIT_TYPE{
        SPLIT_NONE,  // 窗口, 叶子节点, 不进行分割
        SPLIT_H,     // 该容器水平分割(左右)
        SPLIT_V      // 该容器垂直分割(上下)
    };

    // 某个container的浮动信息, 表示因何种原因而浮动
    enum FLOATING_REASON{
        NONE,       // 不浮动
        MOVING,     // 因为用户正在移动窗口
        STACKING,   // 因为用户请求将这个窗口变成堆叠显示, 可以堆叠显示在其他窗口上方
    };

    // 一个NodeContainer可以代表一个叶子节点(真正的窗口)
    // 也可以代表一个容器(用于分割的)
    // 暂时不使用, 因为可能是相对放置, 我们无需将数据和渲染分离
    struct NodeContainer{
        enum SPLIT_TYPE split;
        enum FLOATING_REASON floating;

        struct NodeContainer* parent;
        struct NodeContainer* child1;   //这里我们不以上下左右命名, 只用编号, 避免混淆
        struct NodeContainer* child2;
        Surface* window;   //双向指针其二: NodeContainer->Surface 当是叶子节点时, 指向一个窗口
        LBox box;
        float splitRatio;  //容器分割比例。仅在window==nullptr(是容器)时有效
    };

    // TODO: 取消默认, 动态获取workspace
    static UInt32 DEFAULT_WORKSPACE = 0;
}