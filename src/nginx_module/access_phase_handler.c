#include <ngx_core.h>
#include <ngx_http.h>
#include "filter_context.h"
#include "firetail_config.h"
#include "firetail_module.h"
#include <json-c/json.h>

static void FiretailClientBodyHandler(ngx_http_request_t *request);
static ngx_int_t FiretailClientBodyHandlerInternal(ngx_http_request_t *request);
static ngx_int_t FiretailReturnFailedValidationResult(ngx_http_request_t *request, ngx_buf_t *b,
                                                      ngx_chain_t *chain_head, char *error);

struct ValidateRequestBody_return {
  int r0;
  char *r1;
};
typedef struct ValidateRequestBody_return (*ValidateRequestBody)(void *, int, void *, int, void *, int, void *, int,
                                                                 void *, int);

typedef struct {
  ngx_int_t status;
} ngx_http_firetail_ctx_t;

ngx_int_t FiretailAccessPhaseHandler(ngx_http_request_t *r) {
  // Check if FireTail is enabled for this location; if not, skip this handler
  FiretailConfig *location_config = ngx_http_get_module_loc_conf(r, ngx_firetail_module);
  if (location_config->FiretailEnabled == 0) {
    return NGX_DECLINED;
  }

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

  rc = ngx_http_read_client_request_body(r, FiretailClientBodyHandler);
  if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
    return rc;
  }

  ngx_http_finalize_request(r, NGX_DONE);
  return NGX_DONE;
}

static void FiretailClientBodyHandler(ngx_http_request_t *request) {
  FiretailClientBodyHandlerInternal(request);
  request->preserve_body = 1;
  request->write_event_handler = ngx_http_core_run_phases;
  ngx_http_core_run_phases(request);
}

static ngx_int_t FiretailClientBodyHandlerInternal(ngx_http_request_t *request) {
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
  FiretailConfig *main_config = ngx_http_get_module_main_conf(request, ngx_firetail_module);

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
  if (validation_result.r0 > 0)
    return FiretailReturnFailedValidationResult(request, NULL, chain_head, validation_result.r1);

  dlclose(validator_module);

  return NGX_OK;  // can be NGX_DECLINED - see ngx_http_mirror_handler_internal
                  // function in nginx mirror module
}

static ngx_int_t FiretailReturnFailedValidationResult(ngx_http_request_t *request, ngx_buf_t *b,
                                                      ngx_chain_t *chain_head, char *error) {
  ngx_chain_t out;
  char *code;
  struct json_object *jobj;
  ngx_int_t rc;

  FiretailFilterContext *ctx = GetFiretailFilterContext(request);

  char empty_json[2] = "{}";
  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "INCOMING Request Body: %s, json %s", ctx->request_body,
                empty_json);

  if (b == NULL) {
    ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Buffer for REQUEST is null", NULL);

    // bypass response validation because we no longer need
    // to go through response validation
    ctx->bypass_response = 0;
    if (ctx->request_body || (char *)ctx->request_body != empty_json) {
      ctx->bypass_response = 1;
    }
    ctx->request_result = (u_char *)error;

    // response parse the middleware json response
    jobj = json_tokener_parse(error);
    // Get the string value in "code" json key
    code = (char *)json_object_get_string(json_object_object_get(jobj, "code"));

    // return and finalize request early since we are not going to send
    // it to upstream server
    ngx_str_t content_type = ngx_string("application/json");
    request->headers_out.content_type = content_type;
    // convert "code" which is string to integer (status), example: 200
    request->headers_out.status = ngx_atoi((u_char *)code, strlen(code));
    request->keepalive = 0;

    rc = ngx_http_send_header(request);
    if (rc == NGX_ERROR || rc > NGX_OK || request->header_only) {
      ngx_http_finalize_request(request, rc);
      return NGX_DONE;
    }

    // allocate buffer in pool
    b = ngx_calloc_buf(request->pool);
    // set the error as unsigned char
    u_char *msg = (u_char *)error;
    b->pos = msg;
    b->last = msg + strlen((char *)msg);
    b->memory = 1;

    b->last_buf = 1;

    out.buf = b;
    out.next = NULL;

    rc = ngx_http_output_filter(request, &out);

    ngx_http_finalize_request(request, rc);
  }

  if (request == request->main) {
    request->headers_out.content_length_n = b->last - b->pos;

    if (request->headers_out.content_length) {
      request->headers_out.content_length->hash = 0;
      request->headers_out.content_length = NULL;
    }
  }

  out.buf = b;
  out.next = NULL;

  ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Sending next REQUEST body", NULL);
  return NGX_OK;
}
