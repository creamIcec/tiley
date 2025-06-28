
#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <xkbcommon/xkbcommon.h>

extern std::unordered_map<std::string, std::string> g_hotkey_map;
extern std::mutex g_hotkey_mutex;

// 从文件加载 JSON 配置到 g_hotkey_map
void load_hotkey_config(const std::string& path);

// 将 XKB sym + modifiers 转成形如 "ctrl+Shift+Q" 的字符串
std::string keycombo_to_string(uint32_t keycode, uint32_t modifiers);

// 执行对应功能的操作
void execute_hotkey_action(const std::string& action);
void watch_hotkey_file(const std::string& path);
std::string keysym_to_string(xkb_keysym_t keysym);
std::string keycombo_to_string(uint32_t keycode, uint32_t modifiers, struct xkb_state* xkb_state);