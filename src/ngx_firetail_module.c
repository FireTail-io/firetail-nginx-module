#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "ngx_firetail_module.h"

// The header and body filters of the filter that was added just before ours.
// These make up part of a singly linked list of header and body filters.
static ngx_http_output_header_filter_pt kNextHeaderFilter;
static ngx_http_output_body_filter_pt kNextResponseBodyFilter;
static ngx_http_request_body_filter_pt kNextRequestBodyFilter;

// This struct will hold all of the data we will send to Firetail about the
// request & response bodies & headers
typedef struct {
  long request_body_size;
  long response_body_size;
  u_char *request_body;
  u_char *response_body;
} FiretailFilterContext;

// This utility function will allow us to get the filter ctx whenever we need
// it, and creates it if it doesn't already exist
static FiretailFilterContext *GetFiretailFilterContext(
    ngx_http_request_t *request) {
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

static ngx_int_t FiretailRequestBodyFilter(ngx_http_request_t *request,
                                           ngx_chain_t *chain_head) {
  // Get our context so we can store the request body data
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  // Determine the length of the request body chain we've been given, and if the
  // chain contains the last link
  int chain_contains_last_link = 0;
  long new_request_body_parts_size = 0;
  ngx_chain_t *current_chain_link = chain_head;
  for (;;) {
    new_request_body_parts_size +=
        current_chain_link->buf->last - current_chain_link->buf->pos;
    if (current_chain_link->buf->last_buf) {
      chain_contains_last_link = 1;
    }
    if (current_chain_link->next == NULL) {
      break;
    }
    current_chain_link = current_chain_link->next;
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
    current_chain_link = chain_head;
    for (;;) {
      long buffer_length =
          current_chain_link->buf->last - current_chain_link->buf->pos;
      updated_request_body_i = ngx_copy(
          updated_request_body_i, current_chain_link->buf->pos, buffer_length);
      if (current_chain_link->next == NULL) {
        break;
      }
      current_chain_link = current_chain_link->next;
    }

    // Update the ctx with the new updated body
    ctx->request_body = updated_request_body;
  }

  fprintf(
      stderr,
      "Got a request body chain of size %ld. Total request body size: %ld\n",
      new_request_body_parts_size, ctx->request_body_size);

  if (chain_contains_last_link) {
    fprintf(stderr, "Reached the end of the request body chain!\n");
    fprintf(stderr, "Request Body:\n%.*s\n", (int)ctx->request_body_size,
            ctx->request_body);
    fprintf(stderr, "Request body size: %ld\n", ctx->request_body_size);
  }

  return kNextRequestBodyFilter(request, chain_head);
}

static ngx_int_t FiretailResponseBodyFilter(ngx_http_request_t *request,
                                            ngx_chain_t *chain_head) {
  // Get our context so we can store the response body data
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  // Determine the length of the response body chain we've been given, and if
  // the chain contains the last link
  int chain_contains_last_link = 0;
  long new_response_body_parts_size = 0;
  ngx_chain_t *current_chain_link = chain_head;
  for (;;) {
    new_response_body_parts_size +=
        current_chain_link->buf->last - current_chain_link->buf->pos;
    if (current_chain_link->buf->last_buf) {
      chain_contains_last_link = 1;
    }
    if (current_chain_link->next == NULL) {
      break;
    }
    current_chain_link = current_chain_link->next;
  }

  // If we read in more bytes from this chain then we need to create a new char*
  // and place it in ctx containing the full body
  if (new_response_body_parts_size > 0) {
    // Take note of the response body size before and after adding the new chain
    // & update it in our ctx
    long old_response_body_size = ctx->response_body_size;
    long new_response_body_size =
        old_response_body_size + new_response_body_parts_size;
    ctx->response_body_size = new_response_body_size;

    // Create a new updated body
    u_char *updated_response_body =
        ngx_pcalloc(request->pool, new_response_body_size);

    // Copy the body read so far into ctx into our new updated_response_body
    u_char *updated_response_body_i = ngx_copy(
        updated_response_body, ctx->response_body, old_response_body_size);

    // Iterate over the chain again and copy all of the buffers over to our new
    // response body char*
    current_chain_link = chain_head;
    for (;;) {
      long buffer_length =
          current_chain_link->buf->last - current_chain_link->buf->pos;
      updated_response_body_i = ngx_copy(
          updated_response_body_i, current_chain_link->buf->pos, buffer_length);
      if (current_chain_link->next == NULL) {
        break;
      }
      current_chain_link = current_chain_link->next;
    }

    // Update the ctx with the new updated body
    ctx->response_body = updated_response_body;
  }

  fprintf(
      stderr,
      "Got a response body chain of size %ld. Total response body size: %ld\n",
      new_response_body_parts_size, ctx->response_body_size);

  // If it doesn't contain the last buffer of the response body, pass everything
  // onto the next filter - we do not care.
  if (!chain_contains_last_link) {
    return kNextResponseBodyFilter(request, chain_head);
  }

  fprintf(stderr, "Reached the end of the response body chain!\n");
  fprintf(stderr, "Response Body:\n%.*s\n", (int)ctx->response_body_size,
          ctx->response_body);
  fprintf(stderr, "Response body size: %ld\n", ctx->response_body_size);

  return kNextResponseBodyFilter(request, chain_head);
}

// TODO: Extract the headers and insert them into the
// FiretailFilterContext
static ngx_int_t FiretailHeaderFilter(ngx_http_request_t *r) {
  return kNextHeaderFilter(r);
}

// This function is called whenever the `enable_firetail` directive is found in
// the NGINX config
static char *EnableFiretailDirectiveInit(ngx_conf_t *cf, ngx_command_t *cmd,
                                         void *conf) {
  // TODO: validate the args given to the directive
  // TODO: change the key arg to be the name of an env var and get it from the
  // environment
  return NGX_OK;
}

// An array of the directives provided by the Firetail module
static ngx_command_t FiretailCommands[] = {
    {// Name of the directive
     ngx_string("enable_firetail"),
     // Valid in the main config, server & location configs; and takes two args
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_TAKE2,
     // A callback function to be called when the directive is found in the
     // configuration
     EnableFiretailDirectiveInit, 0, 0, NULL},
    ngx_null_command};

// Updates the next header and body filters to be our own; we're appending to a
// singly linked list here and making our own local references to the next
// filter down the list from us
static ngx_int_t FiretailInit(ngx_conf_t *cf) {
  kNextRequestBodyFilter = ngx_http_top_request_body_filter;
  ngx_http_top_request_body_filter = FiretailRequestBodyFilter;

  kNextResponseBodyFilter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = FiretailResponseBodyFilter;

  kNextHeaderFilter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = FiretailHeaderFilter;

  return NGX_OK;
}

// This struct defines the context for the Firetail NGINX module
static ngx_http_module_t FiretailModuleContext = {
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