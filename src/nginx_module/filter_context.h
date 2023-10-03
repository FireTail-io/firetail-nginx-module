#ifndef FIRETAIL_FILTER_CONTEXT_INCLUDED
#define FIRETAIL_FILTER_CONTEXT_INCLUDED

#include <ngx_http.h>

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
  ngx_uint_t done;
  ngx_uint_t bypass_response;
  u_char *request_result;
} FiretailFilterContext;

// This utility function will allow us to get the filter ctx whenever we need
// it, and creates it if it doesn't already exist
FiretailFilterContext *GetFiretailFilterContext(ngx_http_request_t *request);

#endif
