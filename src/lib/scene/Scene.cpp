#include "Scene.hpp"

#include <LSceneView.h>
#include <LScene.h>

#include "src/lib/types.hpp"

using namespace tiley;
using namespace Louvre;

Scene::Scene() noexcept : LScene(){
    for(int i = 0; i <= OVERLAY_LAYER; i++){
        // mainView返回的LSceneView是唯一的。
        layers[i].setParent(this->mainView());
    }
}