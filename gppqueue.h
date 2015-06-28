#ifndef _GPP_QUEUE
#define _GPP_QUEUE

#include <glib-object.h>

G_BEGIN_DECLS

#define GPP_TYPE_QUEUE (gpp_queue_get_type ())

G_DECLARE_FINAL_TYPE(GPPQueue, gpp_queue, GPP, QUEUE, GObject)

GPPQueue * gpp_queue_new (void);

#endif
