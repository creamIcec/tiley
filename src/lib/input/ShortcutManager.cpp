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
#include <LNamespaces.h>
#include <LCursor.h>
#include <LClient.h>
#include <LLauncher.h>

#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/WallpaperManager.hpp"
#include "src/lib/Utils.hpp"

using json = nlohmann::json;
using namespace tiley;

// 200ms debounce
static constexpr auto kDebounceMs = 200;

std::unique_ptr<ShortcutManager, ShortcutManager::ShortcutManagerDeleter> ShortcutManager::INSTANCE = nullptr;
std::once_flag ShortcutManager::onceFlag;

ShortcutManager& ShortcutManager::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new ShortcutManager());
    });
    return *INSTANCE;
}

ShortcutManager::ShortcutManager() : stopWatcher_(false) {}

ShortcutManager::~ShortcutManager(){
    stopWatcher_.store(true);
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
}

void ShortcutManager::init(const std::string& jsonPath){
    std::lock_guard lock(mutex_);
    loadFromFile(jsonPath);
    startWatcher(jsonPath);
}

void ShortcutManager::initializeHandlers(){
    resetHandlers();
    TileyWindowStateManager& windowStateManager = TileyWindowStateManager::getInstance();

    std::string hotkeyPath = getHotkeyConfigPath();

    if (hotkeyPath.empty() || !std::filesystem::exists(hotkeyPath)) {
        LLog::error("[ShortcutManager] Unable to locate any valid configuration file");
        return;
    }

    init(hotkeyPath);

    //TODO: create an 'Action' class to abstract user action, allow pre/post actions.
    registerHandler("launch_terminal", [](auto){ 
        LLog::log("execute: launch_terminal"); 
        const bool L_CTRL  { seat()->keyboard()->isKeyCodePressed(KEY_LEFTCTRL)  };
        const bool R_CTRL  { seat()->keyboard()->isKeyCodePressed(KEY_RIGHTCTRL) };
        const bool L_SHIFT { seat()->keyboard()->isKeyCodePressed(KEY_LEFTSHIFT) };
        const bool L_ALT   { seat()->keyboard()->isKeyCodePressed(KEY_LEFTALT)   };
        const bool mods    { L_ALT || L_SHIFT || L_CTRL || R_CTRL };
        if(!mods){
            LLauncher::launch("weston-terminal");
        }
    });
    registerHandler("launch_app_launcher",[](auto){ LLog::log("execute: launch_app_launcher"); });
    registerHandler("change_wallpaper",   [](auto){ LLog::log("execute: change_wallpaper"); });
    registerHandler("toggle_floating", [&windowStateManager](auto){
        LLog::debug("execute: toggle_floating");
        Louvre::LSurface* surface = seat()->pointer()->surfaceAt(cursor()->pos());
        if(surface){
            Surface* targetSurface = static_cast<Surface*>(surface);
            if(targetSurface->tl()){
                windowStateManager.toggleStackWindow(targetSurface->tl());
            }
        }
    });
    registerHandler("close_window", [](auto){
        LLog::log("execute: close_window");
        if (seat()->keyboard()->focus()){
            seat()->keyboard()->focus()->client()->destroyLater();
        }
    });
    registerHandler("screenshot",[](auto){
        if (cursor()->output() && cursor()->output()->bufferTexture(0)){
            std::filesystem::path path { getenvString("HOME") };

            if (path.empty())
                return;

            path /= "Desktop/Louvre_Screenshoot_";

            char timeString[32];
            const auto now { std::chrono::system_clock::now() };
            const auto time { std::chrono::system_clock::to_time_t(now) };
            std::strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S.png", std::localtime(&time));

            path += timeString;

            cursor()->output()->bufferTexture(0)->save(path);
        }
    });
    registerWorkspacesHandler();
    registerHandler("quit_compositor", [](auto){
        compositor()->finish();
    });
    registerHandler("change_wallpaper", [](auto){
        WallpaperManager::getInstance().selectAndSetNewWallpaper();
    });
    LLog::debug("shortcut management initialized");
}

void ShortcutManager::registerWorkspacesHandler(){

    TileyWindowStateManager& windowStateManager = TileyWindowStateManager::getInstance();
    // TODO: hard-coded workspace switching actions
    registerHandler("goto_ws_1", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_1");
        windowStateManager.switchWorkspace(0);
    });

    registerHandler("goto_ws_2", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_2"); 
        windowStateManager.switchWorkspace(1);
    });
    
    registerHandler("goto_ws_3", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_3"); 
        windowStateManager.switchWorkspace(2);
    });
    
    registerHandler("goto_ws_4", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_4"); 
        windowStateManager.switchWorkspace(3);
    });
    
    registerHandler("goto_ws_5", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_5"); 
        windowStateManager.switchWorkspace(4);
    });
    
    registerHandler("goto_ws_6", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_6"); 
        windowStateManager.switchWorkspace(5);
    });
    
    registerHandler("goto_ws_7", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_7"); 
        windowStateManager.switchWorkspace(6);
    });
    
    registerHandler("goto_ws_8", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_8"); 
        windowStateManager.switchWorkspace(7);
    });
    
    registerHandler("goto_ws_9", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_9"); 
        windowStateManager.switchWorkspace(8);
    });
    
    registerHandler("goto_ws_10", [&windowStateManager](auto){ 
        LLog::log("execute: goto_ws_10"); 
        windowStateManager.switchWorkspace(9);
    });
}

// TODO: bind more user actions
void ShortcutManager::registerHandler(const std::string& actionName, ShortcutHandler handler){
    std::lock_guard lock(mutex_);
    handlers_[actionName] = std::move(handler);
}

void ShortcutManager::resetHandlers(){
    std::lock_guard lock(mutex_);
    std::map<std::string, ShortcutHandler> empty;
    handlers_.swap(empty);
}

bool ShortcutManager::tryDispatch(const std::string& combo){
    //std::lock_guard lock(mutex_);
    auto it = comboToAction_.find(combo);
    if (it == comboToAction_.end())
        return false;
    const std::string& actionName = it->second;
    auto hit = handlers_.find(actionName);
    if (hit != handlers_.end()){
        hit->second(combo);
    } else {
        //LLog::warning("shortcut action not registered: %s (combo=%s)", actionName.c_str(), combo.c_str());
        return false;
    }
    return true;
}

// Normalize and concatenate
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

// load json
void ShortcutManager::loadFromFile(const std::string& path){
    std::ifstream ifs(path);
    if(!ifs){
        LLog::warning("[ShortcutManager::loadFromFile]: Load shortcut failed: unable to open file: %s", path.c_str());
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
                LLog::debug("[ShortcutManager::loadFromFile]: Shortcut mapping loaded: %s -> %s", combo.c_str(), actionName.c_str());
            }
        }
        comboToAction_.swap(newMap);
    } catch(const std::exception& e){
        LLog::warning("[ShortcutManager::loadFromFile]: Failed to resolve JSON: %s", e.what());
    }
}

void ShortcutManager::startWatcher(const std::string& path){
    watcherThread_ = std::thread([this, path](){
        int fd = inotify_init1(IN_NONBLOCK);
        if(fd < 0){
            LLog::warning("[ShortcutManager::startWatcher]: Failed to initialize inotify ");
            return;
        }
        int wd = inotify_add_watch(fd, path.c_str(), IN_MODIFY | IN_CLOSE_WRITE);
        if(wd < 0){
            LLog::warning("[ShortcutManager::startWatcher]: Unable to add inotify watcher : %s", path.c_str());
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
                            LLog::debug("[ShortcutManager::startWatcher]: Shortcut mapping changed, reloading...");
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