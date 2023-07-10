#include <ngx_http.h>

// This struct defines the context for the Firetail NGINX module
static ngx_http_module_t kFiretailModuleContext;

// An array of the directives provided by the Firetail module
static ngx_command_t kFiretailCommands[];

// This struct defines the Firetail NGINX module's hooks
ngx_module_t ngx_firetail_module = {
    NGX_MODULE_V1,
    &kFiretailModuleContext, /* module context */
    kFiretailCommands,       /* module directives */
    NGX_HTTP_MODULE,         /* module type */
    NULL,                    /* init master */
    NULL,                    /* init module */
    NULL,                    /* init process */
    NULL,                    /* init thread */
    NULL,                    /* exit thread */
    NULL,                    /* exit process */
    NULL,                    /* exit master */
    NGX_MODULE_V1_PADDING};