worker_sources = ['ppworker.c']

executable('ppworker',
	   worker_sources,
	   dependencies: [gobject_dep, czmqlib],
	   link_with: [libgpp],
	   include_directories: inc
	   )

queue_sources = ['ppqueue.c']

executable('ppqueue',
	   queue_sources,
	   dependencies: [gobject_dep],
	   link_with: [libgpp],
	   include_directories: inc
	   )

client_sources = ['ppclient.c']

executable('ppclient',
	   client_sources,
	   dependencies: [gobject_dep],
	   link_with: [libgpp],
	   include_directories: inc
	   )
