#include <ngx_core.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "filter_context.h"
#include "filter_response_body.h"
#include "firetail_module.h"

ngx_int_t FiretailResponseBodyFilter(ngx_http_request_t *request,
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