#include "Seat.hpp"
#include "LKeyboardKeyEvent.h"
#include "LNamespaces.h"
#include "LSeat.h"
#include <LCompositor.h>
#include <LInputDevice.h>
#include <libinput.h>

#include <LEvent.h>
#include <LKeyboard.h>
#include <xkbcommon/xkbcommon.h>

using namespace tiley;

void Seat::configureInputDevices() noexcept{

    // not libinput probably means tiley is running in nested mode.
    // we simply use input service provided by parent
    if(compositor()->inputBackendId() != LInputBackendLibinput){
        return;
    }

    libinput_device* dev;

    for(LInputDevice* device : inputDevices()){
        if(!device->nativeHandle()){
            continue;
        }

        dev = static_cast<libinput_device*>(device->nativeHandle());

        // Accessibility configurations

        // disable touchpad while typing: to avoid exceptional touch input
        libinput_device_config_dwt_set_enabled(dev, LIBINPUT_CONFIG_DWT_DISABLED);

        libinput_device_config_click_set_method(dev, LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);

        // enable natural scroll: user appealing
        libinput_device_config_scroll_set_natural_scroll_enabled(dev, true);
        
    }
}

// TODO: all input events processing start from here
bool Seat::eventFilter(LEvent& event){
    
    return LSeat::eventFilter(event);
    
    /*
    if(event.type() == Louvre::LEvent::Type::Keyboard){
        if(event.subtype() == Louvre::LEvent::Subtype::Key){
            auto keyEvent = static_cast<LKeyboardKeyEvent&>(event);
            // ...
        }
    }
    */
}