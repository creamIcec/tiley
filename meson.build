project(
  'tiley',
  'cpp', 'c',
  meson_version : '>= 1.3.0',
  version : '0.1.0',
  default_options : [
    'warning_level=3',  # 最高警告级别(对应makefile的-Wall -Wextra)
    'cpp_std=c++20',  # Louvre使用的c++20标准, 我们的代码不限定版本, 随库更改即可
    'werror=false'  # 不将警告视为错误, 方便开发
  ],
)

fs = import('fs')

# 安装后的目录
install_asset_dir = get_option('datadir') / meson.project_name() / 'assets'
install_config_dir = get_option('datadir') / meson.project_name() / 'config'
# 开发时的目录
source_shader_dir = join_paths(meson.project_source_root(), 'src/assets/shaders')
source_asset_dir = join_paths(meson.project_source_root(), 'src/assets')
source_config_dir = join_paths(meson.project_source_root(), 'src/config')

# 程序安装数据
# shader
install_data(
    'src/assets/shaders/rounded_corners.vert',
    'src/assets/shaders/rounded_corners.frag',
    install_dir: install_asset_dir / 'shaders'
)
# 壁纸
install_data(
    'src/assets/wallpaper/tiley_16x10@2x.png',
    install_dir: install_asset_dir / 'wallpaper'
)
# 快捷键列表
install_data(
    'src/config/hotkey.json',
    install_dir: install_config_dir
)

conf_data = configuration_data()
conf_data.set('SHADER_INSTALL_DIR', join_paths(install_asset_dir, 'shaders'))
conf_data.set('SHADER_SOURCE_DIR', source_shader_dir)
conf_data.set(
    'DEFAULT_WALLPAPER_INSTALL_PATH',
    join_paths(install_asset_dir, 'wallpaper/tiley_16x10@2x.png')
)
conf_data.set(
    'DEFAULT_WALLPAPER_SOURCE_PATH',
    join_paths(source_asset_dir, 'wallpaper/tiley_16x10@2x.png')
)
conf_data.set(
    'DEFAULT_HOTKEY_INSTALL_PATH',
    join_paths(install_config_dir, 'hotkey.json')
)
conf_data.set(
    'DEFAULT_HOTKEY_SOURCE_PATH',
    join_paths(source_config_dir, 'hotkey.json')
)

configure_file(
    input: 'src/config.h.in',
    output: 'config.h',
    configuration: conf_data
)

xkbcommon_dep = dependency('xkbcommon', required: true)
louvre_dep = dependency('Louvre', version: '>=2.18.1')
pixman_dep = dependency('pixman-1')
libinput_dep = dependency('libinput')
sdbus = dependency(['libsystemd', 'libelogind', 'basu'])
wayland_server_dep = dependency('wayland-server') 

source_root = 'src'

add_project_arguments(
  '-DWLR_USE_UNSTABLE',
  language: 'cpp'
)

add_project_link_arguments(
  '-lGLESv2', 
  language: 'cpp'
)

add_project_arguments(
  '-DWLR_USE_UNSTABLE',
  language: 'c'
)

# 公共包含文件夹
common_includes = include_directories(['include', '.'])
# 收集源文件
all_tiley_sources = []
subdir('src')

# 创建可执行文件
exe = executable(
  'tiley',
  all_tiley_sources,
  dependencies : [louvre_dep, pixman_dep, libinput_dep, sdbus, xkbcommon_dep, wayland_server_dep],
  install : true,
  include_directories: common_includes
)

# 进行基本运行测试
test('basic_run_test', exe)