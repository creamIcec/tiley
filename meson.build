project(
  'tiley',
  'cpp', 'c',
  meson_version : '>= 1.3.0',
  version : '0.1.0',
  default_options : [
    'warning_level=3',  # 最高警告级别(对应makefile的-Wall -Wextra)
    'cpp_std=c++17',  # 使用c++17标准
    'werror=false'  # 不将警告视为错误, 方便开发
  ],
)

source_root = 'src'

# 将编译参数移到目标声明之前
add_project_arguments(
  '-DWLR_USE_UNSTABLE',
  language: 'cpp'
)

add_project_arguments(
  '-DWLR_USE_UNSTABLE',
  language: 'c'
)

# 确保 C 编译器使用 C99 标准
#add_project_arguments('-std=c99', language: 'c')

# 公共包含文件夹
common_includes = include_directories('include')

# 添加正确的路径
#wayland_protocols_inc = include_directories('/usr/local/include/wayland')
#add_project_arguments('-I/usr/local/include/wayland', language: 'cpp')
#add_project_arguments('-I/usr/local/include/wayland', language: 'c')

# 只使用子模块中的 wlroots
message('只使用子模块中的 wlroots.')

# 获取 wlroots 子项目
wlroots_proj = subproject('wlroots')

# 获取子项目 wlroots 的不完整依赖
incomplete_wlroots_dep = wlroots_proj.get_variable('wlroots')

# 添加协议路径
wlroots_protocol_inc = include_directories('subprojects/wlroots/protocol')

# 创建 wlroots 的依赖项
wlroots_dep = declare_dependency(
  dependencies: [incomplete_wlroots_dep],
  include_directories: [wlroots_protocol_inc]
)

# 收集源文件
all_tiley_sources = []
subdir('src')

# 创建可执行文件
exe = executable(
  'tiley',
  all_tiley_sources,
  dependencies : [wlroots_dep],
  install : true,
  include_directories: common_includes
)

# 进行基本运行测试
test('basic_run_test', exe)