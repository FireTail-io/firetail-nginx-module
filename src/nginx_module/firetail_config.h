#include <ngx_core.h>

typedef struct {
  ngx_str_t FiretailApiToken;  // TODO: this should probably be a *ngx_str_t
  ngx_str_t FiretailUrl;
  ngx_str_t FiretailAllowUndefinedRoutes;
  ngx_int_t FiretailEnabled;
} FiretailConfig;
