#include "TileyCompositor.hpp"
#include "LCompositor.h"
#include "LNamespaces.h"
#include "TileyServer.hpp"
#include "src/lib/client/Client.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/input/Keyboard.hpp"
#include "src/lib/input/Pointer.hpp"
#include "src/lib/input/Seat.hpp"
#include "src/lib/output/Output.hpp"
#include "src/lib/surface/Surface.hpp"

#include <LTransform.h>
#include <LLog.h>
#include <LOutputMode.h>
#include <LLauncher.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

using namespace tiley;
using namespace Louvre;

// 静态工具函数: 获取一块屏幕的最佳整数缩放
// TODO: 我们应该允许用户自定义
static UInt32 getIdealScale(LOutput* output){
    // 96这个数字的来源: 一个标准的、不需要缩放的屏幕, 其 DPI 基准被认为是 96。这个数字来自微软Windows的历史设定, 现在被广泛沿用
    float idealScale = output->dpi() / 96.f;

    // 四舍五入
    float finalScale = roundf(idealScale);

    // 保证至少为1
    if (finalScale < 1.f) {
        finalScale = 1.f;
    }

    return finalScale;
}

// 静态工具函数: 获取一块屏幕的最佳显示模式(主要是刷新率)
static LOutputMode* getBestDisplayMode(LOutput* output){
    LOutputMode *bestMode = nullptr;
    const LSize &physicalSize = output->size();
    LLog::debug("[屏幕id: %u] 屏幕尺寸: %dx%d", output->id(), physicalSize.w(), physicalSize.h());

    for (LOutputMode *mode : output->modes()){
        LLog::debug("[屏幕id: %u] 屏幕模式: %dx%d @ %.2f Hz", output->id(), physicalSize.w(), physicalSize.h(), mode->refreshRate() / 1000.f);
        if (mode->sizeB() == physicalSize){
            if (!bestMode || mode->refreshRate() > bestMode->refreshRate()){
                bestMode = mode;
            }
        }
    }

    if (bestMode){
        LLog::debug("[屏幕id: %u] 使用显示模式: %dx%d @ %.2f Hz",
                   output->id(), bestMode->sizeB().w(), bestMode->sizeB().h(), bestMode->refreshRate() / 1000.f);
        return bestMode;
    }

    return nullptr;
}

void TileyCompositor::initialized(){
    setenv("WAYLAND_DISPLAY", getenv("LOUVRE_WAYLAND_DISPLAY"), 1);

    TileyServer& server = TileyServer::getInstance();

    // 配置输入设备
    server.seat()->configureInputDevices();

    int32_t totalWidth {0};

    // 配置显示器
    // Seat中的显示器是所有的(包括已配置和未配置的, 此时刚启动肯定都没有配置)
    // 如果要只获得已经配置的显示器, 使用LCompositor::seat(1)
    for(LOutput* output : server.seat()->outputs()){
        LOutputMode* bestMode = getBestDisplayMode(output);
        /*
        if(bestMode){
            output->setMode(bestMode);
        }else{
            LLog::warning("[屏幕id: %u] 警告: 未找到合适的显示模式, 使用屏幕默认模式");
        }
        */
        output->setScale(getIdealScale(output));
        // 正常显示, 不旋转也不翻转
        output->setTransform(LTransform::Normal);
        // 按照从左到右配置显示器, 每一个显示器都在上一个的右边
        // TODO: 是否添加一种机制, 让用户自定义显示器位置, 不一定从左到右, 也可以从右到左或者"田"字形等
        output->setPos(LPoint(totalWidth, 0));
        totalWidth += output->size().w();
        addOutput(output);
        // 解锁渲染线程, 使屏幕恢复正常渲染。当我们要进行某些操作之前, 需要先上锁, 避免出现不同步的现象。(TODO: 如何?)
        output->repaint();
    }

    // TODO: 从配置判断是否启用waybar
    // TODO: 启动waybar带参数
    // TODO: hyprland: 配置文件exec-once
    bool waybar = true;
    if(waybar){
        Louvre::LLauncher::launch("waybar");
    }

    const std::vector<std::string>& cmds = TileyServer::getInstance().getStartupCMD();
    for(auto cmd : cmds){
        LLog::debug("启动指令: %s", cmd.c_str());
        Louvre::LLauncher::launch(cmd);
    }
}

void TileyCompositor::uninitialized(){
    // 所有组件(Seat, Keyboard, Pointer等等), 客户端窗口, 后端此时仍然有效
    // 该方法在销毁之前调用, 在这里销毁所有相关资源。
}

// 各种协议对象的继承关系
// LObject -> LFactoryObject -> XXXObject(Output, Client, etc.)
// 如果我们这里返回了空指针, 则Wayland会使用LFactoryObject这个公共父类, 但该父类没有任何实际功能, 只是确保程序默认可以处理请求而不静默崩溃.
LFactoryObject* TileyCompositor::createObjectRequest(LFactoryObject::Type objectType, const void* params){

    if (objectType == LFactoryObject::Type::LOutput){
        return new Output(params);
    }

    if (objectType == LFactoryObject::Type::LClient){
        // TODO: 实现客户端对象(原来的toplevel结构体)
        return new Client(params);
    }

    if (objectType == LFactoryObject::Type::LSurface){
        return new Surface(params);
    }
    
    // 以下的"角色"指代的就是不同surface的功能了。
    // 单纯的surface只是一块表面, 对于后端而言没有功能, 需要分配一个角色(是窗口? 是锁屏? 是弹出界面? 是光标的图标?), 后端才知道如何操作它
    // 这是之前我们在使用wlroots时没注意的, "toplevel"被我们直接当成了"窗口"本身, 但实际上不是, "toplevel"是一套抽象的控制方法, 
    // 用于控制类型为"窗口"的surface
    if (objectType == LFactoryObject::Type::LToplevelRole){
        return new ToplevelRole(params);
    }
    if (objectType == LFactoryObject::Type::LKeyboard){
        return new Keyboard(params);
    }
    if (objectType == LFactoryObject::Type::LPointer){
        return new Pointer(params);
    }
    if (objectType == LFactoryObject::Type::LSeat){
        // TODO: 实现Seat, 和wlroots一样, seat用于定义一个完整的用户操作环境("一个座位"), 至少包含一个键盘, 一个显示器
        return new Seat(params);
    }
    // TODO: 随着代码的迁移越来越完善, 下方需要继承的类逐渐完成继承, 就将这行逐渐往下移动, 直到所有需要继承的类都完成继承后, 标志着我们大功告成。
    return LCompositor::createObjectRequest(objectType, params);

    if (objectType == LFactoryObject::Type::LSubsurfaceRole){
        // TODO: 实现subsurface角色
    }
    if (objectType == LFactoryObject::Type::LSessionLockRole){
        // TODO: 实现锁屏角色
    }
    if (objectType == LFactoryObject::Type::LCursorRole){
        // TODO: 实现光标图标角色
    }
    if (objectType == LFactoryObject::Type::LDNDIconRole){
        // TODO: 实现拖拽图标角色。拖拽图标就是我们用鼠标拖动一个文件到另一个窗口时跟随鼠标显示的那个文件图标
    }


    if (objectType == LFactoryObject::Type::LTouch){
        // TODO: 实现触摸支持
    }
    if (objectType == LFactoryObject::Type::LClipboard){
        // TODO: 实现剪贴板(这个应该由合成器管理嘛?)
    }
    if (objectType == LFactoryObject::Type::LDND){
        // TODO: 实现拖拽功能(和上面的LDNDIconRole对应)
    }
    if(objectType == LFactoryObject::Type::LSessionLockManager){
        // TODO: 实现锁屏管理
    }

    // 如果找不到对应的具体对象, 则返回nullptr, 这应该告诉库中的调用者或者我们的代码使用默认的LFactoryObject.
    return nullptr;
}

void TileyCompositor::onAnticipatedObjectDestruction(LFactoryObject* object){
    if(object->factoryObjectType() == LFactoryObject::Type::LClient){
        // 正常销毁一个client
    }
}

bool TileyCompositor::createGlobalsRequest(){
    // 创建所有支持的功能
    return LCompositor::createGlobalsRequest();
}


// TODO: 这是什么?
// 用于过滤Global的吗? 可能是逐个调用, 返回true表示使用, false表示不使用
bool TileyCompositor::globalsFilter(LClient* client, LGlobal* global){
    return true; //TODO
}

void TileyCompositor::printToplevelSurfaceLinklist(){
    LLog::log("***********打印窗口surface关系(后面的显示更靠上)***********");
    for(LSurface* surface : surfaces()){
        if(surface->toplevel()){
            if(surface->nextSurface()){
                LLog::log("%d<-", surface);
            }else{
                LLog::log("%d", surface);
            }
        }
    }
    LLog::log("************************************************");
};