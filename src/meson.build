vls_src = files([
  'list_symbols.vala',
  'codehelp/codehelp.vala',
  'codehelp/completionengine.vala',
  'codehelp/find_scope.vala',
  'codehelp/find_symbol.vala',
  'codehelp/signaturehelpengine.vala',
  'codehelp/symbolextractor.vala',
  'codehelp/symbolreferences.vala',
  'codehelp/symbolvisitor.vala',
  'documentation/cnamemapper.vala',
  'documentation/doccomment.vala',
  'projects/buildtarget.vala',
  'projects/buildtask.vala',
  'projects/ccproject.vala',
  'projects/compilation.vala',
  'projects/defaultproject.vala',
  'projects/mesonproject.vala',
  'projects/project.vala',
  'projects/textdocument.vala',
  'projects/types.vala',
  'protocol.vala',
  'reporter.vala',
  'server.vala',
  'servertypes.vala',
])

vala_args = []

if get_option('parse_system_girs')
  vls_src += 'documentation/girdocumentation.vala'
  vala_args += ['--define=PARSE_SYSTEM_GIRS']
endif

if get_option('active_parameter')
  vala_args += ['--define=VALA_FEATURE_INITIAL_ARGUMENT_COUNT']
endif

executable('vala-language-server',
  dependencies: deps,
  sources: [vls_src, conf_file, extra_vala_sources],
  c_args: ['-DG_LOG_DOMAIN="vls"'],
  vala_args: vala_args,
  install: true)
