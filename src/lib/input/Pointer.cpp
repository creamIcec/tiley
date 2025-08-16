#include "Pointer.hpp"

#include "LEdge.h"
#include "src/lib/TileyServer.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "src/lib/client/ToplevelRole.hpp"
#include "src/lib/core/UserAction.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"
#include "src/lib/core/Container.hpp"

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
#include <xkbcommon/xkbcommon.h>

#include <LToplevelMoveSession.h>

#include <private/LCompositorPrivate.h>

using namespace tiley;

LSurface* Pointer::surfaceAtWithFilter(const LPoint& point, const std::function<bool (LSurface*)> &filter){
    retry:
    // 保留 Louvre 的健壮性设计，防止在遍历时列表被修改
    compositor()->imp()->surfacesListChanged = false;

    // 从 rbegin() 到 rend() 是逆序遍历，即从最顶层的窗口开始
    for (auto it = compositor()->surfaces().rbegin(); it != compositor()->surfaces().rend(); ++it)
    {
        LSurface *s = *it;

        // 基础检查：窗口必须是可见的
        if (s->mapped() && !s->minimized())
        {
            // 几何检查：鼠标指针是否在该窗口的输入区域内？
            if (s->inputRegion().containsPoint(point - s->rolePos()))
            {
                // 关键一步：在确认几何位置匹配后，执行我们自定义的 lambda 过滤器
                if (filter(s))
                {
                    // 如果过滤器返回 true，说明我们找到了完美的匹配！
                    return s;
                }
                // 如果过滤器返回 false，我们什么也不做，继续循环，
                // 查找这个窗口下面的、同样在该坐标点、且可能满足条件的窗口。
            }
        }

        // 再次检查列表是否被修改，如果被修改，就从头再来一次
        if (compositor()->imp()->surfacesListChanged)
            goto retry;
    }

    // 遍历完所有窗口都没有找到匹配
    return nullptr;
}

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

    bool compositorProceed = false;

    // 如果存在键盘
    if(seat()->keyboard()){
        compositorProceed = processCompositorKeybind(event);
    }

    // 如果合成器处理了按键, 则不继续处理(不发给客户端, 不进行其他处理等等)
    if(!compositorProceed){
        processPointerButtonEvent(event);
    }

}

void Pointer::processPointerButtonEvent(const LPointerButtonEvent& event){
    // 修改官方逻辑:
    // 在点击一个窗口的时候: 只改变焦点, 而不改变显示顺序
    // 只有在切换窗口"浮动"和"平铺"模式的时候(包括最开始创建的), 才改变显示顺序

    // TODO: 窗口队列。使用一个队列提交各种改动, 作为缓冲区, 而不在触发相应事件时直接调用

    // TODO: 下面的代码大部分由父类复制而来, 尚未完成修改。

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
        // 鼠标按下
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
            LLog::debug("激活窗口: %d", focus()->toplevel());
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

// 处理合成器范围的按键
bool Pointer::processCompositorKeybind(const LPointerButtonEvent& event){

    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();

    // TODO: 下面的代码大部分由父类复制而来, 尚未完成修改。

    // 首先检查是否已经正在处理一个合成器的keybind
    if(!focus() || !focus()->toplevel()){
        LLog::debug("没有目标或目标不是窗口, 停止处理合成器事件");
        return false;
    }

    ToplevelRole* window = static_cast<ToplevelRole*>(focus()->toplevel());

    // 调整窗口大小
    bool isResizingWindow = !seat()->toplevelResizeSessions().empty();
    // 如果alt被按下 TODO: 分成嵌套运行和tty运行两种情况, 嵌套运行下主要用于调试, 使用alt键; tty运行使用super键
    bool isModifierPressedDown = TileyServer::getInstance().is_compositor_modifier_down;

    // 如果是右键+按下+之前没有调整窗口
    if(event.button() == Louvre::LPointerButtonEvent::Right &&
       event.state() == Louvre::LPointerButtonEvent::Pressed &&
       isModifierPressedDown &&
       !isResizingWindow){
        LLog::debug("调整窗口大小...");
        const LPoint &mousePos = cursor()->pos(); // 鼠标在屏幕上的绝对位置
        const LPoint &winPos = window->surface()->pos(); // 窗口在屏幕上的绝对位置
        const LSize &winSize = window->surface()->size(); // 窗口的大小

        // 计算鼠标在窗口内的相对位置
        float relativeX = (float)(mousePos.x() - winPos.x()) / (float)winSize.w();
        float relativeY = (float)(mousePos.y() - winPos.y()) / (float)winSize.h();

        // 用一个四位二进制数进行位编码编码: 0000 <-> 上下左右
        // 判断应该拖动哪个边界
        LBitset<LEdge> edge = LEdgeNone;

        // 判断 Y 轴位置
        if (relativeY < 0.5f) {
            edge |= LEdgeTop;
        } else if (relativeY >= 0.5f) {
            edge |= LEdgeBottom;
        }

        // 判断 X 轴位置
        if (relativeX < 0.5f) {
            edge |= LEdgeLeft;
        } else if (relativeX >= 0.5f) {
            edge |= LEdgeRight;
        }

        // 对所有窗口的处理
        if (edge != LEdgeNone){
            manager.setupResizeSession(window, edge, cursor()->pos());
            window->startResizeRequest(event, edge);
        }

        if(!window->container){
            return true;
        }

        return true;

    }

    // 如果是右键+按下+之前正在调整窗口
    if(event.button() == Louvre::LPointerButtonEvent::Right &&
       event.state() == Louvre::LPointerButtonEvent::Released &&
       isResizingWindow){
        LLog::debug("停止调整窗口大小...");
        stopResizeSession(true);
        return true;
    }

    // 移动窗口
    bool isMovingWindow = !seat()->toplevelMoveSessions().empty();

    // 如果是左键+按下+之前没有移动窗口
    if(event.button() == Louvre::LPointerButtonEvent::Left &&
        event.state() == Louvre::LPointerButtonEvent::Pressed &&
        isModifierPressedDown &&
        !isMovingWindow){
        
        LLog::debug("移动窗口");
        // 对所有类型的窗口的处理
        window->startMoveRequest(event);

        if(!window->container){
            return true;
        }

        // 对平铺层窗口的处理: 如果是正常窗口并且没有堆叠
        if(window->type == NORMAL && !manager.isStackedWindow(window)){
            // 从管理器分离
            Container* detachedContainer = manager.detachTile(window, MOVING);
            if(detachedContainer){
                // 如果分离成功, 重新组织并重新布局
                manager.reapplyWindowState(window);
                manager.recalculate();
            }
        }

        return true;
    }

    // 如果是左键+释放+正在移动窗口
    if(event.button() == Louvre::LPointerButtonEvent::Left &&
       event.state() == Louvre::LPointerButtonEvent::Released &&
       isMovingWindow){

        stopMoveSession(true);
        return true;
            
    }

    // 问题: 我们没有去掉某些应用的标题栏(即使设置了serverside的装饰, 比如gimp), 所以这些应用仍然可以被只有左键移动
    // TODO: 怎么防止这个问题?

    return false;
}

void Pointer::pointerMoveEvent(const LPointerMoveEvent& event){
    //LLog::debug("鼠标移动事件");

    // 首先移动光标位置, 确保后续的操作是更新过的位置
    // Update the cursor position
    cursor()->move(event.delta().x(), event.delta().y());
 
    // 指针是否被范围限制?
    bool pointerConstrained { false };
 
    // 如果之前鼠标在一个surface上
    if (focus())
    {
        // 获取那个surface的角色坐标(例如一个弹出菜单的话, 就是相对他的父窗口的位置)
        LPointF fpos { focus()->rolePos() };
        
        // 如果目标surface有鼠标范围限制, 就准备判断是否要限制鼠标范围
        // Attempt to enable the pointer constraint mode if the cursor is within the constrained region.
        if (focus()->pointerConstraintMode() != LSurface::PointerConstraintMode::Free)
        {
            // 鼠标在目标surface的范围限制区内, 就确认需要限制鼠标范围
            if (focus()->pointerConstraintRegion().containsPoint(cursor()->pos() - fpos))
                focus()->enablePointerConstraint(true);
        }
 
        // 如果启用了范围限制
        if (focus()->pointerConstraintEnabled())
        {
            // 如果是固定位置: "囚禁", 只能呆在一个固定位置
            if (focus()->pointerConstraintMode() == LSurface::PointerConstraintMode::Lock)
            {
                // 获取锁定的位置的横坐标(纵坐标呢? 是不是只要横坐标>=0就说明被锁定了? (意即: 横纵坐标必须同时为正/为负))
                if (focus()->lockedPointerPosHint().x() >= 0.f)
                    cursor()->setPos(fpos + focus()->lockedPointerPosHint());
                else   // 如果客户端没有设置锁定在哪儿(lockedPointerPosHine()为-1的情况)
                {
                    // 弹回原位
                    cursor()->move(-event.delta().x(), -event.delta().y());
 
                    // 送回限制区域离鼠标最近的点(相对位置), 应该是一种额外保障
                    const LPointF closestPoint {
                        focus()->pointerConstraintRegion().closestPointFrom(cursor()->pos() - fpos)
                    };
 
                    cursor()->setPos(fpos + closestPoint);
                }
            }
            else /* Confined */   // 处理仅仅是限制的情况: "软禁", 可以在一定范围内活动
            {
                // 送回限制区域离鼠标最近的点(相对位置)
                const LPointF closestPoint {
                    focus()->pointerConstraintRegion().closestPointFrom(cursor()->pos() - fpos)
                };
 
                cursor()->setPos(fpos + closestPoint);
            }
            
            // 设置被限制的flag, 便于后续使用
            pointerConstrained = true;
        }
    }
 
    // 触发重绘
    // Schedule repaint on outputs that intersect with the cursor where hardware composition is not supported.
    cursor()->repaintOutputs(true);
 
    const bool sessionLocked { compositor()->sessionLockManager()->state() != LSessionLockManager::Unlocked };
    const bool activeDND { seat()->dnd()->dragging() && seat()->dnd()->triggeringEvent().type() != LEvent::Type::Touch };
 
    // 和PointerButtonEvent里面的相同, 处理正在拖拽文件的情况
    if (activeDND)
    {
        // 这里是先处理有图标的拖拽(也就是和那个图标的surface相关的东西, 有些拖拽没有图标的就不用管他的surface)
        if (seat()->dnd()->icon())
        {
            seat()->dnd()->icon()->surface()->setPos(cursor()->pos());
            seat()->dnd()->icon()->surface()->repaintOutputs();
            cursor()->setCursor(seat()->dnd()->icon()->surface()->client()->lastCursorRequest());
        }
 
        // 正在拖拽文件, 所以其他的都忽略(全部重置), 这会儿只管拖拽文件
        seat()->keyboard()->setFocus(nullptr);
        setDraggingSurface(nullptr);
        setFocus(nullptr);
    }

    // 以下代码: 只要是移动光标, 就更新下一个插入目标
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();
    // 更新目标容器
    LSurface* targetInsertWindowSurface = surfaceAtWithFilter(cursor()->pos(), [&manager](LSurface* surface){
        // 排除不是窗口的surface
        if(!surface->toplevel()){
            return false;
        }

        ToplevelRole* window = static_cast<ToplevelRole*>(surface->toplevel());

        // 排除不是平铺的surface
        if(!manager.isTiledWindow(window)){
            return false;
        }

        return true;
    });

    Surface* _targetInsertWindowSurface = static_cast<Surface*>(targetInsertWindowSurface);
    if(_targetInsertWindowSurface && manager.activateContainer() != _targetInsertWindowSurface->tl()->container){
        manager.setActiveContainer(_targetInsertWindowSurface->tl()->container);
        LLog::debug("已将插入活动目标更新为鼠标处的平铺窗口");
    }
 
    // 一个flag, 用于记录是否正在更改窗口大小
    bool activeResizing { false };
 
    // 获取所有正在进行的窗口调整大小会话
    for (LToplevelResizeSession *session : seat()->toplevelResizeSessions())
    {
        // 如果不是由触摸触发的
        if (session->triggeringEvent().type() != LEvent::Type::Touch)
        {
            
            if(!TileyServer::getInstance().is_compositor_modifier_down){
                break;
            }

            activeResizing = true;

            // 对平铺层的处理
            ToplevelRole* targetWindow = static_cast<ToplevelRole*>(session->toplevel());

            // 是否调整的是平铺层的
            bool isResizingTiledWindow = targetWindow && targetWindow->container && targetWindow->type == NORMAL && manager.isTiledWindow(targetWindow);
            
            if(isResizingTiledWindow){
                LLog::debug("调整平铺容器大小");
                if(manager.resizeTile(cursor()->pos())){
                    manager.reapplyWindowState(targetWindow);
                    manager.recalculate();
                }
            }else{
                // 不是平铺层的, 直接更新调整的位置
                session->updateDragPoint(cursor()->pos());
            }
        }
        
    }
 
    // 如果正在更改大小, 后续的就不用处理了
    if (activeResizing)
        return;
 
    // 一个flag, 用于记录是否正在移动窗口
    bool activeMoving { false };
 
    // 获取所有正在进行的窗口移动会话
    for (LToplevelMoveSession *session : seat()->toplevelMoveSessions())
    {
        // 如果不是由触摸触发的(看来作者不想处理触摸事件?)
        if (session->triggeringEvent().type() != LEvent::Type::Touch){

            if(!TileyServer::getInstance().is_compositor_modifier_down){
                break;
            }

            // 更新拖拽窗口的位置
            activeMoving = true;
            session->updateDragPoint(cursor()->pos());
            // 立即刷新屏幕, 确保视觉跟上
            session->toplevel()->surface()->repaintOutputs();
            
            // 如果窗口被最大化了, 则取消最大化(~Maximized, 取反)
            if (session->toplevel()->maximized())
                session->toplevel()->configureState(session->toplevel()->pendingConfiguration().state &~ LToplevelRole::Maximized);
        }

    }
 
    // 后续无需处理
    if (activeMoving)
        return;
 
    // 如果之前PointerButtonEvent设置了dragging Surface(作者的注释说明draggingSurface只能由PointerButtonEvent触发)
    // If a surface had the left pointer button held down
    if (draggingSurface())
    {
        // 设置相对拖动的surface的坐标
        event.localPos = cursor()->pos() - draggingSurface()->rolePos();
        sendMoveEvent(event);
        return;
    }

    // 从这里开始, 才是正常的鼠标移动逻辑(前面的都是特殊情况处理)
 
    // 查找光标下的第一个正在被显示的surface
    LSurface *surface { pointerConstrained ? focus() : surfaceAtWithFilter(cursor()->pos(), 
        [this](LSurface* s) -> bool{
            auto surface = static_cast<Surface*>(s);
            if(!surface || !surface->getView()){
                return false;
            }

            if(!surface->getView()->visible()){
                return false;
            }
            
            return true;
        })
    };
 
    if (surface)
    {   
        // 如果锁屏了, 停止处理
        if (sessionLocked && surface->client() != sessionLockManager()->client())
            return;
 
        // 设置相对surface的位置
        event.localPos = cursor()->pos() - surface->rolePos();
 
        // 如果正在拖拽(这里不是处理图标(视觉)了, 而是拖拽的逻辑)
        // DND这个surface不在surfaceAt列表中, 可以放心忽略
        if (activeDND)
        {
            if (seat()->dnd()->focus() == surface)
                // 通知surface: 在你的领空中, 一个正在拖拽的文件移动了
                seat()->dnd()->sendMoveEvent(event.localPos, event.ms());
            else
                // 通知surface: 一个正在拖拽的文件进入了你的领空
                seat()->dnd()->setFocus(surface, event.localPos);
        }
        else
        {
            // 如果没有拖拽文件, 则向所在surface发送信号
            // 这里才是整个函数本来干的事情... 因为特殊情况太多必须先处理
            if (focus() == surface){
                sendMoveEvent(event);
                // TODO: 配置: 焦点跟随鼠标
                bool focusFollowMouse = true;
                if(focusFollowMouse){
                    seat()->keyboard()->setFocus(surface);
                    if(surface->toplevel()){
                        surface->toplevel()->configureState(surface->toplevel()->pendingConfiguration().state | LToplevelRole::Activated);
                    }
                }
            } else {
                // 否则, 聚焦到所在surface中, 并且设置成特定位置
                setFocus(surface, event.localPos);
            }
        }
 
        // 响应客户端的光标更改请求
        cursor()->setCursor(surface->client()->lastCursorRequest());
    }
    // 如果鼠标位置没有surface
    else
    {
        if (activeDND)
            // 告诉正在拖拽的文件: 你退出了所有surface的领空(不用和任何surface交互)
            seat()->dnd()->setFocus(nullptr, LPointF());
        else
        {
            // 没有文件拖拽, 则正常设置聚焦为空, 并重置光标
            setFocus(nullptr);
            cursor()->useDefault();
            cursor()->setVisible(true);
        }
    }
}

// pointer: focus(), setFocus() 和 focusChanged() 只是Louvre内置的信号系统, setFocus()并不会将信号发给客户端
// keyboard: setFocus() 会将信号发给客户端。接下来的键盘输入都将定向到focused surface.

// (TODO: hyprland: 点击非窗口位置(桌面等)不会导致上一个聚焦的窗口失焦。原则: 只要桌面上有窗口就有焦点)

void Pointer::focusChanged(){

    LLog::debug("聚焦发生改变");

    Surface* surface = static_cast<Surface*>(focus());
    TileyWindowStateManager& manager = TileyWindowStateManager::getInstance();

    if(!surface){
        return;
    }

    // 以下都是为了设置新的平铺活动容器

    // 排除是弹出菜单
    if(surface->roleId() == LSurface::SessionLock || surface->roleId() == LSurface::Popup){
        return;   
    }

    // 有surface但不是toplevel并且也没有上层toplevel
    if(!surface->toplevel() && !surface->topmostParent()){
        manager.setActiveContainer(nullptr);
        LLog::debug("鼠标下没有窗口surface, 已设置活动容器为空");
        return;
    }

    // 有surface并且自己或者上层是toplevel(上面条件有一个不满足)

    // 排除是浮动的窗口
    if(!manager.isTiledWindow(surface->tl())){
        LLog::debug("鼠标位置的窗口不是平铺窗口, 不更新平铺容器");
        return;
    }

    // 设置活动容器
    manager.setActiveContainer(surface->tl()->container);
    LLog::debug("已设置活动容器为聚焦窗口的容器");

}