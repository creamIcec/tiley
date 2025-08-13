#pragma once

#include "LNamespaces.h"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/types.hpp"
#include "src/lib/test/PerfmonRegistry.hpp"
#include <LOutput.h>
#include <LSolidColorView.h>
#include <LTextureView.h>
#include "src/lib/test/PerformanceMonitor.hpp" 

using namespace Louvre;
using namespace tiley;

namespace tiley{
    class Surface;
}

namespace tiley{
    class Output final : public LOutput{
        public:

            using LOutput::LOutput;

            Output(const void* params) noexcept;

            // 屏幕插入
            void initializeGL() override;

            // 类似wlroots中的output_frame
            void paintGL() override;

            // moveGL, resizeGL类似wlroots中的request_state事件, 这里分成了两个
            // 移动屏幕的index
            void moveGL() override;

            // 屏幕缩放变化，尺寸变化(物理上不太可能)
            void resizeGL() override;

            // 类似wlroots中的output_destroy, 屏幕拔出
            void uninitializeGL() override;

            // Louvre实现了gamma-control-v1 wayland协议, 因此我们可以接受客户端传入的Gamma表, 来动态调整屏幕的亮度管理
            // Gamma本身是一个定值, 代表一条亮度映射曲线, 用于将RGB编码的各分量的亮度进行幂运算变换, 得到符合屏幕和人眼感受规律的亮度曲线。
            // 此处设置这个函数, 是为了方便游戏、视频等需要设置自己的亮度时, 传入以表格表示的映射关系, 设置屏幕的整体变换关系。
            // 正常情况下, sRGB标准色彩配置的Gamma=2.2, 早期苹果显示器的标准是1.8
            // 所有的亮度x(已从[0,255]经过归一化到[0,1])都将经过运算: 
            // f(x) = x_new = clamp(x^(1/gamma), 0, 1), 即, 计算亮度的(gamma的倒数)次方, 再限制到[0,1]范围中。实际上x^(1/gamma)正常情况下就是[0,1]的, 因为gamma通常>1
            // 函数f(x)关于gamma是单调递增, 且一阶导数越来越小的。
            // 简而言之, gamma就是调整亮度用的。
            void setGammaRequest(LClient* client, const LGammaTable* gamma) override;

            void availableGeometryChanged() override;

            // 一个非常好的参考shader案例: 淡入
            LSolidColorView fadeInView{{1.f, 1.f, 1.f}};

            Surface *searchFullscreenSurface() const noexcept;
            bool tryDirectScanout(Surface *surface) noexcept;

            // 壁纸
            void updateWallpaper();
            
            // 打印壁纸信息
            void printWallpaperInfo();
            
            LTextureView wallpaperView{nullptr, &TileyServer::getInstance().layers()[BACKGROUND_LAYER]};
    };
}