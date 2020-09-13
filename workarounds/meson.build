
workarounds_lib_headers = ['workarounds.h']
workarounds_lib_sources = []

if host_machine.system() == 'windows'
  workarounds_lib_sources += ['workarounds-spawn-win32.c']
else
  workarounds_lib_sources += ['workarounds-spawn.c']
endif

if host_machine.system() == 'windows'
  # g-ir-compiler with static libraries on Windows isn't very reliable
  workarounds_lib = shared_library('vls-workarounds',
    workarounds_lib_sources,
    install: true,
    dependencies: [dependency('glib-2.0')])
else
  workarounds_lib = static_library('vls-workarounds',
    workarounds_lib_sources,
    dependencies: [dependency('glib-2.0')])
endif

workarounds_dep = declare_dependency(
  link_with: workarounds_lib)

# generate the .gir

gnome = import('gnome')

workarounds_gir = gnome.generate_gir(workarounds_lib,
  sources: [workarounds_lib_sources, workarounds_lib_headers],
  nsversion: '0.1',
  namespace: 'Workarounds',
  symbol_prefix: 'workarounds',
  identifier_prefix: 'Workarounds',
  link_with: [workarounds_lib],
  includes: [ 'GLib-2.0' ],
  install: false,
  header: 'workarounds.h',
  extra_args: ['--warn-all', '--warn-error'],
)

workarounds_vapi = gnome.generate_vapi('workarounds',
  sources: workarounds_gir.get(0),
  install: false,
  packages: [ 'glib-2.0' ],
)

# add the workarounds library to the list of Vala dependencies
deps += [workarounds_dep, workarounds_vapi]
