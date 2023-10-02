#include <ngx_core.h>
#include <ngx_http.h>
#include "filter_context.h"
#include "firetail_module.h"
#include "filter_firetail_send.h"
#include <json-c/json.h>

ngx_int_t ngx_http_firetail_send(ngx_http_request_t *request, FiretailFilterContext *ctx,
		                 ngx_buf_t *b, char *error) {
  ngx_int_t rc;
  ngx_chain_t out;
  ngx_pool_cleanup_t *cln;

  struct json_object *jobj;
  char *code;

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Start of firetail send", NULL);

  ctx->done = 1;

  if (b == NULL) {
    // if there is an spec validation error by sending out
    // the error response gotten from the middleware
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0, "Buffer is null",
                  NULL);

    // response parse the middleware json response
    jobj = json_tokener_parse(error);
    // Get the string value in "code" json key
    code = (char *)json_object_get_string(json_object_object_get(jobj, "code"));

    // Set the middleware status code after converting string status code to
    // integer
    request->headers_out.status = ngx_atoi((u_char *)code, strlen(code));

    // request->headers_out.status = NGX_HTTP_BAD_REQUEST;
    ngx_str_t content_type = ngx_string("application/json");
    request->headers_out.content_type = content_type;

    // allocate buffer in pool
    b = ngx_calloc_buf(request->pool);
    // set the error as unsigned char
    u_char *msg = (u_char *)error;
    b->pos = msg;
    b->last = msg + strlen((char *)msg);
    b->memory = 1;

    b->last_buf = 1;
  }

  cln = ngx_pool_cleanup_add(request->pool, 0);
  if (cln == NULL) {
      ngx_free(b->pos);
      return ngx_http_filter_finalize_request(request, &ngx_firetail_module,
                                             NGX_HTTP_INTERNAL_SERVER_ERROR);
  } 

  if (request == request->main) {
    request->headers_out.content_length_n = b->last - b->pos;

    if (request->headers_out.content_length) {
      request->headers_out.content_length->hash = 0;
      request->headers_out.content_length = NULL;
    }
  }

  request->keepalive = 0;

  rc = kNextHeaderFilter(request);

  if (rc == NGX_ERROR || rc > NGX_OK || request->header_only) {
    ngx_free(b->pos);
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "SENDING HEADERS...", NULL);
    return rc;
  }

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Sending next RESPONSE body", NULL);

  cln->handler = ngx_http_firetail_cleanup;
  cln->data = b->pos;

  out.buf = b;
  out.next = NULL;

  return kNextResponseBodyFilter(request, &out);
}

ngx_int_t ngx_http_firetail_request(ngx_http_request_t *request, ngx_buf_t *b,
                                    ngx_chain_t *chain_head, char *error) {
  ngx_chain_t out;

  if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Buffer for REQUEST is null", NULL);

    ngx_str_t content_type = ngx_string("application/json");
    request->headers_out.content_type = content_type;
    request->headers_out.status = NGX_HTTP_BAD_REQUEST;

    b = ngx_calloc_buf(request->pool);
    u_char *msg = (u_char *)error;
    b->pos = msg;
    b->last = msg + strlen((char *)msg);
    b->memory = 1;

    b->last_buf = 1;
  }

  out.buf = b;
  out.next = NULL;

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Sending next REQUEST body", NULL);
  return kNextRequestBodyFilter(request, &out);
}

ngx_buf_t *ngx_http_filter_buffer(ngx_http_request_t *request,
                                  u_char *response) {
  ngx_buf_t *b;

  b = ngx_calloc_buf(request->pool);
  if (b == NULL) {
    return NULL;
  }

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Buffer is successful", NULL);

  b->pos = response;
  b->last = response + strlen((char *)response);
  b->memory = 1;

  b->last_buf = 1;

  return b;
}

void ngx_http_firetail_cleanup(void *data)
{
    ngx_free(data);
}
