project(
  'imv',
  ['c'],
  version: '4.1.0',
  license: 'MIT',
  meson_version: '>= 0.47',
  default_options: ['buildtype=debugoptimized', 'c_std=c99'],
)

version = '"@0@"'.format(meson.project_version())
prog_git = find_program('git', required: false)
if prog_git.found()
  git_description = run_command([prog_git.path(), 'describe', '--dirty', '--always', '--tags'])
  if git_description.returncode() == 0
    version = git_description.stdout().strip()
  endif
endif
add_project_arguments('-DIMV_VERSION="@0@"'.format(version), language: 'c')

add_project_arguments('-D_XOPEN_SOURCE=700', language: 'c')

cc = meson.get_compiler('c')
dep_null = dependency('', required: false)

_windows = get_option('windows')
if _windows == 'wayland'
  build_wayland = true
  build_x11 = false
  target_single_ws = true
elif _windows == 'x11'
  build_wayland = false
  build_x11 = true
  target_single_ws = true
else
  build_wayland = true
  build_x11 = true
  target_single_ws = false
endif

deps_for_imv = [
  dependency('pangocairo'),
  dependency('gl'),
  dependency('threads'),
  dependency('xkbcommon'),
  dependency('icu-io'),
]

files_common = files(
  'src/binds.c',
  'src/bitmap.c',
  'src/canvas.c',
  'src/commands.c',
  'src/console.c',
  'src/image.c',
  'src/reload.c',
  'src/imv.c',
  'src/ini.c',
  'src/ipc.c',
  'src/ipc_common.c',
  'src/keyboard.c',
  'src/list.c',
  'src/log.c',
  'src/navigator.c',
  'src/source.c',
  'src/viewport.c',
)

files_imv = files_common + files(
  'src/main.c',
)

enabled_window_systems = []

if build_wayland
  files_wayland = files('src/wl_window.c', 'src/xdg-shell-protocol.c')
  deps_for_wayland = [
    dependency('wayland-client'),
    dependency('wayland-egl'),
    dependency('egl'),
    cc.find_library('rt'),
  ]
  enabled_window_systems += 'wayland'
else
  deps_for_wayland = dep_null
endif

if build_x11
  files_x11 = files('src/x11_window.c')
  deps_for_x11 = [
    dependency('x11'),
    dependency('gl'),
    dependency('glu'),
    dependency('xcb'),
    dependency('xkbcommon-x11'),
  ]
  enabled_window_systems += 'x11'
else
  deps_for_x11 = dep_null
endif

files_msg = files('src/imv_msg.c', 'src/ipc_common.c')

enabled_backends = []
foreach backend : [
  ['freeimage', 'library', 'freeimage'],
  ['libtiff', 'library', 'tiff'],
  ['libpng', 'dependency', 'libpng', []],
  ['libjpeg', 'dependency', 'libturbojpeg', []],
  ['librsvg', 'dependency', 'librsvg-2.0', '>= 2.44'],
  ['libnsgif', 'dependency', 'libnsgif', []],
  ['libheif', 'dependency', 'libheif', []],
]
  _backend_name = backend[0]
  _dep_type = backend[1]
  _dep_name = backend[2]

  if _dep_type == 'dependency'
    _dep = dependency(_dep_name, required: get_option(_backend_name), version: backend[3])
  elif _dep_type == 'library'
    _dep = cc.find_library(_dep_name, required: get_option(_backend_name))
  else
    error('invalid dep type: @0@'.format(_dep_type))
  endif

  if _dep.found()
    deps_for_imv += _dep
    files_imv += files('src/backend_@0@.c'.format(_backend_name))
    add_project_arguments('-DIMV_BACKEND_@0@'.format(_backend_name.to_upper()), language: 'c')
    enabled_backends += _backend_name
  endif
endforeach

executable(
  'imv-msg',
  [files_common, files('src/imv_msg.c', 'src/dummy_window.c')],
  dependencies: deps_for_imv,
  install: true,
  install_dir: get_option('bindir'),
)

foreach ws : ['wayland', 'x11']
  if get_variable('build_' + ws)
    executable(
      target_single_ws ? 'imv' : 'imv-@0@'.format(ws),
      [get_variable('files_' + ws), files_imv],
      dependencies: [deps_for_imv, get_variable('deps_for_' + ws)],
      install: true,
      install_dir: get_option('bindir'),
    )
  endif
endforeach

if not target_single_ws
  install_data(
    files('files/imv'),
    install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x',
  )
endif

install_data(
  files('files/imv.desktop'),
  install_dir: '@0@/applications'.format(get_option('datadir')),
  install_mode: 'rw-r--r--',
)

install_data(
  files('files/imv_config'),
  install_dir: get_option('sysconfdir'),
  install_mode: 'rw-r--r--',
)

dep_cmocka = dependency('cmocka')

foreach test : ['list', 'navigator']
  test(
    'test_@0@'.format(test),
    executable(
      'test_@0@'.format(test),
      [files('test/@0@.c'.format(test), 'src/dummy_window.c'), files_common],
      include_directories: include_directories('src'),
      dependencies: [deps_for_imv, dep_cmocka],
    )
  )
endforeach

prog_a2x = find_program('a2x')

foreach man : [
  [1, 'imv'],
  [1, 'imv-msg'],
  [5, 'imv'],
]
  _section = man[0]
  _topic = man[1]
  custom_target(
    '@0@(@1@)'.format(_topic, _section),
    input: 'doc/@0@.@1@.txt'.format(_topic, _section),
    output: '@0@.@1@'.format(_topic, _section),
    command: [
      prog_a2x,
      '--no-xmllint',
      '--doctype', 'manpage',
      '--format', 'manpage',
      '--destination-dir', meson.current_build_dir(),
      '@INPUT@'
    ],
    install: true,
    install_dir: '@0@/man@1@'.format(get_option('mandir'), _section)
  )
endforeach

message('\n\n'
+ 'Building imv @0@\n\n'.format(meson.project_version())
+ 'Window systems enabled:\n- ' + '\n- '.join(enabled_window_systems) + '\n\n'
+ 'Backends enabled:\n- ' + '\n- '.join(enabled_backends) + '\n'
)
