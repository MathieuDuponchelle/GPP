#ifndef _GPP_WORKER
#define _GPP_WORKER

#include <glib-object.h>

G_BEGIN_DECLS

#define GPP_TYPE_WORKER (gpp_worker_get_type ())

G_DECLARE_DERIVABLE_TYPE(GPPWorker, gpp_worker, GPP, WORKER, GObject)

struct _GPPWorkerClass
{
  GObjectClass parent_class;

  /**
   * GPPWorkerClass::handle_request:
   * @self: the #GPPWorker
   * @request: The request to handle
   *
   * Implement this method to handle requests, requests *MUST* be
   * handled asynchronously as this method should not block, call
   * gpp_worker_set_task_done() when the request has been handled.
   *
   * Returns: %TRUE if the worker can handle that request, %FALSE otherwise.
   */
  gboolean (*handle_request) (GPPWorker *self, const gchar *request);
};

GPPWorker * gpp_worker_new (void);
gboolean gpp_worker_start (GPPWorker *self);
gboolean gpp_worker_set_task_done (GPPWorker *self, const gchar *reply, gboolean success);

#endif
