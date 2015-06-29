project('gpp', 'c')

zmqlib = find_library('zmq', required : true)
czmqlib = find_library('czmq', required : true)

glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')
gio_dep = dependency ('gio-2.0')

inc = include_directories ('src')

subdir ('src')
subdir ('tools')
