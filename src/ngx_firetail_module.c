#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <curl/curl.h>
#include <json-c/json.h>

// The header and body filters of the filter that was added just before ours.
// These make up part of a singly linked list of header and body filters.
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

// This body filter just appends `firetail_message` to the response body.
u_char *firetail_message = (u_char *) "<!-- This response body has been recorded by the Firetail NGINX Module :) -->";
static ngx_int_t ngx_http_firetail_body_filter(ngx_http_request_t *request,
                                               ngx_chain_t *chain_head) {
  // Find if the chain of buffers we've been given contains the last buffer of
  // the response body
  int chain_contains_last_buffer = 0;
  ngx_chain_t *current_chain_link = chain_head;
  for (;;) {
    if (current_chain_link->buf->last_buf) {
      chain_contains_last_buffer = 1;
    }
    if (current_chain_link->next == NULL) {
      break;
    }
    current_chain_link = current_chain_link->next;
  }

  // If it doesn't contain the last buffer of the response body, pass everything
  // onto the next filter - we do not care.
  if (!chain_contains_last_buffer) {
    return ngx_http_next_body_filter(request, chain_head);
  }

  // Create a new buffer for the data we want to append to the end of the
  // response body
  ngx_buf_t *my_buffer = ngx_calloc_buf(request->pool);
  if (my_buffer == NULL) {
    return NGX_ERROR;
  }
  my_buffer->pos = firetail_message;
  my_buffer->last = my_buffer->pos + strlen((char *)firetail_message);
  my_buffer->memory = 1;    // The buffer is readonly
  my_buffer->last_buf = 1;  // There will be no more buffers in the chain

  // Create a new chain link to contain our extra data that we can then add to
  // the chain
  ngx_chain_t *my_extra_chain_link = ngx_alloc_chain_link(request->pool);
  if (my_extra_chain_link == NULL) {
    return NGX_ERROR;
  }
  my_extra_chain_link->buf = my_buffer;
  my_extra_chain_link->next = NULL;

  // Add it to the end of the current chain link & mark the current chain link's
  // end as being no longer the last buf
  current_chain_link->next = my_extra_chain_link;
  current_chain_link->buf->last_buf = 0;

  return ngx_http_next_body_filter(request, chain_head);
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