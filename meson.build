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

# 安装后的着色器目录
install_shader_dir = get_option('datadir') / meson.project_name() / 'shaders'

# 开发时的着色器目录
source_shader_dir = join_paths(meson.project_source_root(), 'src/assets/shaders')

# 程序安装数据
install_data(
    'src/assets/shaders/rounded_corners.vert',
    'src/assets/shaders/rounded_corners.frag',
    install_dir: install_shader_dir
)

conf_data = configuration_data()
conf_data.set('SHADER_INSTALL_DIR', install_shader_dir)
conf_data.set('SHADER_SOURCE_DIR', source_shader_dir)

configure_file(
    input: 'src/config.h.in',
    output: 'config.h',
    configuration: conf_data
)


louvre_dep = dependency('Louvre', version: '>=2.18.1')
pixman_dep = dependency('pixman-1')
libinput_dep = dependency('libinput')
sdbus = dependency(['libsystemd', 'libelogind', 'basu'])


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
  dependencies : [louvre_dep, pixman_dep, libinput_dep, sdbus],
  install : true,
  include_directories: common_includes
)

# 进行基本运行测试
test('basic_run_test', exe)