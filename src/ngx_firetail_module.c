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

// Holds a HTTP header
typedef struct {
  ngx_str_t key;
  ngx_str_t value;
} HTTPHeader;

// This struct will hold all of the data we will send to Firetail about the
// request & response bodies & headers
typedef struct {
  ngx_uint_t status_code;
  u_char *server;
  long request_body_size;
  long response_body_size;
  u_char *request_body;
  u_char *response_body;
  long request_header_count;
  long response_header_count;
  HTTPHeader *request_headers;
  HTTPHeader *response_headers;
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

  // Determine the length of the request body chain we've been given
  long new_request_body_parts_size = 0;
  ngx_chain_t *current_chain_link = chain_head;
  for (;;) {
    new_request_body_parts_size +=
        current_chain_link->buf->last - current_chain_link->buf->pos;
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

  // If it doesn't contain the last buffer of the response body, pass everything
  // onto the next filter - we do not care.
  if (!chain_contains_last_link) {
    return kNextResponseBodyFilter(request, chain_head);
  }

  // Piece together a JSON object
  // {
  //   "version": "1.0.0-alpha",
  //   "dateCreated": 123456789,
  //   "executionTime": 123456789, TODO
  //   "request": {
  //     "ip": "8.8.8.8",
  //     "httpProtocol": "HTTP/2",
  //     "uri": "http://foo.bar/baz",
  //     "resource": "",
  //     "method": "POST",
  //     "headers": {},
  //     "body": ""
  //   },
  //   "response": {
  //     "statusCode": 201,
  //     "body": "",
  //     "headers": {}
  //   }
  // }
  json_object *log_root = json_object_new_object();

  json_object *version = json_object_new_string("1.0.0-alpha");
  json_object_object_add(log_root, "version", version);

  struct timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  json_object *date_created = json_object_new_int64(
      current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000);
  json_object_object_add(log_root, "dateCreated", date_created);

  json_object *request_object = json_object_new_object();
  json_object_object_add(log_root, "request", request_object);

  json_object *request_ip =
      json_object_new_string_len((char *)request->connection->addr_text.data,
                                 request->connection->addr_text.len);
  json_object_object_add(request_object, "ip", request_ip);

  json_object *request_protocol = json_object_new_string_len(
      (char *)request->http_protocol.data, request->http_protocol.len);
  json_object_object_add(request_object, "httpProtocol", request_protocol);

  char *full_uri = ngx_palloc(
      request->pool, strlen((char *)ctx->server) + request->unparsed_uri.len);
  ngx_memcpy(full_uri, ctx->server, strlen((char *)ctx->server));
  ngx_memcpy(full_uri + strlen((char *)ctx->server), request->unparsed_uri.data,
             request->unparsed_uri.len);
  json_object *request_uri = json_object_new_string(full_uri);
  json_object_object_add(request_object, "uri", request_uri);

  json_object *request_method = json_object_new_string_len(
      (char *)request->method_name.data, request->method_name.len);
  json_object_object_add(request_object, "method", request_method);

  json_object *request_body = json_object_new_string_len(
      (char *)ctx->request_body, (int)ctx->request_body_size);
  json_object_object_add(request_object, "body", request_body);

  json_object *request_headers = json_object_new_object();
  for (HTTPHeader *request_header = ctx->request_headers;
       (ngx_uint_t)request_header <
       (ngx_uint_t)ctx->request_headers +
           ctx->request_header_count * sizeof(HTTPHeader);
       request_header++) {
    json_object *header_value_array = json_object_new_array_ext(1);
    json_object_object_add(request_headers, (char *)request_header->key.data,
                           header_value_array);
    json_object *header_value = json_object_new_string_len(
        (char *)request_header->value.data, (int)request_header->value.len);
    json_object_array_add(header_value_array, header_value);
  }
  json_object_object_add(request_object, "headers", request_headers);

  json_object *response_object = json_object_new_object();
  json_object_object_add(log_root, "response", response_object);

  json_object *response_status_code = json_object_new_int(ctx->status_code);
  json_object_object_add(response_object, "statusCode", response_status_code);

  json_object *response_body = json_object_new_string_len(
      (char *)ctx->response_body, (int)ctx->response_body_size);
  json_object_object_add(response_object, "body", response_body);

  json_object *response_headers = json_object_new_object();
  for (HTTPHeader *response_header = ctx->response_headers;
       (ngx_uint_t)response_header <
       (ngx_uint_t)ctx->response_headers +
           ctx->response_header_count * sizeof(HTTPHeader);
       response_header++) {
    json_object *header_value_array = json_object_new_array_ext(1);
    json_object_object_add(response_headers, (char *)response_header->key.data,
                           header_value_array);
    json_object *header_value = json_object_new_string_len(
        (char *)response_header->value.data, (int)response_header->value.len);
    json_object_array_add(header_value_array, header_value);
  }
  json_object_object_add(response_object, "headers", response_headers);

  // Log it
  fprintf(stderr, "%s\n",
          json_object_to_json_string_ext(log_root, JSON_C_TO_STRING_PRETTY));

  // TODO: curl Firetail logging API

  // Pass the chain onto the next response body filter
  return kNextResponseBodyFilter(request, chain_head);
}

// Extracts the headers, status code & server then inserts them into the
// FiretailFilterContext
static ngx_int_t FiretailHeaderFilter(ngx_http_request_t *request) {
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);

  // Copy the status code and server out of the headers
  ctx->status_code = request->headers_out.status;
  ctx->server = request->headers_in.server.data;

  // Count the request headers
  for (ngx_list_part_t *request_header_list_part =
           &request->headers_in.headers.part;
       request_header_list_part != NULL;
       request_header_list_part = request_header_list_part->next) {
    ctx->request_header_count += request_header_list_part->nelts;
  }

  // Allocate memory for the request headers array
  ctx->request_headers =
      ngx_palloc(request->pool, ctx->request_header_count * sizeof(HTTPHeader));

  // Populate the request headers array
  HTTPHeader *recorded_request_header = ctx->request_headers;
  for (ngx_list_part_t *request_header_list_part =
           &request->headers_in.headers.part;
       request_header_list_part != NULL;
       request_header_list_part = request_header_list_part->next) {
    for (ngx_table_elt_t *request_header = request_header_list_part->elts;
         (ngx_uint_t)request_header <
         (ngx_uint_t)request_header_list_part->elts +
             request_header_list_part->nelts * sizeof(ngx_table_elt_t);
         request_header++) {
      recorded_request_header->key = request_header->key;
      recorded_request_header->value = request_header->value;
      recorded_request_header++;
    }
  }

  // Count the response headers
  for (ngx_list_part_t *response_header_list_part =
           &request->headers_out.headers.part;
       response_header_list_part != NULL;
       response_header_list_part = response_header_list_part->next) {
    ctx->response_header_count += response_header_list_part->nelts;
  }

  // Allocate memory for the response headers array
  ctx->response_headers = ngx_palloc(
      request->pool, ctx->response_header_count * sizeof(HTTPHeader));

  // Populate the response headers array
  HTTPHeader *recorded_response_header = ctx->response_headers;
  for (ngx_list_part_t *response_header_list_part =
           &request->headers_out.headers.part;
       response_header_list_part != NULL;
       response_header_list_part = response_header_list_part->next) {
    for (ngx_table_elt_t *response_header = response_header_list_part->elts;
         (ngx_uint_t)response_header <
         (ngx_uint_t)response_header_list_part->elts +
             response_header_list_part->nelts * sizeof(ngx_table_elt_t);
         response_header++) {
      recorded_response_header->key = response_header->key;
      recorded_response_header->value = response_header->value;
      recorded_response_header++;
    }
  }

  return kNextHeaderFilter(request);
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
static ngx_command_t kFiretailCommands[] = {
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
static ngx_http_module_t kFiretailModuleContext = {
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