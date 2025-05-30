# 问题解决

## Q1: wlroots的源代码编译后安装之后带有版本, 如何通用化?

### 问题描述

如果从源构建并安装wlroots, wlroots会为每个版本生成一个带版本号的安装文件夹, 例如:

```text
/usr/local/include/wlroots-0.19/...
/usr/local/include/wlroots-0.20/...
```
这样我们的头文件中引用就需要指定版本:

```c++
#include <wlroots-0.19/wlr/utils/log.h>
```
这样是非常不灵活的。

### 目标

使得我们在引用头文件时可以无视`wlroots`版本:

```c++
#include <wlr/utils/log.h>
```

### 解决方案

1. 使用`meson`构建系统
2. 参考[waybox的meson.build](https://github.com/wizbright/waybox/blob/master/meson.build), 枚举版本。这样可以尽可能多地兼容不同的`wlroots`版本, 也方便修改。