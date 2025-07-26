#include "Keyboard.hpp"
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

#include <LPointer.h>
#include <LSeat.h>
#include <LCursor.h>
#include <LLog.h>

#include "LKeyboard.h"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/surface/Surface.hpp"

using namespace tiley;

void Keyboard::keyEvent(const LKeyboardKeyEvent& event){
    // 先处理自己的, 再调用父级   
    // 首先判断是不是合成器指令

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();

    if(isModActive(XKB_VMOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE)){
         // Alt + 空格 = 浮动窗口
         if(isKeyCodePressed(KEY_SPACE)){
            LLog::log("检测到合成器修饰键+空格按下。尝试切换窗口堆叠状态...");
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
    }

    LKeyboard::keyEvent(event);
}