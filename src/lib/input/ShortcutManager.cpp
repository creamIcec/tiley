#include "ShortcutManager.hpp"

#include <mutex>
#include <nlohmann/json.hpp>
#include <sys/inotify.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <LLog.h>
#include "src/lib/TileyServer.hpp"

using json = nlohmann::json;
using namespace tiley;

//每200m才真正重置一次，防止多时间频繁修改
static constexpr auto kDebounceMs = 200;

ShortcutManager& ShortcutManager::instance(){
    static ShortcutManager inst;
    return inst;
}

ShortcutManager::ShortcutManager() : stopWatcher_(false) {}

ShortcutManager::~ShortcutManager(){
    stopWatcher_.store(true);
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
}

//初始化并且开始监控
void ShortcutManager::init(const std::string& jsonPath){
    std::lock_guard lock(mutex_);
    loadFromFile(jsonPath);
    startWatcher(jsonPath);
}

//绑定对应功能函数，TODO:后续可以继续绑定其它各种功能函数
void ShortcutManager::registerHandler(const std::string& actionName, ShortcutHandler handler){
    std::lock_guard lock(mutex_);
    handlers_[actionName] = std::move(handler);
}

//通过对应事件调用对应函数
bool ShortcutManager::tryDispatch(const std::string& combo){
    std::lock_guard lock(mutex_);
    auto it = comboToAction_.find(combo);
    if (it == comboToAction_.end())
        return false;
    const std::string& actionName = it->second;
    auto hit = handlers_.find(actionName);
    if (hit != handlers_.end()){
        hit->second(combo);
    } else {
        LLog::warning("快捷键动作未注册: %s (combo=%s)", actionName.c_str(), combo.c_str());
        return false;  //未注册则不命中
    }
    return true;
}

//规范化并拼接
std::string ShortcutManager::normalizeCombo(const std::string& raw){
    std::string lower;
    lower.reserve(raw.size());
    for(char c : raw) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::vector<std::string> parts;
    std::stringstream ss(lower);
    std::string token;
    while(std::getline(ss, token, '+')){
        if(!token.empty())
            parts.push_back(token);
    }
    std::vector<std::string> mods;
    std::string key;
    static const std::vector<std::string> modNames = {"ctrl", "alt", "shift", "super", "logo", "meta"};
    for(auto& p : parts){
        if(std::find(modNames.begin(), modNames.end(), p) != modNames.end()){
            if(p == "logo" || p == "meta") p = "super";
            mods.push_back(p);
        } else {
            key = p;
        }
    }
    if(key.empty() && !mods.empty()){
        key = mods.back();
        mods.pop_back();
    }
    std::sort(mods.begin(), mods.end());
    std::string combo;
    for(const auto& m : mods){
        combo += m;
        combo += "+";
    }
    combo += key;
    return combo;
}

//加载json文件
void ShortcutManager::loadFromFile(const std::string& path){
    std::ifstream ifs(path);
    if(!ifs){
        LLog::warning("加载快捷键配置失败: 无法打开 %s", path.c_str());
        return;
    }

    try{
        json j;
        ifs >> j;
        std::map<std::string, std::string> newMap;
        for(auto& [rawCombo, actionJson] : j.items()){
            std::string combo = normalizeCombo(rawCombo);
            std::string actionName = actionJson.get<std::string>();
            if(!combo.empty() && !actionName.empty()){
                newMap[combo] = actionName;
                LLog::debug("快捷键载入: %s -> %s", combo.c_str(), actionName.c_str());
            }
        }
        //原子化
        comboToAction_.swap(newMap);
    } catch(const std::exception& e){
        LLog::warning("解析快捷键 JSON 失败: %s", e.what());
    }
}

//监控
void ShortcutManager::startWatcher(const std::string& path){
    watcherThread_ = std::thread([this, path](){
        int fd = inotify_init1(IN_NONBLOCK);
        if(fd < 0){
            LLog::warning("inotify 初始化失败");
            return;
        }
        int wd = inotify_add_watch(fd, path.c_str(), IN_MODIFY | IN_CLOSE_WRITE);
        if(wd < 0){
            LLog::warning("无法添加 inotify 监控: %s", path.c_str());
            close(fd);
            return;
        }

        auto last = std::chrono::steady_clock::now() - std::chrono::milliseconds(kDebounceMs);
        while(!stopWatcher_.load()){
            char buf[1024];
            ssize_t len = read(fd, buf, sizeof(buf));
            if(len > 0){
                size_t i = 0;
                while(i < static_cast<size_t>(len)){
                    struct inotify_event* ev = reinterpret_cast<struct inotify_event*>(buf + i);
                    if(ev->mask & (IN_MODIFY | IN_CLOSE_WRITE)){
                        auto now = std::chrono::steady_clock::now();
                        if(now - last >= std::chrono::milliseconds(kDebounceMs)){
                            LLog::debug("检测到快捷键配置变化，重新加载");
                            {
                                std::lock_guard lock(mutex_);
                                loadFromFile(path);
                            }
                            last = now;
                        }
                    }
                    i += sizeof(struct inotify_event) + ev->len;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        inotify_rm_watch(fd, wd);
        close(fd);
    });
}