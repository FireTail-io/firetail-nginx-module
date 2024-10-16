#ifndef FIRETAIL_MODULE_INCLUDED
#define FIRETAIL_MODULE_INCLUDED

#include <ngx_http.h>

typedef int (*CreateMiddlewareFunc)(void*, int);

struct ValidateResponseBody_return {
  int r0;
  char* r1;
};
typedef struct ValidateResponseBody_return (*ValidateResponseBody)(char*, int, char*, int, char*, int, char*, int,
                                                                   char*, int, void*, int, char*, int, void*, int, int,
                                                                   void*, int);

struct ValidateRequestBody_return {
  int r0;
  char* r1;
};
typedef struct ValidateRequestBody_return (*ValidateRequestBody)(void*, int, void*, int, void*, int, void*, int, void*,
                                                                 int);

// This config struct will hold our API key
typedef struct {
  ngx_str_t FiretailApiToken;  // TODO: this should probably be a *ngx_str_t
  ngx_str_t FiretailUrl;
  ngx_str_t FiretailAllowUndefinedRoutes;
} FiretailMainConfig;

// The header and body filters of the filter that was added just before ours.
// These make up part of a singly linked list of header and body filters.
extern ngx_http_output_header_filter_pt kNextHeaderFilter;
extern ngx_http_output_body_filter_pt kNextResponseBodyFilter;

// This struct defines the Firetail NGINX module's hooks
extern ngx_module_t ngx_firetail_module;

#endif
