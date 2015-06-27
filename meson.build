project('distributed', 'c')

zmqlib = find_library('zmq', required : true)
czmqlib = find_library('czmq', required : true)

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
	   dependencies: [zmqlib, czmqlib],
	   )

client_sources = ['ppclient.c']

executable('ppclient',
	   client_sources,
	   install: true,
	   dependencies: [zmqlib, czmqlib],
	   )
