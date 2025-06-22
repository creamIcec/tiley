
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

std::string keycombo_to_string(uint32_t keycode, uint32_t modifiers) {
    std::string s;
    if (modifiers & WLR_MODIFIER_CTRL)  s += "ctrl+";
    if (modifiers & WLR_MODIFIER_ALT)   s += "alt+";
    if (modifiers & WLR_MODIFIER_SHIFT) s += "Shift+";
    s += std::to_string(keycode);
    return s;
}

void execute_hotkey_action(const std::string& action) {
    if (action == "close_window") {
        // 调用关闭窗口的功能
        // close_window(); // 此处调用你已有的close_window()函数
    } else if (action == "launch_terminal") {
        system("alacritty &");
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