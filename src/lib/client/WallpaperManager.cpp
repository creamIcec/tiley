#include "WallpaperManager.hpp"
#include <LCompositor.h>
#include <LOutput.h>
#include <LOpenGL.h>
#include <LLog.h>
#include <LTexture.h>
#include <LLauncher.h>
#include <fstream>
#include <sys/wait.h>

#include "src/lib/Utils.hpp"
#include "src/lib/output/Output.hpp"

using namespace tiley;

std::unique_ptr<WallpaperManager, WallpaperManager::WallpaperManagerDeleter> WallpaperManager::INSTANCE = nullptr;
std::once_flag WallpaperManager::onceFlag;

WallpaperManager& WallpaperManager::getInstance() {
    std::call_once(onceFlag, []() {
        INSTANCE.reset(new WallpaperManager());
    });
    return *INSTANCE;
}

WallpaperManager::WallpaperManager() {
    // start timer when the user attempts to change wallpaper
    m_dialogCheckTimer.setCallback([this](Louvre::LTimer*){
        this->checkDialogStatus();
    });
}

WallpaperManager::~WallpaperManager() {
    // stop the timer and release locked files
    if (m_dialogCheckTimer.running()) {
        m_dialogCheckTimer.stop();
        unlink(m_dialogLockFilePath.c_str());
        unlink(m_dialogResultFilePath.c_str());
    }
}

void WallpaperManager::initialize() {
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        LLog::fatal("[WallpaperManager::initialize]: Unable to locate HOME directory, is this an abnormal linux desktop environment?");
        return;
    }

    // build path for wallpaper configuration
    std::filesystem::path configPath = homeDir;
    configPath /= ".config";
    configPath /= "tiley";
    
    std::filesystem::create_directories(configPath);
    
    configPath /= "wallpaper.conf";
    m_configPath = configPath;
    loadConfig();
}

void WallpaperManager::checkDialogStatus() {
    if (access(m_dialogLockFilePath.c_str(), F_OK) == -1) {

        LLog::log("[WallpaperManager::checkDialogStatus]: dialog is closed");

        m_dialogCheckTimer.stop();

        // read selected wallpaper path from file
        std::ifstream resultFile(m_dialogResultFilePath);
        std::string selectedPath;
        if (resultFile.is_open() && std::getline(resultFile, selectedPath)) {
            selectedPath.erase(selectedPath.find_last_not_of("\n\r") + 1);

            if (!selectedPath.empty()) {
                LLog::log("[WallpaperManager::checkDialogStatus]: new wallpaper: %s", selectedPath.c_str());
                m_wallpaperPath = selectedPath;
                m_wallpaperChanged = true;
                for (auto* output : Louvre::compositor()->outputs()) {
                    LLog::log("[WallpaperManager::checkDialogStatus]: attempt to change wallpaper of monitor id %s。", output->name());
                    output->repaint();
                }
                saveConfig();
            } else {
                LLog::log("[WallpaperManager::checkDialogStatus]: the user cancelled selection");
            }
        }

        unlink(m_dialogResultFilePath.c_str());
        return;
    }

    m_dialogCheckTimer.start(500);
}

void WallpaperManager::loadConfig() {
    std::ifstream configFile(m_configPath);
    if (configFile.is_open() && std::getline(configFile, m_wallpaperPath) && !m_wallpaperPath.empty()) {
        LLog::log("[WallpaperManager::loadConfig]: load wallpaper from path %s: %s", m_configPath.c_str(), m_wallpaperPath.c_str());
    } else {
        m_wallpaperPath = getDefaultWallpaperPath();
        LLog::warning("[WallpaperManager::loadConfig]: unable to load wallaper path: %s, fallback to default wallpaper", m_wallpaperPath.c_str());
        saveConfig();
    }
}

void WallpaperManager::saveConfig() {
    std::ofstream configFile(m_configPath);
    if (configFile.is_open()) {
        configFile << m_wallpaperPath;
        LLog::log("[WallpaperManager::saveConfig]: save wallpaper config to %s", m_configPath.c_str());
    } else {
        LLog::error("[WallpaperManager::saveConfig]: unable to open config file for writing, path: %s", m_configPath.c_str());
    }
}

void WallpaperManager::selectAndSetNewWallpaper() {

    if (m_dialogCheckTimer.running()) {
        LLog::warning("[WallpaperManager::selectAndSetNewWallpaper]: one instance of selector is already in running");
        return;
    }

    // TODO: hardcoded paths
    m_dialogLockFilePath = "/tmp/tiley_dialog.lock";
    m_dialogResultFilePath = "/tmp/tiley_dialog.result";

    std::ofstream lockFile(m_dialogLockFilePath);
    lockFile.close();

    // TODO: hardcoded socket path
    const char* socketName = "wayland-2";

    std::string cmd = "WAYLAND_DISPLAY=" + std::string(socketName) +
                      " kdialog --getopenfilename . \"*.png *.jpg *.jpeg\"" +
                      " > " + m_dialogResultFilePath + "; " +
                      "rm " + m_dialogLockFilePath;

    LLog::debug("[WallpaperManager::selectAndSetNewWallpaper] execute wallpaer selection command: %s", cmd.c_str());
    Louvre::LLauncher::launch(cmd);

    m_dialogCheckTimer.start(500);
    LLog::log("[WallpaperManager::selectAndSetNewWallpaper]: timer started, check state...");
}

void WallpaperManager::applyToOutput(Louvre::LOutput* _output) {

    LLog::log("[WallpaperManager::applyToOutput]: attemp to apply wallpaper to output");

    if (!_output){
        LLog::warning("[WallpaperManager::applyToOutout]: wallpaper path does not exists, stop applying wallpaper");
        return;
    }

    auto output = static_cast<Output*>(_output);
    auto& wallpaperView = output->wallpaperView();

    if (wallpaperView.texture() && wallpaperView.texture()->sizeB() == output->sizeB() && !m_wallpaperChanged) {
        wallpaperView.setBufferScale(output->scale());
        wallpaperView.setPos(output->pos());
        return;
    }

    if (wallpaperView.texture()) {
        delete wallpaperView.texture();
    }

    Louvre::LTexture* originalWallpaper = Louvre::LOpenGL::loadTexture(m_wallpaperPath);

    if (!originalWallpaper) {
        LLog::error("[WallpaperManager::applyToOutput] Unable to load wallpaper, path: %s", m_wallpaperPath.c_str());
        originalWallpaper = Louvre::LOpenGL::loadTexture(getDefaultWallpaperPath());
        if (!originalWallpaper) return;
    }

    const Louvre::LSize& originalSizeB = originalWallpaper->sizeB();
    const Louvre::LSize& outputSizeB = output->sizeB();
    Louvre::LRect srcRect{0};
    const Float32 ratio = (Float32)outputSizeB.h() / (Float32)originalSizeB.h();
    const Float32 scaledWidth = originalSizeB.w() * ratio;

    if (scaledWidth >= outputSizeB.w()) {
        srcRect.setW(originalSizeB.w());
        srcRect.setH((Float32)outputSizeB.h() * (Float32)originalSizeB.w() / scaledWidth);
        srcRect.setY(((Float32)originalSizeB.h() - (Float32)srcRect.h()) / 2.0f);
    } else {
        srcRect.setH(originalSizeB.h());
        srcRect.setW((Float32)outputSizeB.w() * (Float32)originalSizeB.h() / (Float32)outputSizeB.h());
        srcRect.setX(((Float32)originalSizeB.w() - (Float32)srcRect.w()) / 2.0f);
    }

    wallpaperView.setTexture(originalWallpaper->copy(outputSizeB, srcRect));
    wallpaperView.setBufferScale(output->scale());
    delete originalWallpaper;
    wallpaperView.setPos(output->pos());

    LLog::log("[WallpaperManager::applyToOutput]: Wallpaper reapplied successfully");
    m_wallpaperChanged = false;
}