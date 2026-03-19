project('cliphist-picker', 'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

gnome = import('gnome')

gtk4_dep        = dependency('gtk4', version: '>= 4.0')
layer_shell_dep = dependency('gtk4-layer-shell-0')

resources = gnome.compile_resources(
  'cliphist-picker-resources',
  'data/cliphist-picker.gresource.xml',
  source_dir: 'data',
  c_name: 'cliphist_picker',
)

sources = files(
  'src/main.c',
  'src/clipboard.c',
  'src/dedup.c',
  'src/row.c',
  'src/window.c',
)

executable('cliphist-picker',
  sources + resources,
  dependencies: [gtk4_dep, layer_shell_dep],
  install: true,
)

# Benchmark (no GUI, just data path)
gio_dep = dependency('gio-2.0')
executable('cliphist-picker-bench',
  files('src/bench.c', 'src/clipboard.c', 'src/dedup.c'),
  dependencies: [gio_dep],
  install: false,
)
