#pragma once

#include <LSurfaceView.h>

using namespace Louvre;

class SurfaceView final : public LSurfaceView{
    public:
        using LSurfaceView::LSurfaceView;
        void pointerButtonEvent (const LPointerButtonEvent &event) override;
        void pointerEnterEvent 	(const LPointerEnterEvent &event) override;
};