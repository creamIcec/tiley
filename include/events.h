#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <wayland-server-core.h>

// 当有新的显示屏接入时触发
void server_new_output(struct wl_listener*, void*);

// 当创建新的窗口时触发
void server_new_xdg_toplevel(struct wl_listener*, void*);

// 当创建新的弹出界面时触发
void server_new_xdg_popup(struct wl_listener*, void*);

// 当鼠标发出相对移动指令时触发
void server_cursor_motion(struct wl_listener*, void*);

// 当鼠标发出绝对移动指令时触发
void server_cursor_motion_absolute(struct wl_listener*, void*);

// 当鼠标按钮按下时触发
void server_cursor_button(struct wl_listener*, void*);

// 当鼠标滚轮滚动(上下滚动, 有些鼠标也有左右滚动的功能)时触发
void server_cursor_axis(struct wl_listener*, void*);

// 特殊事件: 针对鼠标(或者触控板)这种设备的特性而引入的事件。
/* 由于鼠标的移动、按键等事件可以同时发生, 并且用户移动鼠标很快, 考虑到处理性能, 
我们必须设定一个短的固定周期来一起处理这些事件, 而不是移动一点点就处理一次事件。
在这里的frame就是这样一个的特殊事件。工作流程如下:
1. 当鼠标的基本事件发生时(例如移动、按下按键), 先进入队列
2. frame事件触发时, 一次性处理从上个frame到现在的frame之间发生的所有排入队列的事件。
对于移动等高频事件, 可能还存在起点终点的计算, 例如在一个frame中发生了下面的4次移动:
relative: + (100,200),  + (100,300),  - (50,20),  + (50,80) 
那么, 在frame结束处理时, 就会跳过中间的移动, 计算起点和终点之间的差异:
relative: +(100,200)+(100,300)-(50,20)+(50,80) = +(200,560)
然后直接将鼠标右移200像素, 下移560像素即可。
本质上frame类似于Minecraft中的定时器(tick)或者一般事件循环的timer, 通过schedule(安排)机制来同步时序和用户输入处理。
用户输入的频率是不可预测的, 但定时器是固定的, 因此需要同步。

参考: 
https://indico.freedesktop.org/event/4/contributions/182/attachments/165/233/GSoC%202023_%20wlroots%20frame%20scheduling.pdf
https://blog.krx.sh/
不太相关的参考, 但是有帮助:(鼠标响应事件的frame vs 界面刷新的frame(帧率))
https://wayland-book.com/surfaces-in-depth/frame-callbacks.html
*/
void server_cursor_frame(struct wl_listener*, void*);

// 当有新的输入设备插入时触发(例如鼠标、键盘、手写板等等)
void server_new_input(struct wl_listener*, void*);

// 当用户/程序更改鼠标图标时触发
/* 这种情况很常见, 比如程序请求: 比如我们将鼠标放到一个文本框上, 鼠标会变成"输入"的形状; 当放到一个按钮上时, 鼠标会变成"手指"的形状。
另外是用户请求: 用户替换鼠标图像。
程序请求的一个例子就是css中的cursor属性:
.div{
    cursor: pointer;
}
这就会告诉浏览器, 当用户的鼠标放在这个div上时, 请将鼠标变成"手指";'运行在我们的tiley上的浏览器就会发出这个事件。
*/
void seat_request_cursor(struct wl_listener*, void*);

// 当用户选中屏幕上的内容时触发
/* 这也是用户交互的一个主要事件。最简单的例子: 我们按住鼠标左键拖动选中文本。除此以外, 程序也可以主动发出选中的请求,
例如有些老网站实现“复制到剪贴板”这个功能的方法就是先通过javascript选中文本, 再让用户手动按下ctrl+c.
这里面的javascript选中就会触发该事件。 */
void seat_request_set_selection(struct wl_listener*, void*);

/*另记: cursor和pointer不是一个东西(鼠标和光标)。在wlroots/wayland的语境下, cursor指的是屏幕上显示的光标, 而pointer是鼠标这个设备。
这也能解释我们为什么不在new_input中注册cursor的响应事件而是全局注册, 因为cursor和pointer是分离的。
我们只是在有新的鼠标设备(pointer)插入时将其绑定到光标(cursor)上而已。
类似于"巡航"的机制: 我们没有控制鼠标它也可以通过程序来移动光标。这也是前后端分离的一个表现。*/

#endif