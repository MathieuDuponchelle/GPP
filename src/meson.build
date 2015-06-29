sources = ['gppqueue.c', 'gppworker.c']

gpp = shared_library('gpp',
		     sources,
		     install: true,
		     dependencies: [glib_dep, gobject_dep, gio_dep, zmqlib, czmqlib])
