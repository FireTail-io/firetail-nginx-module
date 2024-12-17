#include <ngx_config.h>
#include <ngx_http.h>
#include "access_phase_handler.h"
#include "filter_headers.h"
#include "filter_response_body.h"
#include "firetail_config.h"

ngx_http_output_header_filter_pt kNextHeaderFilter;
ngx_http_output_body_filter_pt kNextResponseBodyFilter;

ngx_int_t FiretailInit(ngx_conf_t *cf) {
  // The FireTail module consists of:
  // - an access phase handler,
  // - a response body filter, and
  // - a header filter

  ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  ngx_http_handler_pt *h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }
  *h = FiretailAccessPhaseHandler;

  kNextResponseBodyFilter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = FiretailResponseBodyFilter;

  kNextHeaderFilter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = FiretailHeaderFilter;

  return NGX_OK;
}

void *CreateFiretailConfig(ngx_conf_t *configuration_object) {
  FiretailConfig *firetail_config = ngx_pcalloc(configuration_object->pool, sizeof(FiretailConfig));
  if (firetail_config == NULL) {
    return NULL;
  }

  ngx_str_t firetail_api_token = ngx_string("");
  ngx_str_t firetail_url = ngx_string("");
  firetail_config->FiretailApiToken = firetail_api_token;
  firetail_config->FiretailUrl = firetail_url;
  firetail_config->FiretailEnabled = NGX_CONF_UNSET;

  return firetail_config;
}

char *InitFiretailMainConfig(ngx_conf_t *configuration_object, void *http_main_config) { return NGX_CONF_OK; }

char *MergeFiretailLocationConfig(ngx_conf_t *cf, void *parent, void *child) {
  ngx_conf_merge_value(((FiretailConfig *)child)->FiretailEnabled, ((FiretailConfig *)parent)->FiretailEnabled, 1);
  return NGX_CONF_OK;
}
