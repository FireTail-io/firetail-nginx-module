#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t FiretailInit(ngx_conf_t *cf);
void *CreateFiretailConfig(ngx_conf_t *configuration_object);
char *InitFiretailMainConfig(ngx_conf_t *configuration_object, void *http_main_config);
char *MergeFiretailLocationConfig(ngx_conf_t *cf, void *parent, void *child);

ngx_http_module_t kFiretailModuleContext = {
    NULL,                        // preconfiguration
    FiretailInit,                // postconfiguration
    CreateFiretailConfig,        // create main configuration
    InitFiretailMainConfig,      // init main configuration
    NULL,                        // create server configuration
    NULL,                        // merge server configuration
    CreateFiretailConfig,        // create location configuration
    MergeFiretailLocationConfig  // merge location configuration
};
