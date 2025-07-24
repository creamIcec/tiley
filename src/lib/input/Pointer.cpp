#include "Pointer.hpp"

#include "src/lib/TileyServer.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

#include <LNamespaces.h>
#include <LLog.h>
#include <LSeat.h>
#include <LPointer.h>
#include <LView.h>
#include <LKeyboard.h>
#include <LSessionLockManager.h>
#include <LDNDSession.h>
#include <LCursor.h>
#include <LClient.h>
#include <LCompositor.h>
#include <LPointerButtonEvent.h>
#include <LToplevelRole.h>

using namespace tiley;

void Pointer::printPointerPressedSurfaceDebugInfo(){

    TileyServer& server = TileyServer::getInstance();
    Surface* surface = static_cast<Surface*>(surfaceAt(cursor()->pos()));

    LLog::log("点击surface: %d", surface);
    LLog::log("Surface所在层次: %d", surface->layer());
    LLog::log("BACKGROUND_VIEW层地址: %d", &server.scene().layers[0]);
    LLog::log("TILED_VIEW层地址: %d", &server.scene().layers[1]);
    LLog::log("FLOATING_VIEW层地址: %d", &server.scene().layers[2]);
    LLog::log("LOCKSCREEN_VIEW层地址: %d", &server.scene().layers[3]);
    LLog::log("OVERLAY_VIEW层地址: %d", &server.scene().layers[4]);
    LLog::log("Surface的view所在层地址: %d", surface->getView()->parent());

    if(surface->toplevel()){
        LLog::log("是窗口");
    }else if(surface->subsurface()){
        LLog::log("是窗口的subsurface");
    }
}


void Pointer::pointerButtonEvent(const LPointerButtonEvent& event){

    TileyServer& server = TileyServer::getInstance();
    Surface* surface = static_cast<Surface*>(surfaceAt(cursor()->pos()));

    //调试: 打印按下鼠标按钮的窗口信息
    //printPointerPressedSurfaceDebugInfo();


    // 修改官方逻辑:
    // 在点击一个窗口的时候: 只改变焦点, 而不改变显示顺序
    // 只有在切换窗口"浮动"和"平铺"模式的时候(包括最开始创建的), 才改变显示顺序

    // TODO: 窗口队列。使用一个队列提交各种改动, 作为缓冲区, 而不在触发相应事件时直接调用

    const bool sessionLocked { sessionLockManager()->state() != LSessionLockManager::Unlocked };
    const bool activeDND { seat()->dnd()->dragging() && seat()->dnd()->triggeringEvent().type() != LEvent::Type::Touch };
 
    // 如果有正在拖动的文件/图标
    if (activeDND)
    {
        // 如果是左键并且是释放
        if (event.state() == LPointerButtonEvent::Released &&
            event.button() == LPointerButtonEvent::Left)
            // 释放拖动的文件
            seat()->dnd()->drop();
 
        // 不管释放没释放, 先重置
        seat()->keyboard()->setFocus(nullptr);
        // 重置焦点
        setFocus(nullptr);
        // 重置正在拖动的surface
        setDraggingSurface(nullptr);
        return;
    }
 
    // 先不分按键, 只要是按键事件就会触发的公共逻辑
    // 如果之前鼠标不在任何一个surface上
    if (!focus())
    {
        // 取得鼠标当前是否已经在一个surface上
        LSurface *surface { surfaceAt(cursor()->pos()) };
 
        // 如果在/进入了一个surface
        if (surface)
        {
            // 如果鼠标现在在的是锁屏程序surface上, 并且屏幕锁定了
            if (sessionLocked && surface->client() != sessionLockManager()->client())
                // 安全第一: 不再处理用户操作
                return;
 
            // 设置成客户端想要的光标样式
            cursor()->setCursor(surface->client()->lastCursorRequest());
            // 键盘聚焦这个surface, 键盘聚焦会发送信号, 并且可能改变鼠标聚焦
            seat()->keyboard()->setFocus(surface);
            // 鼠标也聚焦
            setFocus(surface);
            // 向客户端冒泡该事件, 表明希望客户端自己也处理这个点击事件, 合成器处理的部分已经结束。
            sendButtonEvent(event);
            // 如果鼠标所在的surface不是弹出界面或者他的子surface, 则将现在屏幕上所有显示的弹出菜单关掉
            // 就是点击一个菜单外面关掉菜单
            if (!surface->popup() && !surface->isPopupSubchild())
                seat()->dismissPopups();
        }
        // 如果没有在/离开了一个surface
        else
        {
            // 键盘失焦
            seat()->keyboard()->setFocus(nullptr);
            // 关闭所有弹出菜单
            seat()->dismissPopups();
        }
 
        return;
    }

    // 开始处理具体按键
 
    // 如果不是左键, 合成器不用处理, 直接冒泡给客户端
    // TODO: 平铺式缩放: 按住win+右键拖动
    if (event.button() != LPointerButtonEvent::Left)
    {
        sendButtonEvent(event);
        return;
    }
 
    // 如果是左键, 并且是按下
    if (event.state() == LPointerButtonEvent::Pressed)
    {
        // Keep a ref to continue sending it events after the cursor
        // leaves, if the left button remains pressed
        // 猜测用户可能是要拖动一个surface: 先设置成目前鼠标所在的surface
        // 在其他配套机制中, 如果用户不是在拖动surface(或者正在做一个更高级的操作, 如DND), 会重置这个
        // 关注点分离。我们无需在设置这个之前先检查其他什么, 只需要满足用户可能要拖动surface的需求; 至于是不是真的想拖动,
        // 那就看其他的配套机制怎么判定了。
        setDraggingSurface(focus());
 
        // Most apps close popups when they get keyboard focus,
        // probably because the parent loses it
        // 同样的, 如果目前鼠标所在surface不是一个弹出菜单
        if (!focus()->popup() && !focus()->isPopupSubchild())
        {
            
            // 设置键盘聚焦到目前鼠标的位置
            seat()->keyboard()->setFocus(focus());
 
            // Pointer focus may have changed within LKeyboard::focusChanged()
            // 防止键盘聚焦导致鼠标聚焦也发生了变化, 这里是防御性编程
            if (!focus())
                return;
        }
 
        // 向客户端发送该鼠标左键被按下的事件
        sendButtonEvent(event);
 
        // 如果鼠标所在位置是窗口, 并且这个窗口没有激活
        if (focus()->toplevel() && !focus()->toplevel()->activated()){
            LLog::log("激活窗口: %d", focus()->toplevel());
            // 激活窗口
            focus()->toplevel()->configureState(focus()->toplevel()->pendingConfiguration().state | LToplevelRole::Activated);
        }

        // 继续排除弹出菜单。两个原因: 确实应该在新的窗口激活之后关闭弹出菜单; 
        // 另外也因为激活窗口的动作可能导致弹出菜单, 这里也是防御性编程
        if (!focus()->popup() && !focus()->isPopupSubchild())
            seat()->dismissPopups();
        
        // 提升之前的检查: 如果在上述一系列操作结束后, 鼠标失焦或者目前鼠标所在的就是最顶层的surface,
        // 则事件处理已经结束, 退出
        if (!focus() || focus() == compositor()->surfaces().back())
            return;
 
        // 否则(鼠标所在处有surface并且点击了不是最顶层的surface), 那么按照意图, 提升鼠标所在处的surface
        // 如果聚焦处有父surface, 说明这是个subsurface或者瞬态窗口
        if (focus()->parent())
            // 提升父窗口
            focus()->topmostParent()->raise();
        else
            //否则自己是主窗口, 提升自己
            focus()->raise();
    }
    // 如果是释放左键
    // Left button released
    else
    {
        // 先向客户端冒泡这个事件
        sendButtonEvent(event);
 
        // Stop pointer toplevel resizing sessions
        // 停止所有正在进行的改变窗口大小的操作
        for (auto it = seat()->toplevelResizeSessions().begin(); it != seat()->toplevelResizeSessions().end();)
        {
            if ((*it)->triggeringEvent().type() != LEvent::Type::Touch)
                it = (*it)->stop();
            else
                it++;
        }
 
        // Stop pointer toplevel moving sessions
        // 停止所有正在进行的移动窗口的操作
        for (auto it = seat()->toplevelMoveSessions().begin(); it != seat()->toplevelMoveSessions().end();)
        {
            if ((*it)->triggeringEvent().type() != LEvent::Type::Touch)
                it = (*it)->stop();
            else
                it++;
        }
 
        // We stop sending events to the surface on which the left button was being held down
        // 重置正在拖动的窗口为没有拖动任何窗口。这里原先的注释暗示了: 当不为nullptr时, 会向拖动的目标surface发送事件
        setDraggingSurface(nullptr);
 
        // 如果鼠标没有锁定在一个区域(例如全屏游戏), 并且鼠标所在surface的输入区域不包含当前鼠标所在点
        if (!focus()->pointerConstraintEnabled() && !focus()->inputRegion().containsPoint(cursor()->pos() - focus()->rolePos()))
        {
            // 鼠标失焦, 重置成默认的光标样式, 并且显示
            // 反映用户意图: 当鼠标不在可交互区域时, 即使此时在surface范围内, 也和失焦了基本等价
            setFocus(nullptr);
            cursor()->useDefault();
            cursor()->setVisible(true);
        }
    }


}

void Pointer::pointerMoveEvent(const LPointerMoveEvent& event){
    LPointer::pointerMoveEvent(event);
    //LLog::log("鼠标移动事件");

    //TODO: 是否完善?
}

// pointer: focus(), setFocus() 和 focusChanged() 只是Louvre内置的信号系统, setFocus()并不会将信号发给客户端
// keyboard: setFocus() 会将信号发给客户端。接下来的键盘输入都将定向到focused surface.
void Pointer::focusChanged(){

    LLog::log("聚焦发生改变");
    
    /*

    Surface* surface = static_cast<Surface*>(focus());
    if(!surface){
        return;
    }

    if (!focus()->popup() && !focus()->isPopupSubchild())
    {
        seat()->keyboard()->setFocus(focus());
        // Pointer focus may have changed within LKeyboard::focusChanged()
        if (!focus())
            return;
    }

    if (focus()->toplevel() && !focus()->toplevel()->activated()){
        focus()->toplevel()->configureState(focus()->toplevel()->pendingConfiguration().state | LToplevelRole::Activated);
    }

    */

}