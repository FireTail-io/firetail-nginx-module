#include <ngx_http.h>

char *FiretailApiTokenDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                        void *http_main_config) {
  // TODO: validate the args given to the directive

  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_api_key_field = (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_api_key_field = value[1];

  return NGX_CONF_OK;
}

char *FiretailUrlDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                   void *http_main_config) {
  // TODO: validate the args given to the directive

  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_url_field = (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_url_field = value[1];

  return NGX_CONF_OK;
}

char *FiretailAllowUndefinedRoutesDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                                    void *http_main_config) {
  // Find the firetail_api_key_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_str_t *firetail_allow_undefined_routes_field = (ngx_str_t *)(firetail_config + command_definition->offset);

  // Get the string value from the configuraion object
  ngx_str_t *value = configuration_object->args->elts;
  *firetail_allow_undefined_routes_field = value[1];

  return NGX_CONF_OK;
}

char *FiretailEnableDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                      void *http_main_config) {
  // Find the firetail_enable_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_int_t *firetail_enabled_field = (ngx_int_t *)(firetail_config + command_definition->offset);
  *firetail_enabled_field = 1;

  return NGX_CONF_OK;
}

char *FiretailDisableDirectiveCallback(ngx_conf_t *configuration_object, ngx_command_t *command_definition,
                                       void *http_main_config) {
  // Find the firetail_enable_field given the config pointer & offset in cmd
  char *firetail_config = http_main_config;
  ngx_int_t *firetail_enabled_field = (ngx_int_t *)(firetail_config + command_definition->offset);
  *firetail_enabled_field = 0;

  return NGX_CONF_OK;
}
