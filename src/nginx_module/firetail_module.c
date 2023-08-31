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

static void *CreateFiretailMainConfig(ngx_conf_t *configuration_object) {
  FiretailMainConfig *http_main_config =
      ngx_pcalloc(configuration_object->pool, sizeof(FiretailMainConfig));
  if (http_main_config == NULL) {
    return NULL;
  }

  ngx_str_t firetail_api_token = ngx_string("");
  http_main_config->FiretailApiToken = firetail_api_token;
 
  // load appspec schema
  FILE *schema;
  char str[8192];

  printf("Loading AppSpec Schema...\n");

  schema = fopen("/usr/local/nginx/appspec.yml","r");
  if(schema == NULL)
  {
     printf("Error! count not load schema");
     exit(1);
  }

  while(fgets(str, sizeof(schema), schema))
  fclose(schema);

  ngx_str_t spec = ngx_string(str);

  http_main_config->FiretailAppSpec = spec;

  return http_main_config;
}

static char *InitFiretailMainConfig(ngx_conf_t *configuration_object,
                                    void *http_main_config) {
  return NGX_CONF_OK;
}

ngx_http_module_t kFiretailModuleContext = {
    NULL,                      // preconfiguration
    FiretailInit,              // postconfiguration
    CreateFiretailMainConfig,  // create main configuration
    InitFiretailMainConfig,    // init main configuration
    NULL,                      // create server configuration
    NULL,                      // merge server configuration
    NULL,                      // create location configuration
    NULL                       // merge location configuration
};

char *EnableFiretailDirectiveInit(ngx_conf_t *configuration_object,
                                  ngx_command_t *command_definition,
                                  void *http_main_config) {
  // TODO: validate the args given to the directive

  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_api_key_field =
      (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_api_key_field = value[1];

  return NGX_CONF_OK;
}

ngx_command_t kFiretailCommands[2] = {
    {// Name of the directive
     ngx_string("firetail_api_token"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     EnableFiretailDirectiveInit, NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(FiretailMainConfig, FiretailApiToken), NULL},
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
