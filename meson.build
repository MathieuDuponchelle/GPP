project('distributed', 'c')

zmqlib = find_library('zmq', required : true)
czmqlib = find_library('czmq', required : true)

glib_dep = dependency('glib-2.0')
gio_dep = dependency ('gio-2.0')

worker_sources = ['ppworker.c']

executable('ppworker',
	   worker_sources,
	   install: true,
	   dependencies: [zmqlib, czmqlib],
	   )

queue_sources = ['ppqueue.c']

executable('ppqueue',
	   queue_sources,
	   install: true,
	   dependencies: [zmqlib, czmqlib, glib_dep, gio_dep],
	   )

client_sources = ['ppclient.c']

executable('ppclient',
	   client_sources,
	   install: true,
	   dependencies: [zmqlib, czmqlib],
	   )
