#ifndef __SCENE_H__
#define __SCENE_H__

#include <LScene.h>
#include "src/lib/client/views/LayerView.hpp"

using namespace Louvre;

namespace tiley{
    class Scene final : public LScene{
        public:

            Scene() noexcept;

            LayerView layers[5];
    };
}

#endif //__SCENE_H__