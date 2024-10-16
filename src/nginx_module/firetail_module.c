#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "filter_context.h"
#include "filter_headers.h"
#include "filter_response_body.h"
#include "firetail_module.h"
#include "filter_firetail_send.h"
#include <json-c/json.h>

#define SIZE 65536

ngx_http_output_header_filter_pt kNextHeaderFilter;
ngx_http_output_body_filter_pt kNextResponseBodyFilter;

typedef struct {
  ngx_int_t status;
} ngx_http_firetail_ctx_t;

static ngx_int_t ngx_http_firetail_handler_internal(ngx_http_request_t *r);
static void ngx_http_firetail_body_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_firetail_handler(ngx_http_request_t *r);

static ngx_int_t ngx_http_firetail_handler_internal(ngx_http_request_t *request) {
  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "✅️✅️✅️HANDLER INTERNAL✅️✅️✅️");

  ngx_chain_t *chain_head;
  chain_head = request->request_body->bufs;

  // Get our context so we can store the request body data
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  // get the header values
  ngx_list_part_t *part;
  ngx_table_elt_t *h;
  ngx_uint_t i;
  part = &request->headers_in.headers.part;
  h = part->elts;
  json_object *log_root = json_object_new_object();
  for (i = 0;; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }
      part = part->next;
      h = part->elts;
      i = 0;
    }
    json_object *jobj = json_object_new_string((char *)h[i].value.data);
    json_object *array = json_object_new_array();
    json_object_array_add(array, jobj);
    json_object_object_add(log_root, (char *)h[i].key.data, array);
  }
  ctx->request_headers_json = (u_char *)json_object_to_json_string(log_root);
  ctx->request_headers_json_size = strlen((char *)ctx->request_headers_json);
  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "json value %s", (char *)ctx->request_headers_json);

  // Determine the length of the request body chain we've been given
  long new_request_body_parts_size = 0;
  for (ngx_chain_t *current_chain_link = chain_head; current_chain_link != NULL;
       current_chain_link = current_chain_link->next) {
    new_request_body_parts_size += current_chain_link->buf->last - current_chain_link->buf->pos;
  }

  // Take note of the request body size before and after adding the new chain
  // & update it in our ctx
  long old_request_body_size = ctx->request_body_size;
  long new_request_body_size = old_request_body_size + new_request_body_parts_size;
  ctx->request_body_size = new_request_body_size;

  // Create a new updated body
  u_char *updated_request_body = ngx_pcalloc(request->pool, new_request_body_size);

  // Copy the body read so far into ctx into our new updated_request_body
  u_char *updated_request_body_i = ngx_copy(updated_request_body, ctx->request_body, old_request_body_size);

  // Iterate over the chain again and copy all of the buffers over to our new
  // request body char*
  for (ngx_chain_t *current_chain_link = chain_head; current_chain_link != NULL;
       current_chain_link = current_chain_link->next) {
    long buffer_length = current_chain_link->buf->last - current_chain_link->buf->pos;
    updated_request_body_i = ngx_copy(updated_request_body_i, current_chain_link->buf->pos, buffer_length);
  }

  // Update the ctx with the new updated body
  ctx->request_body = updated_request_body;

  // Get the main config so we can check if we have 404s disabled from the middleware
  FiretailMainConfig *main_config = ngx_http_get_module_main_conf(request, ngx_firetail_module);

  // run the validation
  void *validator_module = dlopen("/etc/nginx/modules/firetail-validator.so", RTLD_LAZY);
  if (!validator_module) {
    return NGX_ERROR;
  }

  ValidateRequestBody request_body_validator = (ValidateRequestBody)dlsym(validator_module, "ValidateRequestBody");
  char *error;
  if ((error = dlerror()) != NULL) {
    ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Failed to load ValidateRequestBody: %s", error);
    exit(1);
  }
  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Validating request body...");

  struct ValidateRequestBody_return validation_result = request_body_validator(
      main_config->FiretailAllowUndefinedRoutes.data, main_config->FiretailAllowUndefinedRoutes.len, ctx->request_body,
      ctx->request_body_size, request->unparsed_uri.data, request->unparsed_uri.len, request->method_name.data,
      request->method_name.len, (char *)ctx->request_headers_json, ctx->request_headers_json_size);

  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Validation request result: %d", validation_result.r0);
  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Validating request body: %s", validation_result.r1);

  // if validation is unsuccessful, return bad request
  if (validation_result.r0 > 0) return ngx_http_firetail_request(request, NULL, chain_head, validation_result.r1);

  dlclose(validator_module);

  return NGX_OK;  // can be NGX_DECLINED - see ngx_http_mirror_handler_internal
                  // function in nginx mirror module
}

static void ngx_http_firetail_body_handler(ngx_http_request_t *request) {
  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "	✅️✅️✅️RUNNING BODY HANDLER %s✅️✅️✅️",
                request->request_body);

  ngx_http_firetail_handler_internal(request);

  request->preserve_body = 1;

  request->write_event_handler = ngx_http_core_run_phases;
  ngx_http_core_run_phases(request);
}

static ngx_int_t ngx_http_firetail_handler(ngx_http_request_t *r) {
  ngx_int_t rc;
  ngx_http_firetail_ctx_t *ctx;

  if (r != r->main) {
    return NGX_DECLINED;
  }

  ctx = ngx_http_get_module_ctx(r, ngx_firetail_module);

  if (ctx) {
    return ctx->status;
  }

  ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_firetail_ctx_t));
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  ctx->status = NGX_DONE;

  rc = ngx_http_read_client_request_body(r, ngx_http_firetail_body_handler);
  if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
    return rc;
  }

  ngx_http_finalize_request(r, NGX_DONE);
  return NGX_DONE;
}

ngx_int_t FiretailInit(ngx_conf_t *cf) {
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_firetail_handler;

  kNextResponseBodyFilter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = FiretailResponseBodyFilter;

  kNextHeaderFilter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = FiretailHeaderFilter;

  return NGX_OK;
}

static void *CreateFiretailMainConfig(ngx_conf_t *configuration_object) {
  FiretailMainConfig *http_main_config = ngx_pcalloc(configuration_object->pool, sizeof(FiretailMainConfig));
  if (http_main_config == NULL) {
    return NULL;
  }

  ngx_str_t firetail_api_token = ngx_string("");
  ngx_str_t firetail_url = ngx_string("");
  http_main_config->FiretailApiToken = firetail_api_token;
  http_main_config->FiretailUrl = firetail_url;

  return http_main_config;
}

static char *InitFiretailMainConfig(ngx_conf_t *configuration_object, void *http_main_config) { return NGX_CONF_OK; }

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

ngx_command_t kFiretailCommands[4] = {
    {// Name of the directive
     ngx_string("firetail_api_token"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailApiTokenDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET, offsetof(FiretailMainConfig, FiretailApiToken),
     NULL},
    {// Name of the directive
     ngx_string("firetail_url"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailUrlDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET, offsetof(FiretailMainConfig, FiretailUrl), NULL},
    {// Name of the directive
     ngx_string("firetail_allow_undefined_routes"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailAllowUndefinedRoutesDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(FiretailMainConfig, FiretailAllowUndefinedRoutes), NULL},
    ngx_null_command};

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
