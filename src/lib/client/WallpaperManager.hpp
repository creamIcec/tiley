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
            static WallpaperManager& getInstance();
            struct WallpaperManagerDeleter {
                void operator()(WallpaperManager* p) const { delete p; }
            };

            void initialize();

            void applyToOutput(LOutput* output);

            void selectAndSetNewWallpaper();

            inline bool wallpaperChanged() { return m_wallpaperChanged; }

        private:
            WallpaperManager();
            ~WallpaperManager();

            WallpaperManager(const WallpaperManager&) = delete;
            WallpaperManager& operator=(const WallpaperManager&) = delete;

            void loadConfig();
            void saveConfig();

            static std::unique_ptr<WallpaperManager, WallpaperManagerDeleter> INSTANCE;
            static std::once_flag onceFlag;
            
            void checkDialogStatus();

            // Flag of a wallpaper is recently changed for render thread to refresh their wallpaper
            bool m_wallpaperChanged = false;

            std::string m_configPath;
            std::string m_wallpaperPath;

            Louvre::LTimer m_dialogCheckTimer;
            std::string m_dialogLockFilePath;
            std::string m_dialogResultFilePath;
    };
}

