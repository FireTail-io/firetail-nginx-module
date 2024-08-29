#include <ngx_core.h>
#include "filter_context.h"
#include "filter_headers.h"
#include "firetail_module.h"

ngx_int_t FiretailHeaderFilter(ngx_http_request_t *request) {
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  // Copy the status code and server out of the headers
  ctx->status_code = request->headers_out.status;
  ctx->server = request->headers_in.server.data;

  // Count the request headers
  for (ngx_list_part_t *request_header_list_part = &request->headers_in.headers.part; request_header_list_part != NULL;
       request_header_list_part = request_header_list_part->next) {
    ctx->request_header_count += request_header_list_part->nelts;
  }

  // Allocate memory for the request headers array
  ctx->request_headers = ngx_palloc(request->pool, ctx->request_header_count * sizeof(HTTPHeader));

  // Populate the request headers array
  HTTPHeader *recorded_request_header = ctx->request_headers;
  for (ngx_list_part_t *request_header_list_part = &request->headers_in.headers.part; request_header_list_part != NULL;
       request_header_list_part = request_header_list_part->next) {
    for (ngx_table_elt_t *request_header = request_header_list_part->elts;
         (ngx_uint_t)request_header <
         (ngx_uint_t)request_header_list_part->elts + request_header_list_part->nelts * sizeof(ngx_table_elt_t);
         request_header++) {
      recorded_request_header->key = request_header->key;
      recorded_request_header->value = request_header->value;
      recorded_request_header++;
    }
  }

  request->main_filter_need_in_memory = 1;
  request->allow_ranges = 0;

  return NGX_OK;
}
