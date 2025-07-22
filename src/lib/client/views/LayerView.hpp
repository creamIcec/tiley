#ifndef __LAYER_VIEW_H__
#define __LAYER_VIEW_H__

#include <LLayerView.h>

using namespace Louvre;

// 图层定义, 对应wlroots中的scene tree(场景树)
// 我们还是用这个实现壁纸层, 平铺层和浮动层

namespace tiley{
    class LayerView final : public LLayerView{
        using LLayerView::LLayerView;
    };
}

#endif