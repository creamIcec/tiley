#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include "LNamespaces.h"
#include <LOutput.h>
#include <LSolidColorView.h>

using namespace Louvre;

namespace tiley{
    class Output final : public LOutput{
        public:

            using LOutput::LOutput;

            void initializeGL() override;

            void paintGL() override;

            void moveGL() override;

            void resizeGL() override;

            void uninitializeGL() override;

            void setGammaRequest(LClient* client, const LGammaTable* gamma) override;

            void availableGeometryChanged() override;

            // 一个非常好的参考shader案例: 淡入
            LSolidColorView fadeInView{{0.f, 0.f, 0.f}};

            //Surface *searchFullscreenSurface() const noexcept;
            //bool tryDirectScanout(Surface *surface) noexcept;

    };
}

#endif // __OUTPUT_H__