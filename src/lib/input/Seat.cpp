#include "Seat.hpp"
#include "LKeyboardKeyEvent.h"
#include "LNamespaces.h"
#include <LCompositor.h>
#include <LInputDevice.h>
#include <libinput.h>

#include <LEvent.h>
#include <LKeyboard.h>
#include <xkbcommon/xkbcommon.h>

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

// 在这里检测键盘事件, eventFilter会在onEvent之前先被调用, 因此是最顶层的判断时机
bool Seat::eventFilter(LEvent& event){
    // 检测: 是键盘事件, 并且是按键事件
    if(event.type() == Louvre::LEvent::Type::Keyboard){
        if(event.subtype() == Louvre::LEvent::Subtype::Key){
            auto keyEvent = static_cast<LKeyboardKeyEvent&>(event);
            // TODO: 完整的键盘按键记录。此时我们专注于实现移动功能, 因此先不完整记录, 仅使用isModActive判断。
        }
    }
}