#include <ngx_core.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "filter_context.h"
#include "filter_response_body.h"
#include "firetail_module.h"
#include "filter_firetail_send.h"

ngx_int_t FiretailResponseBodyFilter(ngx_http_request_t *request, ngx_chain_t *chain_head) {
  struct ValidateResponseBody_return validation_result;

  // You can set the logging level to debug here
  // request->connection->log->log_level = NGX_LOG_DEBUG;

  // Get our context so we can store the response body data
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL || ctx->done) {
    return kNextResponseBodyFilter(request, chain_head);
  }

  // Determine the length of the response body chain we've been given, and if
  // the chain contains the last link
  long new_response_body_parts_size = 0;

  for (ngx_chain_t *current_chain_link = chain_head; current_chain_link != NULL;
       current_chain_link = current_chain_link->next) {
    new_response_body_parts_size += current_chain_link->buf->last - current_chain_link->buf->pos;
  }

  // If we read in more bytes from this chain then we need to create a new char*
  // and place it in ctx containing the full body
  if (new_response_body_parts_size > 0) {
    // Take note of the response body size before and after adding the new chain
    // & update it in our ctx
    long old_response_body_size = ctx->response_body_size;
    long new_response_body_size = old_response_body_size + new_response_body_parts_size;
    ctx->response_body_size = new_response_body_size;

    // Create a new updated body
    u_char *updated_response_body = ngx_pcalloc(request->pool, new_response_body_size);

    // Copy the body read so far into ctx into our new updated_response_body
    u_char *updated_response_body_i = ngx_copy(updated_response_body, ctx->response_body, old_response_body_size);

    // Iterate over the chain again and copy all of the buffers over to our new
    // response body char*
    for (ngx_chain_t *current_chain_link = chain_head; current_chain_link != NULL;
         current_chain_link = current_chain_link->next) {
      long buffer_length = current_chain_link->buf->last - current_chain_link->buf->pos;
      updated_response_body_i = ngx_copy(updated_response_body_i, current_chain_link->buf->pos, buffer_length);
    }

    // Update the ctx with the new updated body
    ctx->response_body = updated_response_body;

    ngx_pfree(request->pool, updated_response_body);
  }

  FiretailConfig *main_config = ngx_http_get_module_main_conf(request, ngx_firetail_module);

  // If it does contain the last buffer, we can validate it with our go lib.
  // NOTE: I'm currently loading this dynamic module in every time we need to
  // call it. If I do it once at startup, it would just hang when I call the
  // response body validator _sometimes_. Couldn't figure out why. Creating the
  // middleware on the go side of things every time will be very inefficient.

  if (ctx->bypass_response == 0) {
    // Get the response header values
    json_object *response_headers_root = json_object_new_object();
    for (ngx_list_part_t *response_header_list_part = &request->headers_out.headers.part;
         response_header_list_part != NULL; response_header_list_part = response_header_list_part->next) {
      for (ngx_table_elt_t *response_header = response_header_list_part->elts;
           (ngx_uint_t)response_header <
           (ngx_uint_t)response_header_list_part->elts + response_header_list_part->nelts * sizeof(ngx_table_elt_t);
           response_header++) {
        json_object_object_add(response_headers_root, (char *)response_header->key.data,
                               json_object_new_string((char *)response_header->value.data));
      }
    }
    // Don't forget to add the Content-Type; NGINX doesn't keep it in `headers_out.headers` - it gets special treatment.
    json_object_object_add(response_headers_root, (char *)"Content-Type",
                           json_object_new_string((char *)request->headers_out.content_type.data));
    char *response_headers_json_string = (char *)json_object_to_json_string(response_headers_root);

    // Load the validator module & get the ValidateResponseBody function
    void *validator_module = dlopen("/etc/nginx/modules/firetail-validator.so", RTLD_LAZY);
    if (!validator_module) {
      return NGX_ERROR;
    }
    ValidateResponseBody response_body_validator =
        (ValidateResponseBody)dlsym(validator_module, "ValidateResponseBody");
    char *error;
    if ((error = dlerror()) != NULL) {
      ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Failed to load ValidateResponseBody: %s", error);
      exit(1);
    }
    ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Validating response body...");

    validation_result = response_body_validator(
        (char *)main_config->FiretailUrl.data, main_config->FiretailUrl.len, (char *)main_config->FiretailApiToken.data,
        main_config->FiretailApiToken.len, (char *)main_config->FiretailAllowUndefinedRoutes.data,
        (int)main_config->FiretailAllowUndefinedRoutes.len, (char *)ctx->request_body, (int)ctx->request_body_size,
        (char *)ctx->request_headers_json, (int)ctx->request_headers_json_size, ctx->response_body,
        ctx->response_body_size, response_headers_json_string, strlen(response_headers_json_string),
        request->unparsed_uri.data, request->unparsed_uri.len, ctx->status_code, request->method_name.data,
        request->method_name.len);
    ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Validation response result: %d", validation_result.r0);
    ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "Validating response body: %s", validation_result.r1);

    // if validation result is not successful
    if (validation_result.r0 > 0) {
      return ngx_http_firetail_send(request, ctx, NULL, validation_result.r1);
    }

    dlclose(validator_module);
  } else {
    validation_result.r1 = (char *)ctx->request_result;
  }

  if (ctx->bypass_response == 1)
    return ngx_http_firetail_send(request, ctx, ngx_http_filter_buffer(request, (u_char *)ctx->request_result), NULL);

  return ngx_http_firetail_send(request, ctx, ngx_http_filter_buffer(request, (u_char *)validation_result.r1), NULL);
}
