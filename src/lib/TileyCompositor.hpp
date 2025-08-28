#ifndef __TILEY_COMPOSITOR_H__
#define __TILEY_COMPOSITOR_H__

#include "LFactoryObject.h"
#include <LLayerView.h>
#include "LNamespaces.h"
#include <LCompositor.h>
#include "src/lib/scene/Scene.hpp"

using namespace Louvre;

namespace tiley{
    class TileyCompositor final : public Louvre::LCompositor{
        public:
            Scene scene;
            void printToplevelSurfaceLinklist();
        protected:
            void initialized() override;
        
            void uninitialized() override;

            LFactoryObject* createObjectRequest(LFactoryObject::Type objectType, const void* params) override;

            void onAnticipatedObjectDestruction(LFactoryObject* object) override;

            bool createGlobalsRequest() override;

            bool globalsFilter(LClient* client, LGlobal* global) override;

    };
}

#endif // __TILEY_COMPOSITOR_H__