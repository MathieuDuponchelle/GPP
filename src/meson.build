gnome = import ('gnome')

sources = ['gppqueue.c', 'gppworker.c', 'gppclient.c', 'gpputils.c']
headers = ['gppqueue.h', 'gppworker.h', 'gppclient.h', 'gpp.h']

install_headers(headers)

libgpp = shared_library('gpp',
		     sources,
		     version: '1.0',
		     install: true,
		     dependencies: [glib_dep, gobject_dep, gio_dep, zmqlib, czmqlib])

if not get_option('disable-introspection')
	girtargets = gnome.generate_gir(libgpp,
	  sources : sources + headers,
	  nsversion : '1.0',
	  namespace : 'GPP',
	  symbol_prefix : 'gpp_',
	  identifier_prefix : 'GPP',
	  export_packages : 'gpp',
	  includes : ['GObject-2.0'],
	  dependencies: [],
	  install : true
	)
endif

pkgconfig_gen(libraries : libgpp,
	      version : '1.0',
	      name : 'libgpp',
	      filebase : 'gpp',
	      subdirs : '.',
	      description : 'A paranoid pirate pattern gobject implementation')
