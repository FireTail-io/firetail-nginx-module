#ifndef FIRETAIL_FILTER_RESPONSE_BODY_INCLUDED
#define FIRETAIL_FILTER_RESPONSE_BODY_INCLUDED

#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t FiretailResponseBodyFilter(ngx_http_request_t *request,
                                     ngx_chain_t *chain_head);
#endif
