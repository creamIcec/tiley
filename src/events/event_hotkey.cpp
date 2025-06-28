
#include "hotkey.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
#include <sys/inotify.h>
#include <thread>
#include <wayland-server-core.h>  
#include <wlr/types/wlr_keyboard.h>
#include <chrono> 
#include"include/interact.hpp"
#include "wlr/util/log.h"
#include <xkbcommon/xkbcommon.h>

using json = nlohmann::json;

std::unordered_map<std::string, std::string> g_hotkey_map;
std::mutex g_hotkey_mutex;
/*
步骤 1：初始化 inotify
使用 inotify_init1(IN_NONBLOCK) 初始化 inotify 实例，获取文件描述符。

步骤 2：添加文件监控
使用 inotify_add_watch(fd, path.c_str(), IN_MODIFY) 监控指定文件的修改事件。

步骤 3：读取事件
进入循环，使用 read(fd, buf, sizeof(buf)) 读取文件变化事件。

如果有事件，触发相应的处理函数（如 load_hotkey_config(path)）。

步骤 4：延迟循环
使用 std::this_thread::sleep_for(std::chrono::seconds(1)) 延迟 1 秒，减少 CPU 占用。

*/

//加载json文件
void load_hotkey_config(const std::string& path) {
     std::cerr << "正在加载配置文件 " << "\n";
     // wlr_log(WLR_DEBUG, "看到它，说明正在加载json文件");
    std::ifstream in{path};
    if (!in) {
        std::cerr << "找不到您的配置文件: " << path << "\n";
        return;
    }
    json j; in >> j;
    std::lock_guard _(g_hotkey_mutex);
    g_hotkey_map.clear();
    for (auto& [k, v] : j.items()) {
        g_hotkey_map[k] = v.get<std::string>();
    }
    std::cerr << "Hotkeys reloaded (" << g_hotkey_map.size() << " entries)\n";
}

std::string keysym_to_string(xkb_keysym_t keysym) {
    char buf[64];
    int n = xkb_keysym_get_name(keysym, buf, sizeof(buf));
    return (n > 0) ? std::string(buf) : "unknown";
}

std::string keycombo_to_string(uint32_t keycode, uint32_t modifiers, struct xkb_state* xkb_state) {
    std::string s;
    if (modifiers & WLR_MODIFIER_CTRL)  s += "ctrl+";
    if (modifiers & WLR_MODIFIER_ALT)   s += "alt+";
    if (modifiers & WLR_MODIFIER_SHIFT) s += "Shift+";

    const xkb_keysym_t* syms;
    int nsyms = xkb_state_key_get_syms(xkb_state, keycode, &syms);

    if (nsyms > 0) {
        s += keysym_to_string(syms[0]);  // 只取第一个 keysym
    } else {
        s += "unknown";
    }

    return s;
}


void execute_hotkey_action(const std::string& action) {
   //  wlr_log(WLR_INFO, "若要在Tiley中打开程序, 请在另一个控制台中输入:");
    if (action == "close_window") {
        // 调用关闭窗口的功能
        // close_window(); // 此处调用已有的close_window()函数
    } else if (action == "launch_terminal") {
        printf("说明json文件没问题");
        system("alacritty &");
    }
    if (action == "focus_right") {
       //  wlr_log(WLR_INFO, "调用进了这个函数");
        focus_next_window();
    }
    
    // …可以加入更多功能...
}

void watch_hotkey_file(const std::string& path) {
    int fd = inotify_init1(IN_NONBLOCK);
    int wd = inotify_add_watch(fd, path.c_str(), IN_MODIFY);
    char buf[4096];
    while (true) {
        int len = read(fd, buf, sizeof(buf));
        if (len>0) {
            load_hotkey_config(path);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}