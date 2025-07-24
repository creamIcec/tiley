#ifndef __SCENE_H__
#define __SCENE_H__

#include <LScene.h>
#include "src/lib/client/views/LayerView.hpp"

using namespace Louvre;

namespace tiley{
    class Scene final : public LScene{
        public:

            Scene() noexcept;

            /*
                场景图层顺序:  overlayLayer > lockscreenLayer > floatingLayer > tiledLayer > backgroundLayer    
                overlayLayer;      //任何需要显示在锁屏之上的东西
                lockscreenLayer;   //锁屏层.
                floatingLayer;     //浮动层. 当窗口浮动时, 移动到该层反之移回正常层次.
                tiledLayer;        //平铺层.
                backgroundLayer;   //背景层, 显示壁纸.
            */

            LayerView layers[5];
    };
}

#endif //__SCENE_H__