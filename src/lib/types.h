#pragma once

namespace tiley{
    enum TILEY_LAYERS{
        OVERLAY_LAYER, // 用于显示一些整个显示器相关的内容, 例如显示器插入时的桌面淡入动画
        LOCKSCREEN_LAYER,
        FLOATING_LAYER,
        TILED_LAYER,
        BACKGROUND_LAYER
    };
}