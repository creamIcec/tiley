#include "Keyboard.hpp"
#include "ShortcutManager.hpp"

#include <linux/input-event-codes.h>

#include <LPointer.h>
#include <LSeat.h>
#include <LCursor.h>
#include <LLog.h>

#include "LKeyboard.h"
#include "LKeyboardKeyEvent.h"
#include "LNamespaces.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/core/UserAction.hpp"
#include "src/lib/surface/Surface.hpp"

#include <algorithm>
#include <cctype>
#include <string>

using namespace tiley;

// 组合键构造（基于 Louvre 提供接口）
static std::string buildComboFromEvent(const Louvre::LKeyboardKeyEvent& event, Louvre::LKeyboard* keyboard){
    std::vector<std::string> modifiers;
    if (keyboard->isModActive(XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE))  modifiers.emplace_back("ctrl");
    if (keyboard->isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE))   modifiers.emplace_back("alt");
    if (keyboard->isModActive(XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE)) modifiers.emplace_back("shift");
    if (keyboard->isModActive(XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE))  modifiers.emplace_back("super");

    xkb_keysym_t sym = keyboard->keySymbol(event.keyCode());
    char nameBuf[64] = {0};
    xkb_keysym_get_name(sym, nameBuf, sizeof(nameBuf));
    std::string key;
    if (nameBuf[0]) {
        key = std::string(nameBuf);
    } else {
        key = "code" + std::to_string(event.keyCode());
    }

    // 统一小写并拼接
    std::string raw;
    for(auto& m : modifiers){
        std::string lower;
        lower.reserve(m.size());
        for(char c : m) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        raw += lower + "+";
    }
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    raw += key;
    return ShortcutManager::normalizeCombo(raw);
}

void Keyboard::keyEvent(const Louvre::LKeyboardKeyEvent& event){
    static bool initialized = false;
    if (!initialized) {
        ShortcutManager& mgr = ShortcutManager::instance();
        mgr.init("/home/iriseplos/projects/os/tiley/hotkey.json");

        //注册测试用默认命令TOFO:在Handle里封装各个功能函数然后在此调用。
        mgr.registerHandler("launch_terminal",    [](auto){ LLog::log("执行: launch_terminal"); });
        mgr.registerHandler("launch_app_launcher",[](auto){ LLog::log("执行: launch_app_launcher"); });
        mgr.registerHandler("change_wallpaper",   [](auto){ LLog::log("执行: change_wallpaper"); });

        // 注册工作区切换，理论上可以注册最大工作区的数量呢
        mgr.registerHandler("goto_ws_1", [](auto){ 
            LLog::log("执行: goto_ws_1");
            TileyWindowStateManager::getInstance().switchWorkspace(0);
        });
        mgr.registerHandler("goto_ws_2", [](auto){ 
             LLog::log("执行: goto_ws_2"); 
            TileyWindowStateManager::getInstance().switchWorkspace(1);
        });
        mgr.registerHandler("goto_ws_3", [](auto){ 
            LLog::log("执行: goto_ws_3"); 
            TileyWindowStateManager::getInstance().switchWorkspace(2);
        });
     
        initialized = true;
        LLog::debug("快捷键系统初始化完成（模块化）");
    }

    std::string combo = buildComboFromEvent(event, this);
    if(!combo.empty()){
        LLog::debug("按键组合: %s", combo.c_str());
        if(ShortcutManager::instance().tryDispatch(combo)){
            return; // 命中后吞掉，不走默认逻辑
        }
    }

    //  旧逻辑（alt+space ）
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    bool altDown = isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    server.is_compositor_modifier_down = altDown;

    if(server.is_compositor_modifier_down){
        if(event.state() == Louvre::LKeyboardKeyEvent::Released && event.keyCode() == KEY_SPACE){
            LLog::debug("检测到合成器修饰键+空格按下。尝试切换窗口堆叠状态...");
            Louvre::LSurface* surface = seat()->pointer()->surfaceAt(cursor()->pos());
            if(surface){
                Surface* targetSurface = static_cast<Surface*>(surface);
                if(targetSurface->tl()){
                    manager.toggleStackWindow(targetSurface->tl());
                }
            }
        }
    } else {
        stopResizeSession(true);
        stopMoveSession(true);
    }

    // 继续默认行为（发送给客户端等）
    LKeyboard::keyEvent(event);
}
