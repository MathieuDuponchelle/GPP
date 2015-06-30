#ifndef _GPP_UTILS
#define _GPP_UTILS

#include <gio/gio.h>

GIOChannel * g_io_channel_from_zmq_socket (void *socket);

#define HEARTBEAT_LIVENESS  3
#define HEARTBEAT_INTERVAL  G_USEC_PER_SEC

#define PPP_READY       "\001"
#define PPP_HEARTBEAT   "\002"
#define PPP_KO          "\003"

#endif
