#ifndef __SURFACE_VIEW_H__
#define __SURFACE_VIEW_H__

#include <LSurfaceView.h>

using namespace Louvre;

class SurfaceView final : public LSurfaceView{
    public:
        using LSurfaceView::LSurfaceView;
        void pointerButtonEvent (const LPointerButtonEvent &event) override;
        void pointerEnterEvent 	(const LPointerEnterEvent &event) override;
};

#endif