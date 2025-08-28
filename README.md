<center>

# Tiley: Highly Customizable Tiling Desktop Manager

</center>

<center>

[English](./README.md) | [简体中文](./README_zh_cn.md)

</center>

![Tiley Theme](./assets/tiley_wallpaper.png)

Welcome to **tiley**! This is a desktop manager developed based on **Wayland** and **Louvre**. Our goal is to create an efficient, customizable, and user-friendly Wayland compositor.

![Tiley Architecture](./assets/tiley-architecture.png)

## Development Environment Setup

This guide will help you quickly set up the development environment for `tiley` on your machine.

### 1. Clone the Repository

First, clone the main `tiley` repository and initialize submodules like `Louvre`:

```bash
git clone https://github.com/creamIcec/tiley.git
cd tiley
git submodule update --init --recursive
```

### 2. Install Dependencies

`tiley` depends on `Louvre` and its core backend `SRM`, as well as related `Wayland` ecosystem libraries.

**Core Dependencies (expressed in `meson` format)**

```meson
wayland_server_dep  = dependency('wayland-server', version: '>= 1.20.0')
gl_dep              = dependency('gl', version: '>= 1.2')
egl_dep             = dependency('egl', version : '>=1.5')
glesv2_dep          = dependency('glesv2', version: '>= 3.2')
udev_dep            = dependency('libudev', version: '>= 249')
xcursor_dep         = dependency('xcursor', version: '>= 1.2.0')
xkbcommon_dep       = dependency('xkbcommon', version: '>= 1.4.0')
pixman_dep          = dependency('pixman-1', version: '>= 0.40.0')
drm_dep             = dependency('libdrm', version: '>= 2.4.113')
input_dep           = dependency('libinput', version: '>= 1.20.0')
libseat_dep         = dependency('libseat', version: '>= 0.6.4')
srm_dep             = dependency('SRM', version : '>=0.13.0')
pthread_dep         = cpp.find_library('pthread')
dl_dep              = cpp.find_library('dl')
```

**Optional Dependencies (subject to change as project develops)**

- **`zwp_linux_dmabuf_v1`**: To support a broader range of modern applications (such as GNOME applications), enabling DMA-BUF support is recommended. This usually doesn't require additional installation, depending on core libraries `libdrm` and `mesa`.
- **`wayland-protocols`**: Provides a series of standard Wayland extension protocols, enhancing compatibility.

**X11 Compatibility Dependencies**

`tiley` aims to be as compatible as possible with `X11` applications, so the following dependencies need to be installed:

- **xwayland**: Provides X11 compatibility layer (required for compilation, optional at runtime)
- **libxcb**: X protocol C binding
- **libxcb-render-util**: Rendering utilities for `libxcb`
- **libxcb-wm**: Window manager extensions for `libxcb`
- **libxcb-errors**: Error reporting library for `libxcb`, for better readability

**Installation Examples**

**Arch Linux (or Arch-based distributions)**

If you're using Arch Linux, you can install related dependencies through `pacman`. `Louvre` and `SRM` will be built as submodules, no need to install them through package manager.

```bash
sudo pacman -S --needed meson wayland wayland-protocols libdrm libinput libxkbcommon pixman systemd libseat libxcursor mesa libglvnd
```

For `X11` compatibility part:

```bash
sudo pacman -S xorg-xwayland libxcb libxcb-render-util libxcb-wm libxcb-errors
```

**Other Distributions (Debian/Ubuntu/Fedora/SUSE etc.)**

Please install packages corresponding to the above list according to your package manager (some packages may need `-dev` or `-devel` suffix). An efficient strategy is to install dependencies as `meson` reports missing ones during the first compilation.

### 3. Build the Project

`tiley` uses **Meson** as the build system. First enter the project root directory, then:

1.  Configure build directory:

    ```bash
    meson setup build/
    ```

    For development convenience, the `meson.build` file is configured to prioritize submodules. It will automatically find and build code in `subprojects/Louvre` and `subprojects/SRM`, ensuring you're always using framework versions compatible with `tiley`.

2.  Compile the project:

    ```bash
    meson compile -C build/
    ```

### 4. Running

After successful compilation, you can run `tiley`!

`tiley` is a Wayland compositor that can run in two main ways:

1.  Running as a window within an existing desktop environment (such as X11 or another Wayland compositor).
2.  Switch to TTY (usually via `Ctrl+Alt+F3` etc.), log in and run directly, where `tiley` will take over the entire screen.

Either way, the startup command is:

```bash
./build/tiley
```

### 5. Common Issues

- **Q: (Runtime) Getting "cannot find .so library" errors like `libSRM.so` or `libLouvre.so`, what to do?**

  A: This means your system dynamic linker can't find the libraries we just compiled from submodules. Because they're placed in the `build/` directory, not in standard system paths (like `/usr/lib`). The solution is to temporarily add this path to environment variables:

  ```bash
  # SRM is Louvre's core runtime library
  export LD_LIBRARY_PATH=$PWD/build/subprojects/SRM:$LD_LIBRARY_PATH
  ./build/tiley
  ```

  If you don't want to set this manually every time, consider adding it to your `.bashrc` or `.zshrc` during development.

- **Q: (Development) Getting `undefined reference to Louvre::LCompositor::...` linking errors, what to do?**

  A: This issue usually doesn't occur in `Louvre` projects because `Louvre` itself is written in C++, and your `tiley` project is also C++. This means the compiler and linker handle symbols (function names, class names, etc.) consistently, avoiding "name mangling" issues from C/C++ mixed programming.

  If you encounter such linking errors, the cause is likely:

  1.  **Meson configuration issue**: Check your `meson.build` file, ensure you've correctly added `Louvre` and `SRM` dependencies to your `tiley` executable's `dependencies` list.
      ```meson
      # Example
      executable('tiley',
          'src/main.cpp',
          # ... other source files
          dependencies: [louvre_dep, srm_dep, /* other dependencies */],
          install: true)
      ```
  2.  **Submodule build failure**: `meson compile` might have failed when building `Louvre` or `SRM` submodules, but you didn't notice. Please carefully check the compilation log to ensure submodules were successfully compiled into `.so` or `.a` files.

- **Q: (Development) `Louvre` or `SRM` submodule build failed?**

  A: If `meson compile` fails when building submodules, it's almost always due to unsatisfied core dependencies. Please carefully read the error information about `Louvre` or `SRM` subproject builds in `build/meson-logs/meson-log.txt`. The log will clearly tell you which library is missing (like `libseat-devel` or `wayland-protocols`), install it with your package manager according to the prompts.

- **Q: (Runtime) When I open certain applications (like GNOME applications), `tiley` crashes or application windows don't display, and the logs show `Unknown buffer type` or `Failed to convert buffer to OpenGL texture` errors. Why?**

  A: This is a very classic issue! It means your compositor only supports basic `wl_shm` shared memory buffers, while client applications are trying to use more efficient `DMA-BUF` for hardware buffer sharing. `Louvre` doesn't enable all extension protocols by default, you need to manually enable them.

  **Solution**: In your main service class (e.g., `TileyServer.cpp`) initialization code, instantiate `LDMABufProtocol`.

  ```cpp
  // In your TileyServer.h or similar
  #include <Louvre/protocols/LinuxDMABuf/LDMABufProtocol.h>
  #include <memory>

  class TileyServer {
      // ...
  private:
      std::unique_ptr<Louvre::Protocols::LinuxDMABuf::LDMABufProtocol> m_dmaBufProtocol;
  };

  // In your TileyServer.cpp constructor or initialization function
  TileyServer::TileyServer() {
      // ... other initialization ...

      // Enable zwp_linux_dmabuf_v1 protocol support
      m_dmaBufProtocol = std::make_unique<Louvre::Protocols::LinuxDMABuf::LDMABufProtocol>();
  }
  ```

  After recompiling, your compositor will learn how to handle `DMA-BUF`, thus being compatible with more modern applications.

- **Q: How should I debug `tiley`?**

  A: `Louvre` and the Wayland ecosystem provide powerful debugging tools:

  1.  **Louvre logging**: `Louvre` has a built-in `LLog` logging system. You can add detailed debug information in your code via `LLog::debug("My message: %d", my_var);` etc.
  2.  **WAYLAND_DEBUG**: This is the ultimate Wayland debugging tool. By setting this environment variable, you can see all protocol-level communication details between clients and your compositor.
      ```bash
      WAYLAND_DEBUG=1 ./build/tiley
      ```
      This is very helpful for diagnosing protocol implementation issues, client compatibility issues, etc.

---

We hope this guide will make your development journey with `tiley` smooth sailing. If you discover any issues or have any suggestions, feel free to raise Issues or Pull Requests anytime!

**Happy Hacking!**
