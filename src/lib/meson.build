subdir('client')

lib_sources = files(
    'TileyCompositor.cpp',
    'TileyServer.cpp',
    'TileyWindowStateManager.cpp',
    'input/Seat.cpp',
    'input/Keyboard.cpp',
    'input/Pointer.cpp',
    'input/ShortcutManager.cpp',
    'output/Output.cpp',
    'scene/Scene.cpp',
    'surface/Surface.cpp',
    'core/Container.cpp',
    'core/UserAction.cpp',
    'test/PerformanceMonitor.cpp',
    'test/PerfmonRegistry.cpp'
    'ipc/IPCManager.cpp',
    'Utils.cpp'
)

dep_sources = lib_sources + client_sources

# 迁移完成后使用

# sources = run_command('find', './src', '-type', 'f', '-name', '*[.c,.cpp,.h,.hpp]', check : false).stdout().strip().split('\n')