#pragma once

#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>

namespace tiley {

    using ShortcutHandler = std::function<void(const std::string& combo)>;
    /// 负责快捷键映射加载 / 规范化 / 热重载 / 分发
    class ShortcutManager {
        public:

            static ShortcutManager& getInstance();
            /// 初始化配置文件路径（立即 load 并启动 watcher 线程）
            void init(const std::string& jsonPath);

            /// 注册某个 action 的 handler
            void registerHandler(const std::string& actionName, ShortcutHandler handler);

            /// 试着调度 combo（规范化后的），命中则执行并返回 true
            bool tryDispatch(const std::string& combo);

            /// 规范化 raw combo，比如 "Ctrl+Shift+T" -> "ctrl+shift+t"
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

            void loadFromFile(const std::string& path);
            void startWatcher(const std::string& path);

            //两个键值对。
            std::map<std::string, std::string> comboToAction_;
            std::map<std::string, ShortcutHandler> handlers_;

            //
            std::mutex mutex_;
            std::atomic<bool> stopWatcher_;
            //单开一个线程
            std::thread watcherThread_;
        };
} // namespace tiley
