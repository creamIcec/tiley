#pragma once

#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>

namespace tiley {

    using ShortcutHandler = std::function<void(const std::string& combo)>;
    
    // Shortcut mapping management: load / hot reload
    class ShortcutManager {
        public:

            static ShortcutManager& getInstance();
            // initialize manager: load instantly
            void init(const std::string& jsonPath);

            // register handler for an action
            void registerHandler(const std::string& actionName, ShortcutHandler handler);

            // try trigger combo
            bool tryDispatch(const std::string& combo);

            // normalize raw combo, for instance "Ctrl+Shift+T" -> "ctrl+shift+t"
            static std::string normalizeCombo(const std::string& raw);

            void initializeHandlers();
            void resetHandlers();

        private:
            ShortcutManager();
            ~ShortcutManager();

            ShortcutManager(const ShortcutManager&) = delete;
            ShortcutManager& operator=(const ShortcutManager&) = delete;

            struct ShortcutManagerDeleter {
                void operator()(ShortcutManager* p) const {
                    delete p;
                }
            };

            friend struct ShortcutManagerDeleter;
            static std::unique_ptr<ShortcutManager, ShortcutManagerDeleter> INSTANCE;
            static std::once_flag onceFlag;

            void registerWorkspacesHandler();

            void loadFromFile(const std::string& path);
            void startWatcher(const std::string& path);

            std::map<std::string, std::string> comboToAction_;
            std::map<std::string, ShortcutHandler> handlers_;

            std::mutex mutex_;
            std::atomic<bool> stopWatcher_;
            std::thread watcherThread_;
        };
}
