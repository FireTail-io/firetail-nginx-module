#include <ngx_core.h>
#include "firetail_context.h"
#include "firetail_directives.h"

#define SIZE 65536

ngx_module_t ngx_firetail_module = {NGX_MODULE_V1,
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
