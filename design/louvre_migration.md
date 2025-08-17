| wlroots 概念 (您自己管理)         | Louvre 概念 (框架提供,您来继承和重写)                  |
| --------------------------------- | ------------------------------------------------------ |
| wlr_backend                       | LCompositor                                            |
| wlr_output                        | LOutput                                                |
| wlr_seat                          | LSeat                                                  |
| wlr_surface                       | LSurface                                               |
| wlr_xdg_toplevel                  | LToplevel                                              |
| wlr_scene_tree, wlr_scene_node    | Louvre 的内置渲染器 (您只需 surface->setPos())         |
| 信号/监听器 (e.g., wl_signal_add) | 虚函数重写 (e.g., virtual void keyEvent(...) override) |
| 大量的 Boilerplate 代码           | 框架处理,您只需关注核心逻辑                            |
