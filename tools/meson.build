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
	   dependencies: [gobject_dep],
	   link_with: [gpp],
	   include_directories: inc
	   )

client_sources = ['ppclient.c']

executable('ppclient',
	   client_sources,
	   install: true,
	   dependencies: [zmqlib, czmqlib],
	   )
