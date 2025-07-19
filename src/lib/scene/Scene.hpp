#ifndef __SCENE_H__
#define __SCENE_H__

#include <LScene.h>
#include "LayerView.hpp"

using namespace Louvre;

namespace tiley{
    class Scene final : public LScene{
        public:

            Scene() noexcept;

            /*
                场景树图层顺序:  floatingLayer > tiledLayer > backgroundLayer
                lockscreenView;    //锁屏层.
                floatingLayer;     //浮动层. 当窗口浮动时, 移动到该层反之移回正常层次.
                tiledLayer;        //正常层.
                backgroundLayer;   //背景层, 显示壁纸.
            */

            LayerView overlayLayer;
            LayerView lockscreenLayer;
            LayerView floatingLayer;
            LayerView tiledLayer;
            LayerView backgroundLayer;

            LayerView layers[5]{&overlayLayer, &lockscreenLayer, &floatingLayer, &tiledLayer, &backgroundLayer};
    };
}

#endif //__SCENE_H__