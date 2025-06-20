#include "include/events.h"
#include "include/core.hpp"
#include "server.hpp"
#include "include/types.h"
#include "wlr/util/log.h"

#include <cstdint>
#include <cstdlib>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

using namespace tiley;

/*********键盘事件***********/

// 处理修饰键。
static void keyboard_handle_modifiers(struct wl_listener* listener, void* data){
    
    struct input_keyboard* keyboard = wl_container_of(listener, keyboard, modifiers);

    // 由于Wayland协议限制, 一个用户只能"使用"一个键盘。我们可以插入多个键盘, 但是对于Wayland来讲同时
    // 只能用一个键盘输入, 比如下面的例子:
    // 我们插入了3个一样的键盘A,B,C, win+shift+s截图
    // 按下A的win, B的shift, C的s -> 可以, 视为同一个键盘按下的win+shift+s;
    // 但是, 如果我们在输入文字"aaaaaaaaaa...", 
    // 同时长按A和B的"a", 不会让输入加速。
    // 可以在外接了机械键盘的笔记本上试一下
    
    TileyServer& server = TileyServer::getInstance();

    wlr_log(WLR_DEBUG, "收到键盘修饰键事件");

    WindowStateManager& manager = WindowStateManager::getInstance();

    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

    manager.is_alt_down = (modifiers & WLR_MODIFIER_ALT);

    if(modifiers & WLR_MODIFIER_ALT){
        std::cout << "按下alt" << std::endl;
    }

    // 同样地, 由于协议限制, 在每次输入按键时, 把正在用的键盘设置为"使用"的键盘 
    wlr_seat_set_keyboard(server.seat, keyboard->wlr_keyboard);
    // 发送事件到客户端
    wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->wlr_keyboard->modifiers);
}

// 处理一般按键。
static void keyboard_handle_key(struct wl_listener* listener, void* data){
    //data 类型: 查看wlr_keyboard_key_event的定义。

    struct input_keyboard* keyboard = wl_container_of(listener, keyboard, key);
    TileyServer& server = TileyServer::getInstance();
    struct wlr_keyboard_key_event* event = static_cast<wlr_keyboard_key_event*>(data);
    struct wlr_seat* seat = server.seat;

    // 1. 拿到输入的按键代码keycode
    uint32_t keycode = event->keycode + 8;
    // 2. 将按键代码转换为代表的符号。一个键在不同的布局下代表的符号是不一样的。
    // 比如同样的美式键盘, 在英文环境下按"A"得到的是"a", 但在希腊语环境下就是"ɑ", 日语环境下就是"あ"。
    const xkb_keysym_t* syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

    // 冒泡事件处理标志
    // 处理层级关系: 合成器 > 客户窗口
    bool handled = false;

    // 3. 拿到修饰键
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

    /***tinywl的设置: 将alt+一个按键视为合成器级别的快捷键, 
        我们最好是允许用户自定义快捷键, 这里先注释掉***/
    /*
        if ((modifiers & WLR_MODIFIER_ALT) &&
			event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		// If alt is held down and this button was _pressed_, we attempt to
		// process it as a compositor keybinding.
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}
    */

    // 如果事件没有被处理, 交给下一个层级(说明不是合成器的快捷键)
    if(!handled){
        // 同keyboard_handle_modifiers中的处理模式, 由于Wayland协议限制, 先激活当前使用的键盘
        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        // 发送给下一级别
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
    }

}

// 处理键盘被拔出(不再可用)。
static void keyboard_handle_destroy(struct wl_listener* listener, void* data){
    struct input_keyboard* keyboard = wl_container_of(listener, keyboard, destroy);

    // 从各种包含了这个keyboard的链表中移除监听
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);

    // 释放键盘对象占用的内存
    free(keyboard);

}

/*********鼠标事件***********/

// 处理新的鼠标(准确来讲包含各种指针设备(触摸板也在其中))插入。
static void server_new_pointer(TileyServer& server, struct wlr_input_device* device){
    // 将鼠标直接绑定到光标上, 对接二者
    wlr_cursor_attach_input_device(server.cursor, device);
    // 这里还可以做一些额外的配置:
    /*
    鼠标速度/指针加速度: 修改设备的移动速度
    自然滚动: 触摸板和鼠标的滚动方向可以独立设置。
    点击方式: 对于触摸板, 可以设置"轻触即点击", 可以在自己的笔记本上触摸板上试一下, 
    触摸一下当点击比按下去才是点击要省力得多
    左手模式: 交换鼠标的左右键功能。方便用户
    禁用设备: 可以设置在打字的时候暂时禁用触摸板, 避免误触    
    */
    // 这些都属于附加功能, 我们优先专注于平铺式的设计
}

void server_new_keyboard(TileyServer& server, struct wlr_input_device* device){
    struct wlr_keyboard* wlr_keyboard = wlr_keyboard_from_input_device(device);
    struct input_keyboard* keyboard = static_cast<input_keyboard*>(calloc(1, sizeof(*keyboard)));
    keyboard->wlr_keyboard = wlr_keyboard;

    // 干下面的事情:
    // 1. 为键盘初始化XKB布局(XKB参考: https://wiki.archlinux.org/title/X_keyboard_extension), XKB作为抽象层
    // 使得不同键盘布局都可以在X Server上使用
    // 2. 设置键盘回报率(每隔多长时间接收一次输入/输入的键延迟多久生效)
    // 3. 初始化键盘事件
    // 4. 设置当前使用的键盘, 并添加键盘到管理列表中

    wlr_log(WLR_DEBUG, "新键盘插入触发");

    // 1
    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap* keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    // 2
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 400);

    // 3
    keyboard->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
    
    keyboard->key.notify = keyboard_handle_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

    keyboard->destroy.notify = keyboard_handle_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);
    
    // 4
    wlr_seat_set_keyboard(server.seat, keyboard->wlr_keyboard);
    wl_list_insert(&server.keyboards, &keyboard->link);
}

void server_new_input(struct wl_listener* _, void* data){
    // TODO: 这里写新设备插入时的逻辑
    // 注意: 刚运行compositor时也会触发一次"新设备插入", 准确地来讲, 不是物理上的新设备插入才会触发,
    // 而是检测到有新设备可用时都会触发(刚运行时检测也是理所应当)
    // 键盘和鼠标输入注册的事件也应该写在这儿。这是为了节省资源: 有一个设备我们才处理一个,
    // 否则不处理。

    // 客户端函数

    // 干下面的事情:
    // 1. 判断插入的设备类型, 根据不同的类型进行不同的初始化
    // 2. 告诉用户我们支持哪些能力(是否支持键盘, 是否支持鼠标, 是否支持触屏等)。
    // 能力的具体定义查看wl_seat_capability这个枚举量的内容。
    
    // 注意: 区分概念: client指的是显示的各个内容(窗口, 光标图像, 顶栏等等); 
    // 而seat才是用户

    // 问题: 为什么要先触发新的键盘事件再初始化能力? 

    wlr_log(WLR_DEBUG, "新的设备插入事件");

    TileyServer& server = TileyServer::getInstance();
    wlr_input_device* device = static_cast<wlr_input_device*>(data);

    // 1
    switch(device->type){
        case WLR_INPUT_DEVICE_KEYBOARD:
            server_new_keyboard(server, device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            server_new_pointer(server, device);
        default:
            break;
    }

    // 2
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if(!wl_list_empty(&server.keyboards)){
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server.seat, caps);
}