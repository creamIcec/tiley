#include "Keyboard.hpp"
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

#include <LPointer.h>
#include <LSeat.h>
#include <LCursor.h>
#include <LLog.h>
#include <LKeyboardKeyEvent.h>
#include "LKeyboard.h"
#include "LNamespaces.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/core/UserAction.hpp"
#include "src/lib/surface/Surface.hpp"
#include <Louvre/LKeyboardKeyEvent.h>

#include<algorithm>
#include<cctype>
#include<map>
#include<string>
#include<vector>
#include<functional>


using namespace tiley;

static std::map<std::string, std::string> shortcut_map; // combo -> action name
using Handler = std::function<void(const std::string& combo)>;
static std::map<std::string, Handler> action_map;

//统一字符为小写字母
static std::string normalizeToken(std::string t){
    std::transform(t.begin(),t.end(),t.begin(),[](unsigned char c){
        return std::tolower(c);
    });
    return t;
}
//拼接修饰键和字符串(修饰键+主键，统一为小写格式)
static std::string makeComboString(const std::vector<std::string>& modifiers, const std::string& key){
    std::vector<std::string> m = modifiers;
    for(auto& s : m) s = normalizeToken(s);
    std::sort(m.begin(), m.end());
    std::string combo;
    for(size_t i = 0; i < m.size(); ++i){
        combo += m[i];
        combo += "+";
    }
    combo += normalizeToken(key);
    return combo;
}
/*
//检查修饰键
static bool isXkbModifierActive(xkb_state* state, const char* name){
    if(!state || !name) return false;
    xkb_mod_index_t idx = xkb_keymap_mod_get_index(xkb_state_get_keymap(state), name);
    if(idx == XKB_MOD_INVALID) return false;
    return xkb_state_mod_index_is_active(state, idx, XKB_STATE_MODS_EFFECTIVE);
}
*/
//获得修饰键得到的修饰键+字符
static std::string buildComboFromEvent(const LKeyboardKeyEvent& event, LKeyboard* keyboard){
    std::vector<std::string>modifiers;
    if (keyboard->isModActive(XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE))  modifiers.emplace_back("ctrl");
    if (keyboard->isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE))   modifiers.emplace_back("alt");
    if (keyboard->isModActive(XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE)) modifiers.emplace_back("shift");
    if (keyboard->isModActive(XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE))  modifiers.emplace_back("super");
    //获得keysym    
    xkb_keysym_t sym = keyboard->keySymbol(event.keyCode());
    char nameBuf[64] = {0};
    xkb_keysym_get_name(sym, nameBuf, sizeof(nameBuf));
    std::string key;
    if (nameBuf[0]) {
        key = std::string(nameBuf);
    } else {
        key = "code" + std::to_string(event.keyCode());
    }
    // 统一小写
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    return makeComboString(modifiers, key);
}


static void executeAction(const std::string& actionName, const std::string& combo) {
    auto it = action_map.find(actionName);
    if (it != action_map.end()) {
        //注意，这里是绑定的函数与对应指令，所谓的值是函数
        it->second(combo);
    } else {
        LLog::warning("快捷键动作未注册: %s (combo=%s)", actionName.c_str(), combo.c_str());
    }
}

static void loadShortcutsFromFile(const std::string& path) {
//TODO:暂时写死，先不写入json,后续替换
  shortcut_map.clear();
  shortcut_map["ctrl+space"] = "launch_app_launcher";
  shortcut_map["ctrl+t"] = "launch_terminal";
  shortcut_map["alt+t"] = "change_wallpaper";
}

//这里负责把对应函数逻辑写进来（这里只需要注册一次）
static void registerDefaultHandlers() {
    LLog::debug("调用了registerDefaultHandlers()");
    
    if(!action_map.empty())return;

      action_map["launch_terminal"] = [](const std::string&){
        LLog::debug("执行: launch_terminal");
        
    };
    action_map["launch_app_launcher"] = [](const std::string&){
        LLog::debug("执行: launch_app_launcher");
    };
    action_map["change_wallpaper"] = [](const std::string&){
        LLog::debug("执行: change_wallpaper");
    };
    // TODO:其他 handler 注册...,当然了上面三个也只是虚假实现QAQ
}

//查询
static bool tryDispatchShortcut(const std::string& combo){
    auto it =shortcut_map.find(combo);
    if(it!=shortcut_map.end()){
        executeAction(it->second,combo);
        return true;
    }
     return false;
}




void Keyboard::keyEvent(const LKeyboardKeyEvent& event){
   //TODO:暂时写死，暂时先不实现热重载，不过思路没问题，后续再改
   static bool first_load = true;
   if (first_load) {
        registerDefaultHandlers();
        loadShortcutsFromFile("/home/zero/tiley/hotkey.json"); 
        LLog::debug("初次加载");
        first_load = false;
    }
   
   std::string combo = buildComboFromEvent(event,this);
    // 先处理自己的, 再调用父级   
    // 首先判断是不是合成器指令
     if(!combo.empty()){
        LLog::debug("按键组合: %s", combo.c_str());
        if(tryDispatchShortcut(combo)){
            // TODO
            return;
        }
    }
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    TileyServer& server = TileyServer::getInstance();

    server.is_compositor_modifier_down = isModActive(XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    
    

    /*
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
*/
    LKeyboard::keyEvent(event);
}