#include <ngx_http.h>
#include "filter_context.h"
#include "firetail_module.h"

FiretailFilterContext *GetFiretailFilterContext(ngx_http_request_t *request) {
  FiretailFilterContext *ctx =
      ngx_http_get_module_ctx(request, ngx_firetail_module);
  if (ctx == NULL) {
    ctx = ngx_pcalloc(request->pool, sizeof(FiretailFilterContext));
    if (ctx == NULL) {
      return NULL;
    }
    ngx_http_set_ctx(request, ctx, ngx_firetail_module);
  }
  return ctx;
}
