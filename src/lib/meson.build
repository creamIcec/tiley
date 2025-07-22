subdir('client')

lib_sources = files(
    'TileyCompositor.cpp',
    'TileyServer.cpp',
    'TileyWindowStateManager.cpp',
    'input/Seat.cpp',
    'output/Output.cpp',
    'scene/Scene.cpp',
    'surface/Surface.cpp',
    'interact/interact.cpp',
    'core/Container.cpp',
    'input/Pointer.cpp'
)

dep_sources = lib_sources + client_sources

# 迁移完成后使用

# sources = run_command('find', './src', '-type', 'f', '-name', '*[.c,.cpp,.h,.hpp]', check : false).stdout().strip().split('\n')