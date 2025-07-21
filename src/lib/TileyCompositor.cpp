#include "TileyCompositor.hpp"
#include "LCompositor.h"
#include "TileyServer.hpp"
#include "src/lib/client/Client.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/input/Keyboard.hpp"
#include "src/lib/output/Output.hpp"
#include "src/lib/surface/Surface.hpp"

#include <LTransform.h>

#include <cstdint>
#include <cstdlib>

using namespace tiley;
using namespace Louvre;


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
        // 根据DPI处理屏幕缩放比例: 当DPI >= 200 时自动设置成2倍放大
        // TODO: 我们应该允许用户自定义
        output->setScale(output->dpi() >= 200 ? 2.f : 1.f);
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

    //TODO: 启动顶栏

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
        // TODO: 实现Surface对象
        return new Surface(params);
    }
    // 以下的"角色"指代的就是不同surface的功能了。
    // 单纯的surface只是一块表面, 对于后端而言没有功能, 需要分配一个角色(是窗口? 是锁屏? 是弹出界面? 是光标的图标?), 后端才知道如何操作它
    // 这是之前我们在使用wlroots时没注意的, "toplevel"被我们直接当成了"窗口"本身, 但实际上不是, "toplevel"是一套抽象的控制方法, 
    // 用于控制类型为"窗口"的surface
    if (objectType == LFactoryObject::Type::LToplevelRole){
        // TODO: 实现toplevel角色(注意: 和以前的"toplevel"结构体不是一个东西, 之前的应该概念有误, toplevel不是"窗口", 而是一个surface的角色)
        return new ToplevelRole(params);
    }
    if (objectType == LFactoryObject::Type::LKeyboard){
        // TODO: 实现键盘支持
        return new Keyboard(params);
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
    if (objectType == LFactoryObject::Type::LSeat){
        // TODO: 实现Seat, 和wlroots一样, seat用于定义一个完整的用户操作环境("一个座位"), 至少包含一个键盘, 一个显示器
    }
    if (objectType == LFactoryObject::Type::LPointer){
        // TODO: 实现鼠标设备支持
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