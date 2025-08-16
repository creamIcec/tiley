#pragma once

#include "LNamespaces.h"
#include "LTimer.h"
#include <string>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <wayland-server-core.h>

#include <LOutput.h>

namespace tiley {

    using namespace Louvre;

    class WallpaperManager {
        public:
            // 单例模式
            static WallpaperManager& getInstance();
            struct WallpaperManagerDeleter {
                void operator()(WallpaperManager* p) const { delete p; }
            };
            // 初始化
            void initialize();

            // 应用壁纸到输出
            void applyToOutput(LOutput* output);

            // 弹出文件选择框
            void selectAndSetNewWallpaper();

            inline bool wallpaperChanged() { return m_wallpaperChanged; }

        private:
            WallpaperManager();
            ~WallpaperManager();

            // 禁止拷贝
            WallpaperManager(const WallpaperManager&) = delete;
            WallpaperManager& operator=(const WallpaperManager&) = delete;

            void loadConfig();
            void saveConfig();

            static std::unique_ptr<WallpaperManager, WallpaperManagerDeleter> INSTANCE;
            static std::once_flag onceFlag;
            
            // 定时器回调函数
            void checkDialogStatus();

            // 壁纸更新时置为true
            bool m_wallpaperChanged = false;

            std::string m_configPath;
            std::string m_wallpaperPath;

            // 用 LTimer 和锁文件路径代替 PID 和信号源
            Louvre::LTimer m_dialogCheckTimer;
            std::string m_dialogLockFilePath;
            std::string m_dialogResultFilePath;
    };
}

