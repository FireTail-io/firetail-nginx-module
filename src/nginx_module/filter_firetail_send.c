#include <ngx_core.h>
#include <ngx_http.h>
#include "filter_context.h"
#include "firetail_module.h"

ngx_int_t ngx_http_firetail_send(ngx_http_request_t *request, ngx_buf_t *b,
                                 char *error) {
  ngx_int_t rc;
  ngx_chain_t out;

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Start of firetail send", NULL);

  if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0, "Buffer is null",
                  NULL);

    // send out error
    ngx_str_t content_type = ngx_string("application_json");
    request->headers_out.content_type = content_type;
    request->headers_out.status = NGX_HTTP_BAD_REQUEST;

    b = ngx_calloc_buf(request->pool);
    u_char *t = (u_char *)error;
    b->pos = t;
    b->last = t + strlen((char *)t);
    b->memory = 1;

    b->last_buf = 1;
  }

  if (request == request->main) {
    request->headers_out.content_length_n = b->last - b->pos;

    if (request->headers_out.content_length) {
      request->headers_out.content_length->hash = 0;
      request->headers_out.content_length = NULL;
    }
  }

  request->keepalive = 0;

  out.buf = b;
  out.next = NULL;

  rc = kNextHeaderFilter(request);

  if (rc == NGX_ERROR || rc > NGX_OK || request->header_only) {
    ngx_free(b->pos);
    return rc;
  }

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Sending next RESPONSE body", NULL);

  return kNextResponseBodyFilter(request, &out);
}

ngx_int_t ngx_http_firetail_request(ngx_http_request_t *request, ngx_buf_t *b,
                                    ngx_chain_t *chain_head, char *error) {
  if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Buffer for REQUEST is null", NULL);

    ngx_str_t content_type = ngx_string("application_json");
    request->headers_out.content_type = content_type;
    request->headers_out.status = NGX_HTTP_BAD_REQUEST;

    b = ngx_calloc_buf(request->pool);
    u_char *t = (u_char *)error;
    b->pos = t;
    b->last = t + strlen((char *)t);
    b->memory = 1;

    b->last_buf = 1;
  }

  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Sending next REQUEST body", NULL);
  return kNextRequestBodyFilter(request, chain_head);
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
