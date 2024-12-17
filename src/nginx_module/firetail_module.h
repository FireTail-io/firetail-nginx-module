#ifndef FIRETAIL_MODULE_INCLUDED
#define FIRETAIL_MODULE_INCLUDED

#include <ngx_core.h>
#include <ngx_http.h>

// The header and body filters of the filter that was added just before ours.
// These make up part of a singly linked list of header and body filters.
extern ngx_http_output_header_filter_pt kNextHeaderFilter;
extern ngx_http_output_body_filter_pt kNextResponseBodyFilter;

// This struct defines the Firetail NGINX module's hooks
extern ngx_module_t ngx_firetail_module;

#endif
