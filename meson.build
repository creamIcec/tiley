project(
  'tiley',
  'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1.0',
  default_options : [
    'warning_level=3', # 最高警告级别(对应makefile的-Wall -Wextra)
    'cpp_std=c++17', # 使用c++17标准
    'werror=false'  # 不将警告视为错误, 方便开发
  ],
)

source_root = 'src' 

# 启用不稳定的api链接支持, wlroots的锅

add_project_arguments(
  '-DWLR_USE_UNSTABLE',
  language: 'cpp'
)

# 决定我们依赖库灵活性的地方
# 具体原因和思路见design/thought.md中的Q1

# 指定查找wlroots的地方

# 1. 首先尝试系统安装的wlroots

# 注意required是false, 方便后面继续查找而不会找不到直接挂掉
_system_wlroots_dep = dependency('wlroots', version: '>=0.18.0', required: false)

if not _system_wlroots_dep.found()
  _versioned_wlroots_names = ['wlroots-0.20', 'wlroots-0.19', 'wlroots-0.18']
  foreach _vname : _versioned_wlroots_names
    if not _system_wlroots_dep.found() # 没找到系统库
      _system_wlroots_dep = dependency(_vname, version: '>=0.18.0', required: false)
      if _system_wlroots_dep.found()
        message('找到了系统提供的 wlroots: ' + _vname + ' (version: ' + system_wlroots_dep.version() + ')')
      endif
    endif
  endforeach
endif

# 2. 根据有没有系统库来使用对应依赖
if _system_wlroots_dep.found()
  wlroots_dep = _system_wlroots_dep
  message('使用系统提供的wlroots.')
else
  message('找不到系统提供的wlroots. 使用子模块的wlroots.')
  wlroots_proj = subproject('wlroots')

  wlroots_dep = wlroots_proj.get_variable('wlroots')

  if not wlroots_dep.version().version_compare('>=0.18.0')
    error('子模块中的wlroots版本过旧: ' + wlroots_dep.version() + ', 请先运行git submodule update --init --recursive更新到 >= 0.18.0 的版本')
  endif
  message('使用子模块的wlroots.')
endif

exe = executable(
  'tiley',
  files(source_root / 'tiley.cpp'),
  dependencies : [wlroots_dep],
  install : true,
)

test('basic_run_test', exe)
