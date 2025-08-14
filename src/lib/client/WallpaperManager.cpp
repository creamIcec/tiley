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

// 单例实现
std::unique_ptr<WallpaperManager, WallpaperManager::WallpaperManagerDeleter> WallpaperManager::INSTANCE = nullptr;
std::once_flag WallpaperManager::onceFlag;

WallpaperManager& WallpaperManager::getInstance() {
    std::call_once(onceFlag, []() {
        INSTANCE.reset(new WallpaperManager());
    });
    return *INSTANCE;
}

// 构造与析构
WallpaperManager::WallpaperManager() {
    // 设置定时器的回调函数，它会在每次触发时调用 checkDialogStatus
    m_dialogCheckTimer.setCallback([this](Louvre::LTimer*){
        this->checkDialogStatus();
    });
}

WallpaperManager::~WallpaperManager() {
    // 析构时确保定时器停止，并清理可能残留的文件
    if (m_dialogCheckTimer.running()) {
        m_dialogCheckTimer.stop();
        unlink(m_dialogLockFilePath.c_str());
        unlink(m_dialogResultFilePath.c_str());
    }
}

void WallpaperManager::initialize() {
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        LLog::fatal("无法获取 HOME 目录，无法确定配置文件路径。");
        return;
    }
    // 2. 构建配置文件路径
    std::filesystem::path configPath = homeDir;
    configPath /= ".config";
    configPath /= "tiley";
    // 3. 确保目录存在
    std::filesystem::create_directories(configPath);
    // 4. 得到最终的完整路径
    configPath /= "wallpaper.conf";
    m_configPath = configPath;
    loadConfig();
}

void WallpaperManager::checkDialogStatus() {
    // 检查“锁文件”是否存在
    if (access(m_dialogLockFilePath.c_str(), F_OK) == -1) {
        // 文件不存在，说明 kdialog 已经结束
        LLog::log("[WallpaperManager] 锁文件消失，对话框已关闭。");

        // 1. 停止定时器
        m_dialogCheckTimer.stop();

        // 2. 读取结果文件
        std::ifstream resultFile(m_dialogResultFilePath);
        std::string selectedPath;
        if (resultFile.is_open() && std::getline(resultFile, selectedPath)) {
            selectedPath.erase(selectedPath.find_last_not_of("\n\r") + 1);

            if (!selectedPath.empty()) {
                LLog::log("[WallpaperManager] 选择的新壁纸: %s", selectedPath.c_str());
                m_wallpaperPath = selectedPath;
                m_wallpaperChanged = true;
                for (auto* output : Louvre::compositor()->outputs()) {
                    LLog::log("[WallpaperManager] 请求为显示器 %s 更新壁纸。", output->name());
                    output->repaint();
                }
                saveConfig();
            } else {
                LLog::log("[WallpaperManager] 用户取消了选择。");
            }
        }

        // 3. 清理结果文件
        unlink(m_dialogResultFilePath.c_str());
        return;
    }
    m_dialogCheckTimer.start(500);
}

void WallpaperManager::loadConfig() {
    std::ifstream configFile(m_configPath);
    if (configFile.is_open() && std::getline(configFile, m_wallpaperPath) && !m_wallpaperPath.empty()) {
        LLog::log("[WallpaperManager] 从 %s 加载壁纸路径: %s", m_configPath.c_str(), m_wallpaperPath.c_str());
    } else {
        // 【修改】使用我们的新函数获取默认路径
        m_wallpaperPath = getDefaultWallpaperPath();
        LLog::warning("[WallpaperManager] 无法加载配置或配置为空，使用默认壁纸: %s", m_wallpaperPath.c_str());
        saveConfig();
    }
}

void WallpaperManager::saveConfig() {
    std::ofstream configFile(m_configPath);
    if (configFile.is_open()) {
        configFile << m_wallpaperPath;
        LLog::log("[WallpaperManager] 保存壁纸路径到 %s", m_configPath.c_str());
    } else {
        LLog::error("[WallpaperManager] 无法打开配置文件进行写入: %s", m_configPath.c_str());
    }
}

void WallpaperManager::selectAndSetNewWallpaper() {
    // 通过检查定时器是否在运行来判断是否已有对话框
    if (m_dialogCheckTimer.running()) {
        LLog::warning("[WallpaperManager] 已经有一个文件选择器在运行。");
        return;
    }

    // 定义文件路径
    m_dialogLockFilePath = "/tmp/tiley_dialog.lock";
    m_dialogResultFilePath = "/tmp/tiley_dialog.result";

    // 创建锁文件, 表示"我正忙"
    std::ofstream lockFile(m_dialogLockFilePath);
    lockFile.close();

    // 暂时硬编码 socket name
    const char* socketName = "wayland-2";

    // 1. 启动 kdialog, 将结果输出到 result 文件
    // 2. 确保 kdialog 结束后, 无论成功或取消,都执行下一步
    // 3. 删除 lock 文件
    std::string cmd = "WAYLAND_DISPLAY=" + std::string(socketName) +
                      " kdialog --getopenfilename . \"*.png *.jpg *.jpeg\"" +
                      " > " + m_dialogResultFilePath + "; " +
                      "rm " + m_dialogLockFilePath;

    LLog::debug("[WallpaperManager] 执行命令: %s", cmd.c_str());
    Louvre::LLauncher::launch(cmd);

    // 启动定时器，每 500 毫秒检查一次状态
    m_dialogCheckTimer.start(500);
    LLog::log("[WallpaperManager] 文件选择器已启动，开始轮询状态...");
}

void WallpaperManager::applyToOutput(Louvre::LOutput* _output) {

    LLog::log("应用壁纸到显示器。");

    if (!_output){
        LLog::warning("显示器不存在, 无法更新壁纸。");
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
        LLog::error("[WallpaperManager] 无法加载壁纸: %s。", m_wallpaperPath.c_str());
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

    LLog::log("[WallpaperManager] 成功刷新壁纸。");
    m_wallpaperChanged = false;
}