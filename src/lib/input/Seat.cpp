#include "Seat.hpp"
#include "LNamespaces.h"
#include <LCompositor.h>
#include <LInputDevice.h>
#include <libinput.h>

using namespace tiley;

void Seat::configureInputDevices() noexcept{
    if(compositor()->inputBackendId() != LInputBackendLibinput){   //如果输入后端不是libinput, 则终止。说明此时非常有可能运行在wayland嵌套模式下, 由父合成器提供输入设备。
        return;
    }

    libinput_device* dev;

    for(LInputDevice* device : inputDevices()){
        if(!device->nativeHandle()){  //如果输入设备无效或者没有插入, 则不配置
            continue;
        }

        // 获取设备。设备数据为了通用, 
        dev = static_cast<libinput_device*>(device->nativeHandle());

        // 配置一些用户友好的功能

        // 启用"在打字时禁用触摸板"功能
        libinput_device_config_dwt_set_enabled(dev, LIBINPUT_CONFIG_DWT_DISABLED);

        // 禁用触摸板的区域限制
        libinput_device_config_click_set_method(dev, LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);

        // 启用平滑滚轮滚动(在用户看网页时非常有效)
        libinput_device_config_scroll_set_natural_scroll_enabled(dev, true);
        
    }
}
