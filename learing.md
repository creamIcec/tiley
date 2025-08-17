## 画面撕裂,和解决办法

显卡每一秒往帧缓存器放 120 张图片,但是显示器每秒只能拿走 60 张,它两之间又没有沟通,显示器不知道显卡今天晚上是不是嗑药了,突然变得这么猛。
显卡也不知道显示器今天不行,还在一个劲的猛扎猛打,等它写满了帧缓存器,就用新的图片从头覆盖,于是,显示器还没来得及拿走的图片,就被显卡重新覆盖掉了。结果就是,画面呈现到一半,下面的跟上面的画面完全对不上,这就是画面撕裂。
下面对 垂直同步（V-Sync） 和 页面翻转（Page Flip） 两个概念做一次深入解析,帮助你理解它们在图形渲染管线中的角色与区别。

### 一、页面翻转（Page Flip）

1. 双缓冲与前后缓冲区
   现代图形系统通常采用 双缓冲（Double Buffering）或 多重缓冲（Triple Buffering）策略来避免“画面撕裂”问题。

前缓冲区（Front Buffer）：GPU 已合成完毕、并正被扫描输出到显示面板的帧缓存。

后缓冲区（Back Buffer）：CPU/GPU 正在往里绘制下一个帧（调用 OpenGL/DirectX/DXGI 等 API 提交命令）。

2. 什么是页面翻转
   当后缓冲区的一帧完成绘制后,驱动会在 下一个垂直消隐（VBlank,见下一节） 时刻,把“后缓冲”与“前缓冲”进行指针交换（或称交换缓冲区指针）,从而让刚绘制好的帧无缝显示到屏幕上。这一过程就叫 页面翻转（Page Flip）。

优点

零拷贝：只交换缓冲区地址,无需把内存数据从一个缓冲区复制到另一个。

原子更新：新帧与旧帧切换在 VBlank 瞬间完成,避免了中途部分区域更新带来的撕裂。

注意

页面翻转只能在显卡和显示硬件都支持双缓冲交换的模式下使用。

如果硬件或驱动不支持交换,有时会退回到“拷贝后缓冲到前缓冲”（blit）的方式,效率较低。

### 二、垂直同步（V-Sync,Vertical Sync）

1. 垂直消隐（VBlank）信号
   CRT、LCD 等显示设备每刷新一帧,会产生一次 垂直消隐信号（Vertical Blank）。

在这个时刻,显示面板内部正完成一帧扫描后,准备开始下一个扫描。此时切换帧缓冲,能够保证在扫描开启新帧前完成完整切换。

2. 开启 V-Sync 的含义
   当启用 V-Sync 时,GPU 在执行 swapBuffers()（或 DRM 的 drmModePageFlip()）后,会 阻塞／延迟 直到下一个 VBlank 到来,才真正完成页面翻转：

用户调用 swapBuffers()

GPU 驱动 将翻转请求提交到命令队列

驱动或内核 等待下一次 VBlank

在 VBlank 时刻,一次性将后缓冲指向前缓冲,并通知用户线程继续

优势
消除撕裂：不会出现画面中上半帧、下半帧属于不同时间点内容的情况。

固定帧率：帧率与显示器刷新率锁定（例如 60 Hz → 60 FPS）,保证一致输出。

代价
阻塞延迟：如果你的渲染只花费了 1 ms,而显示器每 16.67 ms 刷新一次,那么调用 swapBuffers() 后,线程要等到大约 15.67 ms 后才能返回。

总体吞吐率下降：渲染线程在等候期间无法提交更多命令,可能让 CPU 或其它线程空闲。

### 原理

交换指针（Swap Buffers）是实现 页面翻转（Page Flip）中的关键操作。为了理解为什么交换指针能够实现页面翻转,首先我们需要理解缓冲区、渲染和显示是如何协同工作的。以下是详细的解析。

一、双缓冲与页面翻转

1. 双缓冲（Double Buffering）
   现代图形系统通常使用双缓冲来避免画面撕裂（tearing）问题。双缓冲是指：

前缓冲区（Front Buffer）：用于显示的帧缓冲区,GPU 将渲染的内容发送到此缓冲区,显示设备（如屏幕）正在读取和显示这个缓冲区的内容。

后缓冲区（Back Buffer）：用来存储下一帧的渲染内容。GPU 在这个缓冲区上执行渲染工作,直到渲染完成。

2. 什么是页面翻转？
   当后缓冲区的一帧渲染完毕并准备好显示时,驱动程序会执行“页面翻转（Page Flip）”,即：

交换前后缓冲区的指针,让前缓冲区指向刚才完成渲染的后缓冲区。

这样,新的帧内容会在下次刷新周期时无缝显示到屏幕上。

页面翻转的关键是交换指针的操作。交换指针的作用是直接将渲染后的内容“呈现”到屏幕上,而不需要再次复制数据。这种方式被称为 零拷贝（Zero Copy）,即只交换两个缓冲区的指针地址,而不涉及大量内存数据的复制操作,极大提高了效率。

二、为什么交换指针能实现页面翻转
在 GPU 渲染的过程中,页面翻转的实现主要是通过交换前后缓冲区的指针地址来完成的。这是因为：

前缓冲区和后缓冲区的角色：

前缓冲区是显示内容的缓冲区,显示器读取前缓冲区的内容进行显示。

后缓冲区是渲染内容的缓冲区,GPU 正在往里面渲染下一帧的图像。

指针交换的作用：
页面翻转通过交换指针实现：

原本指向前缓冲区的指针会指向后缓冲区,原本指向后缓冲区的指针会指向新的一块内存区域,或者指向同一个内存区域（双缓冲模式）。

这样,当渲染完成后,显示硬件会显示刚才渲染好的内容,即便没有复制任何数据。所有的变化都是通过交换前后缓冲区指针实现的。

零拷贝（Zero Copy）：
通过交换指针,不需要将数据从后缓冲区拷贝到前缓冲区。指针交换的操作只是将内存地址的指向切换,这非常快速,不会浪费 CPU 和内存带宽。

避免撕裂：
如果直接渲染到前缓冲区,并在同一时刻进行显示和渲染（例如没有页面翻转）,就可能会出现撕裂现象,即屏幕的某些部分显示的是旧的一帧,其他部分显示的是新的一帧。通过交换指针并在垂直消隐（VBlank）期间进行交换,可以确保每一帧的完整性,避免了撕裂。

三、示例
没有页面翻转（直接渲染到前缓冲区）：

渲染：GPU 将新帧渲染到前缓冲区。

显示：显示器开始读取前缓冲区的内容并展示。

撕裂：由于显示器读取的内容可能不是完全一致的,部分区域显示的是当前帧,部分区域可能仍然显示的是上一帧的内容,导致画面撕裂。

使用页面翻转（交换前后缓冲区）：

渲染：GPU 将新帧渲染到后缓冲区。

页面翻转：当显示器完成一帧显示并进入垂直消隐时,GPU 将前缓冲区指向后缓冲区,后缓冲区指向新的内存区域（或仍指向同一内存区域）。

显示：显示器读取新的前缓冲区的内容,这时新的帧内容无缝显示出来。

这种方式通过 交换指针 的方法,保证每次页面翻转时,屏幕上显示的总是渲染完成的完整帧,避免了撕裂。

## Lscene

LScene 的基本功能
LScene 类：是一个可选的工具类,旨在简化渲染管理,提供一个主要的 LSceneView,该视图可以包含多个子视图,并支持嵌套场景。

渲染多输出设备：一个 LScene 可以驱动多个 LOutput 输出设备,同时也可以处理 Wayland 输入事件,并为每个视图提供独立的输入事件处理。

例如,你可以使用 LScene 来渲染整个合成器的 UI,而不是直接依赖底层的 OpenGL 着色器或者 LPainter 类提供的基本渲染功能。

2. LScene 的渲染机制
   高效的脏区渲染：LScene 会根据每个视图的修改状态,智能地只渲染“受影响的区域”。这意味着只有视图中有更新或者被标记为脏区的部分才会被重新渲染,从而提高渲染效率,避免不必要的全屏渲染。

遮挡区域剔除：如果一个视图完全被其他不透明视图遮挡,LScene 会避免对该视图进行渲染,进一步节省 GPU 资源。

3. 与 LOutput 的集成
   LScene 与输出设备 (LOutput) 的集成是通过一些关键的 OpenGL 生命周期方法来实现的。每个输出设备都需要将以下方法与 LScene 的相应方法绑定：

#include <LOutput.h>

class YourCustomOutput : public LOutput {
void initializeGL(LOutput *output) override {
scene->handleInitializeGL(output); // 绑定 OpenGL 初始化
}
void paintGL(LOutput *output) override {
scene->handlePaintGL(output); // 绑定渲染过程
}
void moveGL(LOutput *output) override {
scene->handleMoveGL(output); // 绑定移动事件
}
void resizeGL(LOutput *output) override {
scene->handleResizeGL(output); // 绑定大小调整
}
void uninitializeGL(LOutput \*output) override {
scene->handleUninitializeGL(output); // 绑定清理过程
}
};
这些方法确保了每个输出设备都能正确地初始化、绘制、调整大小、移动以及清理其 OpenGL 环境。

4. 输入事件管理
   LScene 还负责管理输入事件的分发,支持多种输入设备（指针、键盘、触摸）的事件处理。以下是一些方法和事件类型的说明：

handlePointerMoveEvent、handlePointerButtonEvent、handlePointerScrollEvent 等方法：处理指针移动、按钮点击、滚动等事件。

handleKeyboardKeyEvent：处理键盘按键事件。

handleTouchDownEvent、handleTouchMoveEvent、handleTouchUpEvent：处理触摸事件。

EventOptions 和 InputFilter：提供额外的控制选项,用于控制事件分发的行为,例如只发送事件给有输入事件启用的视图,或者在输入区域内进行过滤。

5. LScene 中的焦点管理
   pointerFocus()：返回当前指针所在区域的所有视图,这些视图允许接收指针事件,按 Z 顺序排列,最上面的视图排在前面。

keyboardFocus()：返回当前拥有键盘焦点的视图,只有那些启用了键盘事件的视图会被包括在内。

touchPoints()：返回场景中所有的活跃触摸点,帮助管理触摸事件的分发。

6. 自动重绘控制
   enableAutoRepaint()：启用或禁用子视图的自动重绘。当禁用时,子视图的 LOutput::repaint() 请求将被忽略。
   这在你需要在一个 paintGL() 回调中修改多个视图时特别有用,可以避免多次触发重绘。

7. 查找和定位视图
   viewAt()：根据给定的坐标查找并返回位于该位置的视图。这在需要检测指针、键盘或触摸事件时非常有用,允许你获取位于特定位置的视图实例。

## 场景与视图

LScene 的基本功能
LScene 类：是一个可选的工具类,旨在简化渲染管理,提供一个主要的 LSceneView,该视图可以包含多个子视图,并支持嵌套场景。

渲染多输出设备：一个 LScene 可以驱动多个 LOutput 输出设备,同时也可以处理 Wayland 输入事件,并为每个视图提供独立的输入事件处理。

例如,你可以使用 LScene 来渲染整个合成器的 UI,而不是直接依赖底层的 OpenGL 着色器或者 LPainter 类提供的基本渲染功能。

2. LScene 的渲染机制
   高效的脏区渲染：LScene 会根据每个视图的修改状态,智能地只渲染“受影响的区域”。这意味着只有视图中有更新或者被标记为脏区的部分才会被重新渲染,从而提高渲染效率,避免不必要的全屏渲染。

遮挡区域剔除：如果一个视图完全被其他不透明视图遮挡,LScene 会避免对该视图进行渲染,进一步节省 GPU 资源。

3. 与 LOutput 的集成
   LScene 与输出设备 (LOutput) 的集成是通过一些关键的 OpenGL 生命周期方法来实现的。每个输出设备都需要将以下方法与 LScene 的相应方法绑定：

cpp
复制
编辑
#include <LOutput.h>

class YourCustomOutput : public LOutput {
void initializeGL(LOutput *output) override {
scene->handleInitializeGL(output); // 绑定 OpenGL 初始化
}
void paintGL(LOutput *output) override {
scene->handlePaintGL(output); // 绑定渲染过程
}
void moveGL(LOutput *output) override {
scene->handleMoveGL(output); // 绑定移动事件
}
void resizeGL(LOutput *output) override {
scene->handleResizeGL(output); // 绑定大小调整
}
void uninitializeGL(LOutput \*output) override {
scene->handleUninitializeGL(output); // 绑定清理过程
}
};
这些方法确保了每个输出设备都能正确地初始化、绘制、调整大小、移动以及清理其 OpenGL 环境。

4. 输入事件管理
   LScene 还负责管理输入事件的分发,支持多种输入设备（指针、键盘、触摸）的事件处理。以下是一些方法和事件类型的说明：

handlePointerMoveEvent、handlePointerButtonEvent、handlePointerScrollEvent 等方法：处理指针移动、按钮点击、滚动等事件。

handleKeyboardKeyEvent：处理键盘按键事件。

handleTouchDownEvent、handleTouchMoveEvent、handleTouchUpEvent：处理触摸事件。

EventOptions 和 InputFilter：提供额外的控制选项,用于控制事件分发的行为,例如只发送事件给有输入事件启用的视图,或者在输入区域内进行过滤。

5. LScene 中的焦点管理
   pointerFocus()：返回当前指针所在区域的所有视图,这些视图允许接收指针事件,按 Z 顺序排列,最上面的视图排在前面。

keyboardFocus()：返回当前拥有键盘焦点的视图,只有那些启用了键盘事件的视图会被包括在内。

touchPoints()：返回场景中所有的活跃触摸点,帮助管理触摸事件的分发。

6. 自动重绘控制
   enableAutoRepaint()：启用或禁用子视图的自动重绘。当禁用时,子视图的 LOutput::repaint() 请求将被忽略。
   这在你需要在一个 paintGL() 回调中修改多个视图时特别有用,可以避免多次触发重绘。

7. 查找和定位视图
   viewAt()：根据给定的坐标查找并返回位于该位置的视图。这在需要检测指针、键盘或触摸事件时非常有用,允许你获取位于特定位置的视图实例。

LView 是 Louvre 框架中定义的一个基类,用于表示在 LScene 中的视图（view）。它提供了一个基本的接口和多种功能,可以让开发者创建和管理自定义视图（如容器、纹理视图、颜色视图等）。这些视图可以是可渲染的,也可以是容器型的,用来管理其他视图的布局和层次。

LView 类概述
LView 是所有视图的基类,用于表示一个场景中的视图,通常需要继承和扩展 LView 来创建自定义视图。

LView 提供了管理视图的多个功能,例如：

输入事件（例如鼠标、键盘、触摸事件）

视图的渲染（通过 paintEvent）

视图的层次结构（视图的父子关系）

可见性和渲染状态控制（如透明度、是否渲染、是否可见等）

关键功能解释

1. 输入事件管理
   Pointer Events：通过 enablePointerEvents(bool enabled) 启用或禁用视图的鼠标事件。当启用时,视图可以接收鼠标事件。

Keyboard Events：通过 enableKeyboardEvents(bool enabled) 启用或禁用键盘事件。

Touch Events：通过 enableTouchEvents(bool enabled) 启用或禁用触摸事件。

Blocking Events：例如,enableBlockPointer(bool enabled) 用于阻止视图背后的其他视图接收鼠标事件,防止用户与背景视图交互。

2. 视图的渲染和刷新
   Repaint：调用 repaint() 会调度视图的重绘,在视图的任何输出设备上重新绘制该视图。

Damage：通过 damageAll() 强制在下一帧进行完整的重绘。

paintEvent：是一个虚函数,用于渲染视图的内容,必须在自定义视图中实现。

3. 视图的位置、大小和变换
   位置和大小：pos() 和 size() 方法返回视图的当前位置和大小（考虑到父视图的偏移和缩放）。

父视图的影响：enableParentOffset(bool enabled) 和 enableParentScaling(bool enabled) 控制视图是否受其父视图的影响,继承父视图的位置和缩放。

4. 视图的可见性与映射
   可见性控制：通过 setVisible(bool visible) 来设置视图是否可见。虽然视图可能标记为可见,但它的父视图或其他条件也会影响最终是否渲染。

映射状态：mapped() 方法检查视图是否应该被渲染,考虑了可见性、父视图的映射状态等因素。

5. 自定义视图的扩展
   视图类型：LView 类可以有不同的子类,表示不同的视图类型,如：

LLayerView（层视图,通常用作容器）

LSurfaceView（显示客户端表面）

LTextureView（显示纹理）

LSolidColorView（显示纯色矩形）

LSceneView（自定义场景视图）

等等

这些视图类型可以通过继承 LView 并重写相关方法来创建。

6. 父子视图关系
   Parent-child relationship：视图可以有父视图和多个子视图。子视图的绘制会叠加在父视图上,而父视图会影响子视图的位置和缩放。

parent() 和 children()：parent() 获取视图的父视图,而 children() 获取该视图的所有子视图。

insertAfter() 和 setParent()：insertAfter() 用于将视图插入到另一个视图后面,setParent() 用于更改视图的父视图。

7. 绘制和透明度
   透明度控制：setOpacity() 用于设置视图的透明度,支持 0（完全透明）到 1（完全不透明）的范围。

混合函数：setBlendFunc() 用于设置视图的自定义颜色混合函数,可以控制如何将视图与背景合成。

8. 视图的边界和裁剪
   裁剪：enableClipping(bool enabled) 控制是否启用视图的裁剪,setClippingRect() 用于设置裁剪区域。

边界框：boundingBox() 返回视图及其所有可见子视图的边界框,计算包含子视图的位置。

视图（View）在图形界面和图形渲染中通常指的是 界面的一部分,它显示给用户的内容或者交互元素。在许多图形框架中,视图是用来承载并显示可视内容的组件或对象。具体来说,视图在 Louvre 框架中的作用可以理解为 场景中的一个渲染单元,它可能是一个简单的矩形区域、一个包含子视图的容器,或者是一个显示客户端窗口、图形或文本的元素。

视图的核心概念
容器：视图不仅可以渲染内容,还可以作为其他视图的容器,管理多个子视图。例如,一个视图可能包含多个按钮、文本框和图形元素。

位置与大小：视图通常有自己的 位置 和 大小,这决定了它在显示屏上的显示区域。位置和大小可以是固定的,或者根据父视图、缩放、偏移等因素进行动态计算。

渲染：视图负责将某些内容渲染到屏幕上,可能涉及绘制图形、显示图像或文本。视图的内容可以根据场景的需求进行变化。

输入事件：视图还可以接收用户输入事件,例如鼠标点击、移动、触摸事件或键盘输入。这些事件通常由视图的父容器管理,并分发到对应的子视图。

层级关系：视图通常具有父子关系,视图的排列顺序由它们在层级结构中的位置决定。子视图会被绘制在父视图之上,控制视图的堆叠顺序。

可见性：视图可能是可见的,也可能是不可见的。可见性决定了视图是否会被渲染或显示给用户。

在 Louvre 框架中的视图
在 Louvre 框架中,LView 类是所有视图的基类。你可以创建不同类型的视图,如：

LLayerView：一个不渲染自身内容的容器视图,用来管理其他视图。

LSurfaceView：用于显示客户端表面的视图。

LTextureView：显示纹理的视图。

LSolidColorView：显示纯色矩形的视图。

LSceneView：一个更复杂的视图,可以看作是一个场景本身,管理自己的内容。

视图的基本功能
渲染：每个视图都可以包含绘制逻辑（如使用 OpenGL 渲染图像或文本）。通过 paintEvent 方法,视图将自己的内容绘制到屏幕上。

事件处理：视图能够处理来自用户的输入事件（如鼠标点击、键盘输入等）。例如,通过实现 pointerMoveEvent 或 keyEvent,视图可以响应用户的动作。

层级和排列：视图可以有父视图和多个子视图。视图的排列顺序和显示层级（即哪些视图在前面,哪些在后面）由父子关系以及 children() 方法返回的子视图列表决定。

可见性控制：视图可以通过 setVisible() 和 visible() 控制是否可见。尽管视图可能标记为可见,但是否被渲染还受到父视图的映射状态、视图的可见性等因素的影响。

变换和缩放：视图的位置和大小可以经过变换和缩放。例如,enableParentScaling() 允许视图的大小和位置受父视图缩放的影响。

视图与场景（Scene）
在 Louvre 框架中,视图通常被嵌入到一个场景（LScene）中,场景是用来管理和渲染多个视图的容器。一个场景可能包含多个视图,视图之间可能有复杂的层次结构。

## 伽马类

LGammaTable 是 Louvre 库中的一个类,主要用于 显示输出的伽马校正。伽马校正是一个图像处理技术,用于调整显示设备的亮度和对比度,以使图像显示更加自然或符合某些视觉效果要求。LGammaTable 类允许为每个显示输出（LOutput）定义 RGB 组件的伽马校正曲线,并将这些曲线应用到显示设备中。

核心功能
构建伽马校正表：

LGammaTable 允许创建一个包含三个颜色通道（红色、绿色和蓝色）伽马校正曲线的表格。每个颜色通道的曲线存储为 UInt16 数值数组。

伽马校正表的大小由 setSize() 和 size() 方法管理。每个颜色通道会有一个 UInt16 数值数组,数组大小由 size() 确定。如果 size() 设置为 256,那么每个通道（红、绿、蓝）会有 256 个 UInt16 值。

填充伽马、亮度与对比度：

fill() 方法可以用来设置伽马、亮度和对比度值,进而更新伽马校正表的内容。它接受三个参数,分别是：gamma（伽马值）,brightness（亮度值）,contrast（对比度值）。

访问颜色通道：

red()、green() 和 blue() 方法分别返回指向红色、绿色和蓝色曲线数组的指针。通过这些指针,可以直接操作和修改各个颜色通道的伽马值。

表格大小与校验：

setSize() 用来设置表格的大小（即每个颜色通道的 UInt16 数值数量）,并可以通过 size() 方法获取当前表格的大小。

LGammaTable 的大小必须与 LOutput::gammaSize() 配置的输出的伽马表大小匹配。如果不匹配,LOutput::setGamma() 方法将失败。

主要方法说明
构造函数与析构函数：

LGammaTable(UInt32 size=0)：构造一个伽马校正表,size 表示表格的大小。

~LGammaTable()：析构函数,销毁伽马校正表。

表格大小操作：

setSize(UInt32 size)：设置伽马表的大小。

size()：获取伽马表的大小。

填充表格：

fill(Float64 gamma, Float64 brightness, Float64 contrast)：使用指定的伽马值、亮度值和对比度值填充表格。

颜色通道访问：

red()：返回指向红色曲线的指针。

green()：返回指向绿色曲线的指针。

blue()：返回指向蓝色曲线的指针。

应用场景
显示输出的伽马校正：
LGammaTable 主要用于 Wayland 合成器 中的图形渲染,调整显示设备的颜色显示效果。通过伽马校正,可以优化显示内容,使其在不同的显示设备上具有一致的颜色和亮度表现。

动态调整显示设备的视觉效果：
通过修改 LGammaTable 中的 RGB 曲线,开发者可以根据不同的显示需求（如增强对比度、调整亮度等）来动态调整显示设备的视觉效果。这对于需要准确颜色显示的应用场景,如图像处理、视频编辑等,尤其重要。

为显示设备配置 Gamma 校正：
当使用 LOutput::setGamma() 或 LOutput::setGammaRequest() 时,LGammaTable 可以被应用到显示输出上,精确控制显示的每个颜色通道,适应各种硬件设备的显示需求。

## 键盘事件

LKeyboard 是 Louvre 库中用于处理键盘事件的类,主要功能是接收和转发由输入后端生成的键盘事件,并允许配置诸如键盘映射、键重复速率等参数。它与 Wayland 系统的键盘输入交互,并提供了一些方法来处理键盘焦点、抓取、按键状态和修改器（如 Ctrl、Shift 等）的管理。

主要功能
设置和获取键盘焦点：

setFocus(LSurface \*surface)：设置哪个表面（LSurface）具有键盘焦点。只有一个表面可以获得键盘焦点,且每次调用会移除之前的焦点。

focus()：获取当前获得键盘焦点的表面。

键盘抓取：

setGrab(LSurface \*surface)：设置一个键盘抓取,这会阻止其他表面获得键盘焦点。被抓取的表面会处理所有键盘事件,即使其他表面请求焦点时也无效。

grab()：获取当前有抓取键盘事件的表面。

键盘修改器状态：

modifiers()：获取当前键盘修改器的状态,例如 Shift、Ctrl 和 Caps Lock 等按键是否被按下。

isModActive(const char \*name, xkb_state_component type)：检查某个修改器键（如 Shift、Ctrl）是否处于活动状态。

键盘映射：

setKeymap(...)：设置键盘映射,可以指定规则、布局、变种等信息。

keySymbol(UInt32 keyCode)：根据键盘代码获取对应的符号。

键重复设置：

setRepeatInfo(Int32 rate, Int32 msDelay)：设置按键重复的速率和延迟。按下按键时,系统会自动重复按键,速率和延迟可以自定义。

按键事件发送：

sendKeyEvent(const LKeyboardKeyEvent &event)：将按键事件发送到当前具有焦点的表面,事件类型可以是按键按下或释放。

键盘事件回调：

keyEvent(const LKeyboardKeyEvent &event)：这个虚拟方法由输入后端生成键盘事件并调用。开发者可以重写此方法来处理键盘事件,例如根据按键状态（按下或释放）执行不同的操作。

关键方法解释
设置焦点和抓取：

setFocus()：设置焦点到指定的表面。焦点表面将接收后续的键盘事件。

setGrab()：设置抓取表面,抓取的表面将“锁定”键盘事件,其他表面不能获得键盘焦点,直到抓取被移除。

按键事件处理：

sendKeyEvent()：当有按键事件（例如按下或释放）时,将该事件发送到当前获得焦点的表面。这使得该表面能够处理键盘输入。

keyEvent()：这个方法是由输入后端触发,处理所有传入的键盘事件。例如,如果按下 Ctrl + Shift + ESC,可以执行退出操作。

键盘映射和修改器：

keymapFd()：获取键盘映射的文件描述符,通常是通过 XKB 获取的。

keySymbol()：将原始的键盘扫描码转换为相应的键符号（例如 XKB_KEY_Q）。

isKeyCodePressed()：检查某个按键的扫描码是否被按下。

重复按键设置：

setRepeatInfo()：设置键盘按键的重复速率和延迟。默认情况下,按下某个键后会持续发送该按键的信号,直到松开键。

使用场景
Wayland 键盘输入管理：

LKeyboard 用于管理与 Wayland 相关的键盘输入,处理焦点、抓取和键盘事件的分发。通过 LKeyboard 类,开发者能够精确地控制哪些表面接收键盘事件,如何处理键盘输入,以及是否启用键盘事件的重复。

键盘事件响应：

开发者可以根据按下的键（例如 Ctrl、Shift）执行特定操作。例如,按下 Ctrl+Q 可以关闭当前应用,按下 F1 可以启动一个终端。

支持键盘修改器和重复按键：

支持按键修改器（如 Ctrl、Shift）的状态检查,以及控制键盘按键的重复行为（例如按住某个键会自动重复输入）。这对于实现复杂的快捷键功能和文本编辑功能非常有用。

多表面键盘焦点切换：

setFocus() 和 setGrab() 使得多个表面可以共享同一键盘输入,允许开发者更细致地控制键盘事件的路由。

总结
LKeyboard 类为 Louvre 提供了强大的键盘事件处理功能,支持键盘焦点、键盘抓取、按键重复、修改器状态以及键盘映射管理等功能。开发者可以使用 LKeyboard 来实现复杂的键盘输入响应逻辑,适应不同的输入需求和用户交互。

## 触摸事件

LTouch 是 Louvre 库中的一个类,用于处理触摸输入事件。它管理和分发触摸事件（如触摸按下、移动、释放）,并将这些事件转发给相应的客户端表面。LTouch 提供了多种方法来跟踪触摸点的状态,处理事件以及管理触摸输入的生命周期。

主要功能
管理触摸点：

surfaceAt(const LPoint &point)：通过指定的坐标查找包含该点的表面。它考虑了表面的角色位置和输入区域。

touchPoints()：返回当前所有活动的触摸点。

createOrGetTouchPoint(const LTouchDownEvent &event)：基于触摸下事件创建新的触摸点,或者返回已经存在的触摸点。

findTouchPoint(Int32 id)：根据触摸点的唯一标识符查找触摸点。

发送触摸事件：

sendFrameEvent(const LTouchFrameEvent &event)：将一个触摸框架事件发送给所有与触摸点相关的客户端。

sendCancelEvent(const LTouchCancelEvent &event)：发送一个取消事件,通常在触摸设备不可用时触发。

触摸事件回调：

touchDownEvent(const LTouchDownEvent &event)：当触摸点被按下时触发。

touchMoveEvent(const LTouchMoveEvent &event)：当触摸点移动时触发。

touchUpEvent(const LTouchUpEvent &event)：当触摸点释放时触发。

touchFrameEvent(const LTouchFrameEvent &event)：在触摸事件集（按下、移动、释放）处理完成后触发,通常用于保证事件的原子性。

touchCancelEvent(const LTouchCancelEvent &event)：当所有活动触摸点被取消时触发,通常发生在触摸设备失效时。

触摸点坐标转换：

toGlobal(LOutput \*output, const LPointF &touchPointPos)：将触摸点坐标转换为全局坐标。

关键方法解释
触摸点管理：

createOrGetTouchPoint()：当一个新的触摸点被按下时,使用该方法创建一个新的触摸点,或者如果触摸点已经存在,则返回现有的触摸点。

findTouchPoint()：查找具有指定 ID 的触摸点。这对于跟踪多个触摸点非常有用。

触摸事件处理：

touchDownEvent()：当触摸设备检测到一个新的触摸按下时调用。它通过创建或获取触摸点,并将触摸点关联到一个表面来开始处理该事件。

touchMoveEvent()：当一个触摸点在设备上移动时调用。它用于更新触摸点的位置,并可能涉及拖放（DND）会话或窗口的移动和大小调整。

touchUpEvent()：当一个触摸点被释放时调用。它处理触摸点的释放事件,并可能停止当前的拖放或大小调整会话。

touchFrameEvent()：当一组触摸事件（按下、移动、释放）都完成并且应被原子处理时调用。这个方法用于协调多个触摸事件的同步。

touchCancelEvent()：当所有活动的触摸点都被取消时调用,通常在触摸设备不可用或发生错误时。

触摸事件框架：

sendFrameEvent()：在一组触摸事件完成后发送一个框架事件,通常用于通知所有相关的客户端,触摸点已经完成,并且可以处理它们的输入。

sendCancelEvent()：发送一个取消事件,通常是在触摸设备失效或断开时调用,通知所有客户端清除当前的触摸点。

触摸点坐标转换：

toGlobal()：将设备坐标系中的触摸点位置转换为全局坐标,通常用于将触摸事件映射到特定的输出设备上。

使用场景
触摸屏输入管理：
LTouch 类用于管理触摸输入事件的分发。在一个支持触摸屏的 Wayland 环境中,当用户在触摸设备上进行操作时,LTouch 可以跟踪每个触摸点,并将这些事件转发到相应的客户端表面。

多点触摸和手势识别：
LTouch 支持多点触摸管理。它能够追踪每个触摸点,并根据用户的手势（如滑动、缩放）执行不同的操作。

窗口拖动与大小调整：
在桌面环境中,LTouch 通过监控触摸事件,可以实现窗口的拖动和大小调整功能。当触摸点移动时,系统可以触发窗口的相应操作。

触摸事件同步：
LTouch 提供了框架事件和取消事件的方法,允许将多个触摸事件（按下、移动、释放）作为一个原子事件处理,确保在应用程序处理触摸输入时的同步性和一致性。

## 设备管理

LInputDevice 是 Louvre 输入层的最基础抽象：代表系统里由输入后端（input backend）发现的一条“输入硬件通道”。
与很多框架把“设备类型”直接做成派生类不同,Louvre 通过 “能力（Capability）位标记” 来描述一个物理设备拥有哪些输入功能：例如一个触控板既有 Pointer（指针）能力,又可能包含 Gestures（手势）能力；一个游戏平板可能同时提供 TabletTool、TabletPad 等能力；一只普通 USB 鼠标只具备 Pointer；而一个 2-in-1 触摸键盘底座既可能报键盘又报开合盖 Switch。

你通过 LSeat::inputDevices() 能拿到当前所有已知设备的列表（这些对象在设备插拔的整个生命周期中不会被销毁,只是“失活”）,并通过 LSeat::inputDevicePlugged() / inputDeviceUnplugged() 事件监控热插拔。

2. 生命周期与“不会销毁”的含义
   热插拔发生：输入后端（例如 libinput）检测到一个新物理设备 → Louvre 创建或更新内部结构并暴露 LInputDevice。

拔出设备：该 LInputDevice 对象仍然存在（因此你持有的指针仍然有效）,但其 nativeHandle() 可能返回 nullptr（表示底层资源已经不可配置）,你应在逻辑中对 nullptr 做防御判断。

再次插入同型号设备：可能复用或新建底层 handle。你的高层逻辑可以保持对同一个 LInputDevice 的引用,不必重新枚举绑定。

这种设计好处是避免你在高层还要做复杂的“观察者 + 智能指针失效”处理；缺点是你必须每次使用 nativeHandle() 前检查非空。

3. 能力（Capability）位标记
   枚举（可按位组合）：

Capability 语义 典型设备举例 可能配套的高层类 / 事件
Pointer 产生相对/绝对指针移动、按钮 鼠标、触控板、绘图板指针模式 LPointer
Keyboard 键码 / 修饰键输入 USB/蓝牙键盘、笔记本内置键盘 LKeyboard
Touch 多点触摸（Down/Move/Up/Frame/Cancel） 触摸屏、触控数字板 LTouch
TabletTool 绘图笔(工具)坐标、压力、倾角 专业数位板笔 将对应 Tablet 角色整合到指针 / 绘图输入
TabletPad 平板上的功能按键、转轮 专业数位板的快捷键区域 自定义快捷映射
Gestures 高级手势（3/4 指、捏合、滑动） 触控板 手势识别层（可能产生手势事件传给合成器）
Switch 开合盖、模式开关、摄像头遮挡开关等 笔记本合盖传感器、姿态开关 会影响休眠 / 屏幕关闭策略

使用方法：

cpp
复制
编辑
for (LInputDevice\* dev : seat()->inputDevices()) {
if (dev->hasCapability(LInputDevice::Pointer)) {
// 注册或统计指针类逻辑
}
if (dev->hasCapability(LInputDevice::Keyboard)) {
// 可决定是否改变焦点策略
}
} 4. 与其它输入类的关系
类 作用 与 LInputDevice 关系
LPointer 全局“指针通道”逻辑（聚合所有提供 Pointer 能力的物理设备的运动、按钮） 来自所有 hasCapability(Pointer) 设备的事件在输入后端聚合后推到 LPointer::pointerMoveEvent() 等
LKeyboard 单实例键盘逻辑（焦点、映射、重复） 聚合所有 Keyboard 能力设备的按键
LTouch 触摸点管理（多点 ID、Down/Move/Up/Frame） 聚合所有 Touch 能力设备的触摸事件
手势处理层（内部） 抽象出手势（Swipe / Pinch） 依赖具 Gestures 能力的设备提供的原始数据
Tablet 相关（Tool/Pad） 压力、倾角、快捷键轮盘 读取 TabletTool/TabletPad 能力设备

关键点：Louvre 将“物理设备集合”与“语义输入管线”分离：

物理多设备 → 能力辨识 → 汇聚成若干“逻辑输入器”（Pointer / Keyboard / Touch）

你的 compositor 逻辑大多数时候只与逻辑输入器交互,而非逐物理设备管理。

5. 典型使用场景
   (1) 设备枚举与日志
   cpp
   复制
   编辑
   void MyCompositor::logDevices() {
   for (LInputDevice\* dev : seat()->inputDevices()) {
   std::ostringstream caps;
   if (dev->hasCapability(LInputDevice::Pointer)) caps << "Pointer ";
   if (dev->hasCapability(LInputDevice::Keyboard)) caps << "Keyboard ";
   if (dev->hasCapability(LInputDevice::Touch)) caps << "Touch ";
   // ...
   LLog::info("InputDev: name=%s vendor=%04x product=%04x caps=%s",
   dev->name().c_str(), dev->vendorId(), dev->productId(), caps.str().c_str());
   }
   }
   (2) 基于 Vendor/Product ID 匹配特殊策略
   例如某些触控板需要禁用“自然滚动”或开启“掌压抑制”：

cpp
复制
编辑
void MyCompositor::tuneDevices() {
for (auto* dev : seat()->inputDevices()) {
if (!dev->hasCapability(LInputDevice::Pointer)) continue;
if (dev->vendorId() == 0x06cb /*例*/ && dev->productId() == 0x1234) {
if (void* h = dev->nativeHandle()) {
// 假设后端是 libinput
auto* libinput_dev = static_cast<libinput_device*>(h);
libinput_device_config_scroll_set_natural_scroll_enabled(libinput_dev, 0);
}
}
}
}
在调用前要先通过 LCompositor::inputBackendId() 确保后端确实是 LInputBackendLibinput,否则 nativeHandle() 可能无意义或为 nullptr。

(3) 监听热插拔
你在 LSeat::inputDevicePlugged() 回调里做：

重新刷新策略（如重新注册 Tablet 设备）

输出日志 / 动态 UI 提示
拔出时 (inputDeviceUnplugged())：

撤销专属配置

如果该设备参与了某些模式（如抓取）,判断是否需要回退

(4) 安全性与 NULL 处理
当设备拔出后,其 nativeHandle() 返回 nullptr。你在任何定时任务或延后回调中再访问底层结构时,必须先判空：

if (void\* h = dev->nativeHandle()) {
// 安全访问
} else {
// 设备已断开或后端不提供 handle
} 6. 为什么用“能力位”而不是“类型继承”？
一个物理设备可能组合多种功能（触控板 = Pointer + Gestures；某些平板 = Touch + TabletTool）。

使用位标记能让后期增加新功能（如开关、陀螺、摄像头遮挡）时不破坏继承层级。

上层逻辑只需判断“是否具备某能力”,无需进行 dynamic_cast。

## 截图

LScreenshotRequest 类是用于处理截图请求的对象,通常在 Wayland compositors 中使用。其核心功能是处理来自客户端的截图请求,允许客户端捕获当前输出（LOutput）的特定区域,并将其传送到客户端的缓冲区。

以下是对该类及其方法的详细解释：

类作用
LScreenshotRequest 类表示一个截图请求,它请求从一个 LOutput（即显示器或输出设备）捕获一个特定区域的图像。这个请求会被传递给合成器（compositor）以便在 LOutput::paintGL() 事件期间处理。也就是说,每次绘制新帧时,如果有客户端请求截图,该请求将被处理。

关键成员函数
client()

功能：获取发起截图请求的客户端（即哪个程序请求了截图）。

返回值：返回一个指向 LClient 对象的指针,表示发起请求的客户端。

rect()

功能：获取截图请求的区域。

返回值：返回一个 LRect 对象,表示请求捕获的矩形区域（单位是表面坐标,基于输出的位置）。

accept()

功能：响应截图请求,允许或拒绝截图。

参数：accept（布尔值）— true 表示接受截图请求,false 表示拒绝。

注意：此方法只能在 LOutput::paintGL() 事件中调用。每个请求只能接受或拒绝一次,最后一次调用的响应在帧结束时生效。如果未调用 accept() 方法,则默认拒绝截图。

resource()

功能：获取与截图请求相关联的 Wayland 资源。

返回值：返回 Protocols::ScreenCopy::RScreenCopyFrame 类型的资源对象,它与截图请求相关联,并在后续操作中用于传输截图数据。

详细说明
截图请求生命周期：当一个客户端请求截图时,Louvre 合成器会创建一个 LScreenshotRequest 对象来表示该请求。截图请求将在 LOutput::paintGL() 事件中处理,此时所有正在进行的绘制操作会被捕捉。一个客户端可能会在一个 paintGL 事件中多次请求截图,因此 LOutput::screenshotRequests() 中可能会有多个 LScreenshotRequest 对象。

接受或拒绝请求：LScreenshotRequest::accept() 方法是唯一用来响应请求的接口。默认情况下,所有请求都会被拒绝,直到 accept(true) 被调用。如果 accept() 在 paintGL 事件结束前未被调用,则该请求被视为被拒绝。

资源处理：每个截图请求都会关联一个 Wayland 资源 (ScreenCopyFrame),用于将截图数据传输给客户端。此资源可以通过 resource() 方法访问。

使用场景
通常情况下,客户端（例如截图工具、屏幕录制工具等）会通过 Wlr Screencopy Protocol 向合成器发出截图请求。在处理截图时,合成器会检查当前是否有活动的请求,并按请求的区域截取屏幕数据。

## 剪切板

LClipboard 类是一个剪贴板管理器,用于管理和控制客户端对剪贴板的访问。它使得客户端可以请求设置剪贴板内容,并且可以设置某些 MIME 类型的持久性。这个类确保只有获得权限的客户端可以修改剪贴板,并支持在不同客户端之间共享剪贴板数据。

关键功能和成员
LClipboard::MimeTypeFile

结构体：表示剪贴板中的 MIME 类型。包含 MIME 类型字符串（mimeType）和相关文件指针（tmp）,后者可以是 NULL,用于存储剪贴板内容。

setClipboardRequest()

功能：客户端请求设置剪贴板。

参数：LClient \*client：发起请求的客户端；const LEvent &triggeringEvent：触发请求的事件。

返回值：如果返回 true,客户端可以设置剪贴板；如果返回 false,则无法设置剪贴板。

说明：每次只有一个客户端能够设置剪贴板。该方法通过检查事件类型（如指针事件、键盘事件、触摸事件）来决定是否允许该客户端设置剪贴板。

默认实现：只有在指针、键盘或触摸事件的焦点与请求的客户端一致时,才能允许该客户端设置剪贴板。

persistentMimeTypeFilter()

功能：过滤持久化的 MIME 类型,决定哪些 MIME 类型的剪贴板数据在客户端断开连接后依然保持。

参数：const std::string &mimeType：MIME 类型字符串。

返回值：true 表示该 MIME 类型的数据应当持久化,false 表示不持久化。

默认实现：将以下 MIME 类型的数据设为持久化：image/png、text/uri-list、UTF8_STRING、COMPOUND_TEXT、TEXT、STRING、text/plain;charset=utf-8 和 text/plain。

mimeTypes()

功能：返回当前剪贴板中支持的 MIME 类型列表。

返回值：std::vector<LClipboard::MimeTypeFile>：包含当前剪贴板 MIME 类型的向量。

默认实现与行为
setClipboardRequest()：

如果是鼠标指针事件,只有当指针聚焦的表面与客户端相符时,才能允许该客户端设置剪贴板。

如果是键盘事件,只有当键盘聚焦的表面与客户端相符时,才能允许设置剪贴板。

如果是触摸事件,客户端的触摸点与表面关联时,允许设置剪贴板。
