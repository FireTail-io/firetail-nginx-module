#include <curl/curl.h>
#include <json-c/json.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

// The header and body filters of the filter that was added just before ours.
// These make up part of a singly linked list of header and body filters.
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

// This body filter prepends `firetail_message` to the response body.
u_char *firetail_message = (u_char *) "<!-- This response body has been recorded by the Firetail NGINX Module :) -->";
static ngx_int_t ngx_http_firetail_body_filter(ngx_http_request_t *r,
                                               ngx_chain_t *in) {
  ngx_buf_t *buf;
  ngx_chain_t *link;

  buf = ngx_calloc_buf(r->pool);

  buf->pos = firetail_message;
  buf->last = buf->pos + strlen((char *)firetail_message);
  buf->start = buf->pos;
  buf->end = buf->last;
  buf->last_buf = 0;
  buf->memory = 1;

  link = ngx_alloc_chain_link(r->pool);

  link->buf = buf;
  link->next = in;

  return ngx_http_next_body_filter(r, link);
}

// This header filter just updates the content length header after the appending
// of the `firetail_message`
static ngx_int_t ngx_http_firetail_header_filter(ngx_http_request_t *r) {
  r->headers_out.content_length_n += strlen((char *)firetail_message);
  return ngx_http_next_header_filter(r);
}

// This function is called whenever the `enable_firetail` directive is found in
// the NGINX config
static char *ngx_http_firetail(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  // TODO: validate the args given to the directive
  // TODO: change the key arg to be the name of an env var and get it from the
  // environment
  return NGX_OK;
}

// An array of the directives provided by the Firetail module
static ngx_command_t ngx_http_firetail_commands[] = {
    {// Name of the directive
     ngx_string("enable_firetail"),
     // Valid in the main config, server & location configs; and takes two args
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_TAKE2,
     // A callback function to be called when the directive is found in the
     // configuration
     ngx_http_firetail, 0, 0, NULL},
    ngx_null_command};

// Updates the next header and body filters to be our own; we're appending to a
// singly linked list here and making our own local references to the next
// filter down the list from us
static ngx_int_t ngx_http_firetail_init(ngx_conf_t *cf) {
  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_firetail_body_filter;

  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_firetail_header_filter;

  return NGX_OK;
}

// This struct defines the context for the Firetail NGINX module
static ngx_http_module_t ngx_firetail_module_ctx = {
    NULL,  // preconfiguration
    // Filters are added in the postconfiguration step
    ngx_http_firetail_init,  // postconfiguration
    NULL,                    // create main configuration
    NULL,                    // init main configuration
    NULL,                    // create server configuration
    NULL,                    // merge server configuration
    NULL,                    // create location configuration
    NULL                     // merge location configuration
};

// This struct defines the Firetail NGINX module's hooks
ngx_module_t ngx_firetail_module = {
    NGX_MODULE_V1,
    &ngx_firetail_module_ctx,   /* module context */
    ngx_http_firetail_commands, /* module directives */
    NGX_HTTP_MODULE,            /* module type */
    NULL,                       /* init master */
    NULL,                       /* init module */
    NULL,                       /* init process */
    NULL,                       /* init thread */
    NULL,                       /* exit thread */
    NULL,                       /* exit process */
    NULL,                       /* exit master */
    NGX_MODULE_V1_PADDING};