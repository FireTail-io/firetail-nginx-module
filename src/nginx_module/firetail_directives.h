#include <ngx_core.h>
#include "firetail_config.h"

char *FiretailApiTokenDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                        void *http_main_config);
char *FiretailUrlDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                   void *http_main_config);
char *FiretailAllowUndefinedRoutesDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                                    void *http_main_config);
char *FiretailEnableDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                      void *http_main_config);
char *FiretailDisableDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                       void *http_main_config);

ngx_command_t kFiretailCommands[6] = {
    {// Name of the directive
     ngx_string("firetail_api_token"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailApiTokenDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET, offsetof(FiretailConfig, FiretailApiToken), NULL},
    {// Name of the directive
     ngx_string("firetail_url"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailUrlDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET, offsetof(FiretailConfig, FiretailUrl), NULL},
    {// Name of the directive
     ngx_string("firetail_allow_undefined_routes"),
     // Valid in the main config and takes one arg
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailAllowUndefinedRoutesDirectiveCallback, NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(FiretailConfig, FiretailAllowUndefinedRoutes), NULL},
    {// Name of the directive
     ngx_string("firetail_enable"),
     // Valid in location configs and takes no args
     NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailEnableDirectiveCallback, NGX_HTTP_LOC_CONF_OFFSET, offsetof(FiretailConfig, FiretailEnabled), NULL},
    {// Name of the directive
     ngx_string("firetail_disable"),
     // Valid in location configs and takes no args
     NGX_HTTP_MAIN_CONF | NGX_CONF_NOARGS,
     // A callback function to be called when the directive is found in the
     // configuration
     FiretailDisableDirectiveCallback, NGX_HTTP_LOC_CONF_OFFSET, offsetof(FiretailConfig, FiretailEnabled), NULL},
    ngx_null_command};
