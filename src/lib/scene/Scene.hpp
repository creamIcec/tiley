#ifndef __SCENE_H__
#define __SCENE_H__

#include <LScene.h>
#include "LayerView.hpp"

using namespace Louvre;

namespace tiley{
    class Scene final : public Louvre::LScene{
        public:
            // 暂时使用官方示例中的5层(背景, 底层, 中层, 顶层和覆盖层)
            // 需要改成我们的平铺分层(最终4层: 背景(壁纸), 平铺层, 浮动层, 覆盖层(顶栏/锁屏等))
            LayerView layers[5];
    };
}

#endif //__SCENE_H__