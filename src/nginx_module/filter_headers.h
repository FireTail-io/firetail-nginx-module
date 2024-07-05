#ifndef FIRETAIL_FILTER_HEADERS_INCLUDED
#define FIRETAIL_FILTER_HEADERS_INCLUDED

#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t FiretailHeaderFilter(ngx_http_request_t *request);

#endif