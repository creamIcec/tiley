#include <linux/input-event-codes.h>

#include <LPointer.h>
#include <LSeat.h>
#include <LCursor.h>
#include <LLog.h>
#include <LKeyboard.h>
#include <LKeyboardKeyEvent.h>
#include <LNamespaces.h>
#include <LSessionLockManager.h>
#include <LDND.h>

#include "Keyboard.hpp"
#include "ShortcutManager.hpp"
#include "src/lib/TileyServer.hpp"
#include "src/lib/core/UserAction.hpp"

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
    // 父类部分处理逻辑下移
    const bool L_CTRL      { isKeyCodePressed(KEY_LEFTCTRL)  };
    const bool L_SHIFT     { isKeyCodePressed(KEY_LEFTSHIFT) };

    const bool sessionLocked { sessionLockManager()->state() != LSessionLockManager::Unlocked };
 
    // 将按键发送给客户端
    sendKeyEvent(event);

    if (L_CTRL && !L_SHIFT) { seat()->dnd()->setPreferredAction(LDND::Copy); }
    else if (!L_CTRL && L_SHIFT) { seat()->dnd()->setPreferredAction(LDND::Move); }
    else if (!L_CTRL && !L_SHIFT) { seat()->dnd()->setPreferredAction(LDND::NoAction); }
 
    // 如果锁屏, 直接结束
    if (sessionLocked) { return; }

    bool altDown = isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);

    TileyServer& server = TileyServer::getInstance();
    server.is_compositor_modifier_down = altDown;
    
    if(!altDown){
        stopResizeSession(true);
        stopMoveSession(true);
    }

    if(event.state() == Louvre::LKeyboardKeyEvent::Pressed) {
        std::string combo = buildComboFromEvent(event, this);

        if(!combo.empty()){
            LLog::debug("按键组合: %s", combo.c_str());
            ShortcutManager::getInstance().tryDispatch(combo);
        }

        if (L_CTRL) { seat()->dnd()->setPreferredAction(LDND::Copy); }
        else if (L_SHIFT) { seat()->dnd()->setPreferredAction(LDND::Move); }
    }
}