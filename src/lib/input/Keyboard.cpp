#include "Keyboard.hpp"
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

#include <LPointer.h>
#include <LSeat.h>
#include <LCursor.h>
#include <LLog.h>

#include "LKeyboard.h"
#include "LNamespaces.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/core/UserAction.hpp"
#include "src/lib/surface/Surface.hpp"

using namespace tiley;

void Keyboard::keyEvent(const LKeyboardKeyEvent& event){
    // 先处理自己的, 再调用父级   
    // 首先判断是不是合成器指令

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    server.is_compositor_modifier_down = isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    //这里有问题
/*
    std::string keycombo_to_string(uint32_t keycode, uint32_t modifiers) {
    std::string s;
    if (modifiers & WLR_MODIFIER_CTRL)  s += "ctrl+";
    if (modifiers & WLR_MODIFIER_ALT)   s += "alt+";
    if (modifiers & WLR_MODIFIER_SHIFT) s += "Shift+";
    s += std::to_string(keycode);
    return s;
   }
    std::string combo = keycombo_to_string(
        event.keycode,
        event.state,        样
        keyboard()->wlr_keyboard->xkb_state
    );
*/
    if(server.is_compositor_modifier_down){
       // LLog::debug("检测到按下了修饰键");
         // Alt + 空格 = 浮动窗口
         if(isKeyCodePressed(KEY_SPACE)){
            LLog::debug("检测到合成器修饰键+空格按下。尝试切换窗口堆叠状态...");
            // 1. 获取当前的焦点
            LSurface* surface = seat()->pointer()->surfaceAt(cursor()->pos());
            
            // 如果当前在一个surface上
            if(surface){
                Surface* targetSurface = static_cast<Surface*>(surface);
                // 并且是一个窗口
                if(targetSurface->tl()){
                    manager.toggleStackWindow(targetSurface->tl());
                }
            }
         }
    }else{
        // 释放所有正在进行的移动窗口或者调整窗口大小的操作
        // TODO: 迁移到eventFilter, 比键盘/鼠标更加靠前, 是最先处理的。避免在键盘和鼠标事件中都编写处理逻辑
        stopResizeSession(true);
        stopMoveSession(true);
    }

    LKeyboard::keyEvent(event);
}