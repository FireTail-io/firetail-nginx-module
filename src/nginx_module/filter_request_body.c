#include <ngx_core.h>
#include "filter_context.h"
#include "filter_request_body.h"
#include "firetail_module.h"
#include "filter_firetail_send.h"

ngx_int_t FiretailRequestBodyFilter(ngx_http_request_t *request,
                                    ngx_chain_t *chain_head) {
  // Get our context so we can store the request body data
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  // Determine the length of the request body chain we've been given
  long new_request_body_parts_size = 0;
  for (ngx_chain_t *current_chain_link = chain_head; current_chain_link != NULL;
       current_chain_link = current_chain_link->next) {
    new_request_body_parts_size +=
        current_chain_link->buf->last - current_chain_link->buf->pos;
  }

  // If we read in more bytes from this chain then we need to create a new char*
  // and place it in ctx containing the full body
  if (new_request_body_parts_size > 0) {
    // Take note of the request body size before and after adding the new chain
    // & update it in our ctx
    long old_request_body_size = ctx->request_body_size;
    long new_request_body_size =
        old_request_body_size + new_request_body_parts_size;
    ctx->request_body_size = new_request_body_size;

    // Create a new updated body
    u_char *updated_request_body =
        ngx_pcalloc(request->pool, new_request_body_size);

    // Copy the body read so far into ctx into our new updated_request_body
    u_char *updated_request_body_i = ngx_copy(
        updated_request_body, ctx->request_body, old_request_body_size);

    // Iterate over the chain again and copy all of the buffers over to our new
    // request body char*
    for (ngx_chain_t *current_chain_link = chain_head;
         current_chain_link != NULL;
         current_chain_link = current_chain_link->next) {
      long buffer_length =
          current_chain_link->buf->last - current_chain_link->buf->pos;
      updated_request_body_i = ngx_copy(
          updated_request_body_i, current_chain_link->buf->pos, buffer_length);
    }

    // Update the ctx with the new updated body
    ctx->request_body = updated_request_body;

    // run the validation
    FiretailMainConfig *main_config =
        ngx_http_get_module_main_conf(request, ngx_firetail_module);

    void *validator_module =
        dlopen("/etc/nginx/modules/firetail-validator.so", RTLD_LAZY);
    if (!validator_module) {
      return NGX_ERROR;
    }

    ValidateRequestBody request_body_validator =
        (ValidateRequestBody)dlsym(validator_module, "ValidateRequestBody");
    char *error;
    if ((error = dlerror()) != NULL) {
      ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                    "Failed to load ValidateRequestBody: %s", error);
      exit(1);
    }
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Validating request body...");

    char *schema = ngx_palloc(request->pool, main_config->FiretailAppSpec.len);
    ngx_memcpy(schema, main_config->FiretailAppSpec.data,
               main_config->FiretailAppSpec.len);

    struct ValidateRequestBody_return validation_result =
        request_body_validator(
            schema, strlen(schema), ctx->request_body, ctx->request_body_size,
            request->unparsed_uri.data, request->unparsed_uri.len);
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Validation request result: %d", validation_result.r0);
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Validating request body: %s", validation_result.r1);

    // if validation is unsuccessful, return bad request
    if (validation_result.r0 > 0) {
      return ngx_http_firetail_request(request, NULL, chain_head,
                                       validation_result.r1);
    }

    // else continue request
    return ngx_http_firetail_request(
        request, ngx_http_filter_buffer(request, validation_result.r1),
        chain_head, NULL);

    ngx_pfree(request->pool, schema);

    dlclose(validator_module);
  }

  return NGX_OK;
}
