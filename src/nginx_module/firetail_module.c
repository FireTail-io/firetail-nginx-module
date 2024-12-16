#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "access_phase_handler.h"
#include "filter_context.h"
#include "filter_headers.h"
#include "filter_response_body.h"
#include "firetail_module.h"
#include <json-c/json.h>

#define SIZE 65536

ngx_http_output_header_filter_pt kNextHeaderFilter;
ngx_http_output_body_filter_pt kNextResponseBodyFilter;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Context

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

static void *CreateFiretailConfig(ngx_conf_t *configuration_object) {
  FiretailConfig *firetail_config = ngx_pcalloc(configuration_object->pool, sizeof(FiretailConfig));
  if (firetail_config == NULL) {
    return NULL;
  }

  ngx_str_t firetail_api_token = ngx_string("");
  ngx_str_t firetail_url = ngx_string("");
  firetail_config->FiretailApiToken = firetail_api_token;
  firetail_config->FiretailUrl = firetail_url;
  firetail_config->FiretailEnabled = 0;

  return firetail_config;
}

static char *InitFiretailMainConfig(ngx_conf_t *configuration_object, void *http_main_config) { return NGX_CONF_OK; }

static char *MergeFiretailLocationConfig(ngx_conf_t *cf, void *parent, void *child) {
  ngx_conf_merge_value(((FiretailConfig *)child)->FiretailEnabled, ((FiretailConfig *)parent)->FiretailEnabled, 0);
  return NGX_CONF_OK;
}

ngx_http_module_t kFiretailModuleContext = {
    NULL,                        // preconfiguration
    FiretailInit,                // postconfiguration
    CreateFiretailConfig,        // create main configuration
    InitFiretailMainConfig,      // init main configuration
    NULL,                        // create server configuration
    NULL,                        // merge server configuration
    CreateFiretailConfig,        // create location configuration
    MergeFiretailLocationConfig  // merge location configuration
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Directives

char *FiretailApiTokenDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                        void *http_main_config) {
  // TODO: validate the args given to the directive

  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_api_key_field = (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_api_key_field = value[1];

  return NGX_CONF_OK;
}

char *FiretailUrlDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                   void *http_main_config) {
  // TODO: validate the args given to the directive

  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_url_field = (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_url_field = value[1];

  return NGX_CONF_OK;
}

char *FiretailAllowUndefinedRoutesDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                                    void *http_main_config) {
  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_allow_undefined_routes_field = (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_allow_undefined_routes_field = value[1];

  return NGX_CONF_OK;
}

char *FiretailEnableDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                      void *http_main_config) {
  // Find the firetail_enable_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_int_t *firetail_enabled_field = (ngx_int_t *)(firetail_config + command_definition->offset);
  *firetail_enabled_field = 1;

  return NGX_CONF_OK;
}

ngx_command_t kFiretailCommands[5] = {
    {// Name of the directive
     ngx_string("firetail_api_token"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailApiTokenDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET, offsetof(FiretailConfig, FiretailApiToken), NULL},
    {// Name of the directive
     ngx_string("firetail_url"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailUrlDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET, offsetof(FiretailConfig, FiretailUrl), NULL},
    {// Name of the directive
     ngx_string("firetail_allow_undefined_routes"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailAllowUndefinedRoutesDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(FiretailConfig, FiretailAllowUndefinedRoutes), NULL},
    {// Name of the directive
     ngx_string("firetail_enable"),
     // Valid in location configs and takes no args
     NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailEnableDirectiveCallback, NGX_HTTP_LOC_CONF_OFFSET, offsetof(FiretailConfig, FiretailEnabled), NULL},
    ngx_null_command};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Module

ngx_module_t ngx_firetail_module = {NGX_MODULE_V1,
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
