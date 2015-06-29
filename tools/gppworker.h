#ifndef _GPP_WORKER
#define _GPP_WORKER

#include <glib-object.h>

G_BEGIN_DECLS

#define GPP_TYPE_WORKER (gpp_worker_get_type ())

G_DECLARE_FINAL_TYPE(GPPWorker, gpp_worker, GPP, WORKER, GObject)

GPPWorker * gpp_worker_new (void);
gboolean gpp_worker_start (GPPWorker *self);

#endif
