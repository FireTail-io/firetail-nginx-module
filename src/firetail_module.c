#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "filter_context.h"
#include "filter_headers.h"
#include "filter_request_body.h"
#include "filter_response_body.h"
#include "firetail_module.h"

ngx_http_output_header_filter_pt kNextHeaderFilter;
ngx_http_output_body_filter_pt kNextResponseBodyFilter;
ngx_http_request_body_filter_pt kNextRequestBodyFilter;

ngx_int_t FiretailInit(ngx_conf_t *cf) {
  kNextRequestBodyFilter = ngx_http_top_request_body_filter;
  ngx_http_top_request_body_filter = FiretailRequestBodyFilter;

  kNextResponseBodyFilter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = FiretailResponseBodyFilter;

  kNextHeaderFilter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = FiretailHeaderFilter;

  return NGX_OK;
}

ngx_http_module_t kFiretailModuleContext = {
    NULL,  // preconfiguration
    // Filters are added in the postconfiguration step
    FiretailInit,  // postconfiguration
    NULL,          // create main configuration
    NULL,          // init main configuration
    NULL,          // create server configuration
    NULL,          // merge server configuration
    NULL,          // create location configuration
    NULL           // merge location configuration
};

char *EnableFiretailDirectiveInit(ngx_conf_t *cf, ngx_command_t *cmd,
                                  void *conf) {
  // TODO: validate the args given to the directive
  // TODO: change the key arg to be the name of an env var and get it from the
  // environment
  return NGX_OK;
}

ngx_command_t kFiretailCommands[2] = {
    {// Name of the directive
     ngx_string("enable_firetail"),
     // Valid in the main config, server & location configs; and takes two args
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_TAKE2,
     // A callback function to be called when the directive is found in the
     // configuration
     EnableFiretailDirectiveInit, 0, 0, NULL},
    ngx_null_command};

ngx_module_t ngx_firetail_module = {
    NGX_MODULE_V1,
    &kFiretailModuleContext, /* module context */
    kFiretailCommands,       /* module directives */
    NGX_HTTP_MODULE,         /* module type */
    NULL,                    /* init master */
    NULL,                    /* init module */
    NULL,                    /* init process */
    NULL,                    /* init thread */
    NULL,                    /* exit thread */
    NULL,                    /* exit process */
    NULL,                    /* exit master */
    NGX_MODULE_V1_PADDING};