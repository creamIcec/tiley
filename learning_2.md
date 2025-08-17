一、整体调用流程（从启动到交互到渲染）
启动 & 初始化

Compositor（或 Louvre::Compositor 单例）启动,读取 Wayland 连接参数。

它创建并管理若干 Output（物理屏幕）和 Seat（输入座席）。

每个 Output 负责一块屏幕的 OpenGL 上下文以及对应的渲染流。

Compositor 还初始化一个全局 Scene,用于在每个 Output 上合成各 Surface。

创建 & 映射窗口

当客户端通过 Wayland 请求新建一个 toplevel 窗口时,Louvre 内部会构造一个 Surface（或派生自 LOutputSurface）对象。

Surface 维护了它的“视图”(View)——即绘制内容的节点,以及一组“角色”（role,如 ToplevelRole）来响应配置事件。

Surface 注册到 Scene,并根据所处的 Output 的可用几何被添加到对应图层（layer）中。

输入分发 & 焦点管理

Seat 代表一组输入设备（键盘、鼠标）,内部持有 LKeyboard、LPointer 等接口。

当用户移动鼠标或按键时,Compositor 会先把事件发给 Seat,再由 Seat 根据当前焦点的 Surface 决定分发给哪扇窗。

调用 Surface::raise()、Seat::keyboard()->setFocus()、Seat::pointer()->setFocus() 等方法,切换活动窗口。

渲染 & 合成

每帧开始时,Output::paintGL() 被 Wayland/Louvre 调度。

它询问 Scene：“在这块屏幕上,我该如何合成这些 Surface？”

Scene 按 layer（Background → Tiled → Top → Overlay…）遍历每个 Surface 对应的 View,调用它们的 paint(),最终通过 OpenGL/DMA 绘制到屏幕上。

如果发现有独占全屏窗口,Output 会优先尝试 Direct Scanout,跳过合成流程,直接把客户端缓冲区送到硬件。

二、核心类职责一览
类名 职责概要
Louvre::Compositor 全局调度中心：启动 Wayland,管理 Output、Seat、Scene,路由输入事件与配置请求。
TileyServer 项目封装的“服务端单例”：保存对 Compositor、Scene、各 layer 容器的引用,提供统一入口。
Output 代表物理屏幕：OpenGL 初始化 (initializeGL)、每帧合成 (paintGL)、配置变更 (resizeGL…)。
Seat 代表一组输入设备：管理 LKeyboard、LPointer、焦点 Surface 分发、输入事件转发。
Surface 代表一个窗口实例：维护 its Role（如 ToplevelRole）、View（渲染节点）、配置状态。
ToplevelRole Surface 的“角色”之一,处理 atomsChanged、configureRequest 等 XDG 协议回调。
View（LView 系列） 渲染树节点：LSolidColorView、LTextureView、LTextureView（壁纸）等,支持变换与绘制。
Scene 合成逻辑：维护多个 layer,每个 layer 下有若干 View,按先后顺序执行初始化、绘制与移动。
LPointer / LKeyboard 输入接口：封装对鼠标指针与键盘硬件/协议的抽象,为 Seat 提供 focus()、setFocus() 等。
Compositor::layer() Scene or TileyServer 提供的接口,返回某一 layer 的 View 列表,供 Output 渲染。

三、类与类之间如何协作
Compositor ↔ Output

Compositor 在启动时创建 n 个 Output,并将 Wayland 分配的屏幕句柄传入其构造函数。

输出变化时（新屏、拔屏、模式切换）,Compositor 调用 Output::initializeGL / resizeGL / uninitializeGL。

Compositor ↔ Seat

Seat 由 Compositor 创建并持有,绑定到 Wayland 的各输入接口。

Compositor 将所有来自 Wayland 的键鼠事件先路由到 Seat。

Output ↔ Scene

Output::initializeGL / paintGL / moveGL / resizeGL 会调用 server.scene().handleXXX(this),告知 Scene “对这个屏幕执行哪步合成操作”。

Scene 会从 TileyServer 拿到每个 layer 对应的 View 数组,然后在内部管理它们的生命周期与渲染次序。

Surface ↔ Scene

每次客户端映射/重绘时,ToplevelRole::configureRequest 会将 Surface 加入或调整到某个 layer。

Scene 只关心 View,而每个 Surface 拥有一个或多个 View,负责真正的绘制。

Seat ↔ Surface

Seat 中的 keyboard()->focus() / pointer()->focus() 返回当前聚焦的 Surface（或其基类接口）。

当用户点击窗口边框或通过快捷切换时,focusWindow()（我们前面分析的 tiley 模块）会调用 Seat::keyboard()->setFocus(surface),并通过 surface->raise() 改变 Z-order。

Surface ↔ View

Surface 持有一个或多个 LView：

内容层：用户缓冲区 LTextureView；

装饰层：由 Louvre 绘制的边框/标题栏 LSolidColorView；

动画/特效层：如 fadeInView。

View 提供位置、尺寸、裁剪矩形等属性,并在 Scene 调用 paint() 时合成到屏幕。

四、数据流 & 事件流示意
客户端 → Wayland → Compositor

新建窗口 → ToplevelRole::configureRequest

移动/缩放/装饰请求 → ToplevelRole::events

用户输入 → Wayland → Compositor → Seat → Surface via focus()

每帧合成 → Output.paintGL() → Scene.handlePaintGL() → 遍历各 layer → 各 View.paint() → GPU

屏幕变更 → Compositor 调用 → Output.resizeGL() → Scene.handleResizeGL() → 更新 layer & wallpaper

## 概念

└─ Scene
├─ Layer[Background]
│ └─ RootView (LayerRootView)
│ └─ wallpaperView (in its children)
|-- Layer[Bottom]
| |- RootView (LayerBottomView)
| |- 桌面时钟小部件等等
├─ Layer[Tiled]
│ └─ RootView
│ ├─ SurfaceViewA
│ └─ SurfaceViewB
| |-- SurfaceView(Dialog)(一个窗口的弹窗, 不是在 Top, 是在这一层)
│ └─ decorationViewA(这个是以后要加的)
│  
 ├─ Layer[Top]
│ └─ RootView
│ └─ 输入法弹窗, 系统弹窗, 顶栏\
 └─ Layer[Overlay]
└─ RootView
└─ fadeInView, screenshotOverlays…
LLayerBackground (背景层)

    壁纸和桌面背景：桌面环境的背景图片或颜色
    根窗口内容：一些简单合成器的基础背景surface
    系统级背景元素：如登录界面的背景

LLayerBottom (底层)

    桌面图标：文件管理器在桌面上显示的图标和文件
    桌面小部件：如时钟、天气预报等桌面组件
    窗口管理器的装饰性元素：某些特殊的装饰surface

LLayerMiddle (中间层)

    普通应用窗口：大部分应用程序的主窗口
    文档编辑器、浏览器、文件管理器等常规应用
    对话框和模态窗口：应用程序的设置窗口、文件选择器等
    子窗口：应用程序的工具栏、侧边栏等

LLayerTop (顶层)

    系统通知：桌面通知弹窗
    系统对话框：如权限请求、系统设置对话框
    窗口管理器的UI元素：Alt+Tab切换器、窗口预览等
    输入法窗口：候选词列表、输入法状态指示器
    上下文菜单：右键菜单、应用程序菜单

LLayerOverlay (覆盖层)

    全屏覆盖：屏幕保护程序、锁屏界面
    系统级UI：电源管理对话框、紧急模式界面
    调试和开发工具：性能监控覆盖、开发者工具
    可访问性工具：屏幕放大镜、屏幕阅读器的visual feedback
    鼠标指针：在某些实现中,鼠标cursor可能位于此层
