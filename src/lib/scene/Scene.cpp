#include "Scene.hpp"

#include <LSceneView.h>
#include <LScene.h>

#include "src/lib/types.h"

using namespace tiley;
using namespace Louvre;

Scene::Scene() noexcept : LScene(){
    for(int i = 0; i <= BACKGROUND_LAYER; i++){
        layers[i].setParent(this->mainView());
    }
}