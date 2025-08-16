#pragma once

#include <LKeyboard.h>
#include<algorithm>
#include<cctype>
#include<map>
#include<string>
#include<vector>
#include<functional>
using namespace Louvre;

namespace tiley{
    class Keyboard final : public LKeyboard{
        public:
            using LKeyboard::LKeyboard;
            void keyEvent(const LKeyboardKeyEvent& event) override;
    };
    //先全部在cpp申明了,跑通再说。
    /*
    std::map<std::string,std::string>shortcut_map;
    using Handler=std::function<void(const std::string& combo)>;
     std::map<std::string,Handler>action_map;
     std::string normalizeToken(std::string t);
    //报错重载,先注释掉看看什么情况
     std::string makeComboString(const std::vector<std::string>& modifiers,const std::string& key);
     std::string buildComboFromEvent(const LKeyboardKeyEvent& event) ;
     void executeAction(const std::string& actionName, const std::string& combo) ;
    void loadShortcutsFromFile(const std::string& path);
     void registerDefaultHandlers();
    bool tryDispatchShortcut(const std::string& combo);
*/
    }
