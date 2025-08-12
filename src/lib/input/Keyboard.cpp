#include "Keyboard.hpp"
#include "ShortcutManager.hpp"

#include <linux/input-event-codes.h>

#include <LPointer.h>
#include <LSeat.h>
#include <LCursor.h>
#include <LLog.h>
#include <LKeyboard.h>
#include <LKeyboardKeyEvent.h>
#include <LNamespaces.h>

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

    std::string combo = buildComboFromEvent(event, this);
    if(!combo.empty()){
        LLog::debug("按键组合: %s", combo.c_str());
        if(ShortcutManager::getInstance().tryDispatch(combo)){
            return; // 命中后吞掉，不走默认逻辑
        }
    }

    LLog::debug("[keyEvent]: 尝试切换窗口浮动");

    //  旧逻辑（alt+space ）
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    bool altDown = isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    server.is_compositor_modifier_down = altDown;

    if(server.is_compositor_modifier_down){
        if(event.state() == Louvre::LKeyboardKeyEvent::Pressed && event.keyCode() == KEY_SPACE){
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
