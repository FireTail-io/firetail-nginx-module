#include <ngx_core.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "filter_context.h"
#include "filter_response_body.h"
#include "firetail_module.h"

size_t LibcurlNoopWriteFunction(void *buffer, size_t size, size_t nmemb,
                                void *userp) {
  return size * nmemb;
}

#define MAX_WAIT_MSECS 30 * 100 /* max wait 30 secs */

ngx_int_t FiretailResponseBodyFilter(ngx_http_request_t *request,
                                     ngx_chain_t *chain_head) {
  // Set the logging level to debug
  // TODO: remove
  request->connection->log->log_level = NGX_LOG_DEBUG;

  // Get our context so we can store the response body data
  FiretailFilterContext *ctx = GetFiretailFilterContext(request);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  // Determine the length of the response body chain we've been given, and if
  // the chain contains the last link
  int chain_contains_last_link = 0;
  long new_response_body_parts_size = 0;

  for (ngx_chain_t *current_chain_link = chain_head; current_chain_link != NULL;
       current_chain_link = current_chain_link->next) {
    new_response_body_parts_size +=
        current_chain_link->buf->last - current_chain_link->buf->pos;
    if (current_chain_link->buf->last_buf) {
      chain_contains_last_link = 1;
    }
  }

  // If we read in more bytes from this chain then we need to create a new char*
  // and place it in ctx containing the full body
  if (new_response_body_parts_size > 0) {
    // Take note of the response body size before and after adding the new chain
    // & update it in our ctx
    long old_response_body_size = ctx->response_body_size;
    long new_response_body_size =
        old_response_body_size + new_response_body_parts_size;
    ctx->response_body_size = new_response_body_size;

    // Create a new updated body
    u_char *updated_response_body =
        ngx_pcalloc(request->pool, new_response_body_size);

    // Copy the body read so far into ctx into our new updated_response_body
    u_char *updated_response_body_i = ngx_copy(
        updated_response_body, ctx->response_body, old_response_body_size);

    // Iterate over the chain again and copy all of the buffers over to our new
    // response body char*
    for (ngx_chain_t *current_chain_link = chain_head;
         current_chain_link != NULL;
         current_chain_link = current_chain_link->next) {
      long buffer_length =
          current_chain_link->buf->last - current_chain_link->buf->pos;
      updated_response_body_i = ngx_copy(
          updated_response_body_i, current_chain_link->buf->pos, buffer_length);
    }

    // Update the ctx with the new updated body
    ctx->response_body = updated_response_body;

    ngx_pfree(request->pool, updated_response_body);
  }

  // If it doesn't contain the last buffer of the response body, pass everything
  // onto the next filter - we do not care.
  if (!chain_contains_last_link) {
    return kNextResponseBodyFilter(request, chain_head);
  }

  FiretailMainConfig *main_config =
      ngx_http_get_module_main_conf(request, ngx_firetail_module);

  void *validator_module =
      dlopen("/etc/nginx/modules/firetail-validator.so", RTLD_LAZY);
  if (!validator_module) {
    return NGX_ERROR;
  }

  ValidateResponseBody response_body_validator =
      (ValidateResponseBody)dlsym(validator_module, "ValidateResponseBody");
  char *error;
  if ((error = dlerror()) != NULL) {
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Failed to load ValidateResponseBody: %s", error);
    exit(1);
  }
  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Validating response body...");

  char *schema = ngx_palloc(request->pool, main_config->FiretailAppSpec.len);
  ngx_memcpy(schema, main_config->FiretailAppSpec.data,
             main_config->FiretailAppSpec.len);

  // ngx_log_debug(NGX_LOG_DEBUG, request->connection->log, 0, "schema: %s",
  //               schema);
  struct ValidateResponseBody_return validation_result =
      response_body_validator(schema, strlen(schema), ctx->response_body,
                              ctx->response_body_size,
                              request->unparsed_uri.data,
                              request->unparsed_uri.len, ctx->status_code);
  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Validation response result: %d", validation_result.r0);
  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Validating response body: %s", validation_result.r1);

  ngx_pfree(request->pool, schema);

  dlclose(validator_module);

  // If it does contain the last buffer, we can validate it with our go lib.
  // NOTE: I'm currently loading this dynamic module in every time we need to
  // call it. If I do it once at startup, it would just hang when I call the
  // response body validator _sometimes_. Couldn't figure out why. Creating the
  // middleware on the go side of things every time will be very inefficient.
  /*
  void *validator_module =
      dlopen("/etc/nginx/modules/firetail-validator.so", RTLD_LAZY);
  if (validator_module == NULL) {
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Failed to open validator.so: %s", dlerror());
    exit(1);
  }

  CreateMiddlewareFunc create_middleware =
      (CreateMiddlewareFunc)dlsym(validator_module, "CreateMiddleware");
  char *error;
  if ((error = dlerror()) != NULL) {
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Failed to load CreateMiddleware: %s", error);
    exit(1);
  }

  int create_middleware_result = create_middleware(
      "/usr/local/nginx/appspec.yml", strlen("/usr/local/nginx/appspec.yml"));
  ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                "Create middleware result: %d", create_middleware_result);
  if (create_middleware_result == 1) {
    ValidateResponseBody response_body_validator =
        (ValidateResponseBody)dlsym(validator_module, "ValidateResponseBody");
    if ((error = dlerror()) != NULL) {
      ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                    "Failed to load ValidateRequestBody: %s", error);
      exit(1);
    }
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Validating response body...");
    struct ValidateResponseBody_return validation_result =
        response_body_validator(ctx->response_body, ctx->response_body_size,
                                request->unparsed_uri.data,
                                request->unparsed_uri.len, ctx->status_code);
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Validation result: %d", validation_result.r0);
    ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                  "Validation response body: %s", validation_result.r1);
  }

  dlclose(validator_module); */

  // Piece together a JSON object
  // TODO: optimise the JSON generation process
  // {
  //   "version": "1.0.0-alpha",
  //   "dateCreated": 123456789,
  //   "executionTime": 123456789,
  //   "request": {
  //     "ip": "8.8.8.8",
  //     "httpProtocol": "HTTP/2",
  //     "uri": "http://foo.bar/baz",
  //     "resource": "",
  //     "method": "POST",
  //     "headers": {},
  //     "body": ""
  //   },
  //   "response": {
  //     "statusCode": 201,
  //     "body": "",
  //     "headers": {}
  //   }
  // }
  json_object *log_root = json_object_new_object();

  json_object *version = json_object_new_string("1.0.0-alpha");
  json_object_object_add(log_root, "version", version);

  struct timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  json_object *date_created = json_object_new_int64(
      current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000);
  json_object_object_add(log_root, "dateCreated", date_created);

  // TODO: replace with actual executionTime
  json_object *execution_time = json_object_new_int64(123456789);
  json_object_object_add(log_root, "executionTime", execution_time);

  json_object *request_object = json_object_new_object();
  json_object_object_add(log_root, "request", request_object);

  json_object *request_ip =
      json_object_new_string_len((char *)request->connection->addr_text.data,
                                 request->connection->addr_text.len);
  json_object_object_add(request_object, "ip", request_ip);

  json_object *request_protocol = json_object_new_string_len(
      (char *)request->http_protocol.data, request->http_protocol.len);
  json_object_object_add(request_object, "httpProtocol", request_protocol);

  // TODO: determine http/https
  char *full_uri = ngx_palloc(request->pool, strlen((char *)ctx->server) +
                                                 request->unparsed_uri.len +
                                                 strlen("http://") + 1);
  ngx_memcpy(full_uri, "http://", strlen("http://"));
  ngx_memcpy(full_uri + strlen("http://"), ctx->server,
             strlen((char *)ctx->server));
  ngx_memcpy(full_uri + strlen("http://") + strlen((char *)ctx->server),
             request->unparsed_uri.data, request->unparsed_uri.len);
  *(full_uri + strlen("http://") + strlen((char *)ctx->server) +
    request->unparsed_uri.len) = '\0';

  json_object *request_uri = json_object_new_string(full_uri);
  json_object_object_add(request_object, "uri", request_uri);

  json_object *request_resource = json_object_new_string_len(
      (char *)request->unparsed_uri.data, request->unparsed_uri.len);
  json_object_object_add(request_object, "resource", request_resource);

  json_object *request_method = json_object_new_string_len(
      (char *)request->method_name.data, request->method_name.len);
  json_object_object_add(request_object, "method", request_method);

  json_object *request_body = json_object_new_string_len(
      (char *)ctx->request_body, (int)ctx->request_body_size);
  json_object_object_add(request_object, "body", request_body);

  json_object *request_headers = json_object_new_object();
  for (HTTPHeader *request_header = ctx->request_headers;
       (ngx_uint_t)request_header <
       (ngx_uint_t)ctx->request_headers +
           ctx->request_header_count * sizeof(HTTPHeader);
       request_header++) {
    json_object *header_value_array = json_object_new_array_ext(1);
    json_object_object_add(request_headers, (char *)request_header->key.data,
                           header_value_array);
    json_object *header_value = json_object_new_string_len(
        (char *)request_header->value.data, (int)request_header->value.len);
    json_object_array_add(header_value_array, header_value);
  }
  json_object_object_add(request_object, "headers", request_headers);

  json_object *response_object = json_object_new_object();
  json_object_object_add(log_root, "response", response_object);

  json_object *response_status_code = json_object_new_int(ctx->status_code);
  json_object_object_add(response_object, "statusCode", response_status_code);

  json_object *response_body = json_object_new_string_len(
      (char *)ctx->response_body, (int)ctx->response_body_size);
  json_object_object_add(response_object, "body", response_body);

  json_object *response_headers = json_object_new_object();
  for (HTTPHeader *response_header = ctx->response_headers;
       (ngx_uint_t)response_header <
       (ngx_uint_t)ctx->response_headers +
           ctx->response_header_count * sizeof(HTTPHeader);
       response_header++) {
    json_object *header_value_array = json_object_new_array_ext(1);
    json_object_object_add(response_headers, (char *)response_header->key.data,
                           header_value_array);
    json_object *header_value = json_object_new_string_len(
        (char *)response_header->value.data, (int)response_header->value.len);
    json_object_array_add(header_value_array, header_value);
  }
  json_object_object_add(response_object, "headers", response_headers);

  // Log it
  ngx_log_error(
      NGX_LOG_DEBUG, request->connection->log, 0, "%s",
      json_object_to_json_string_ext(log_root, JSON_C_TO_STRING_PRETTY));

  // Curl the Firetail logging API
  // TODO: replace this with multi curl for non-blocking requests
  CURLM *multiHandler = curl_multi_init();
  CURL *curlHandler = curl_easy_init();

  int still_running = 0;

  if (curlHandler == NULL) {
    return kNextResponseBodyFilter(request, chain_head);
  }

  // Don't log the response
  curl_easy_setopt(curlHandler, CURLOPT_WRITEFUNCTION,
                   LibcurlNoopWriteFunction);

  // Set CURLOPT_ACCEPT_ENCODING otherwise emoji will break things 🥲
  curl_easy_setopt(curlHandler, CURLOPT_ACCEPT_ENCODING, "");

  // The request headers need to specify Content-Type: application/nd-json
  struct curl_slist *curl_headers = NULL;
  curl_headers =
      curl_slist_append(curl_headers, "Content-Type: application/nd-json");

  // The headers also need to provide the Firetail API key
  // TODO: check this wayyyyy earlier so we don't wastefully generate JSON etc.
  // FiretailMainConfig *main_config =
  //    ngx_http_get_module_main_conf(request, ngx_firetail_module);
  if (main_config->FiretailApiToken.len > 0) {
    char *x_ft_api_key =
        ngx_palloc(request->pool, strlen("x-ft-api-key: ") +
                                      main_config->FiretailApiToken.len);
    ngx_memcpy(x_ft_api_key, "x-ft-api-key: ", strlen("x-ft-api-key: "));
    ngx_memcpy(x_ft_api_key + strlen("x-ft-api-key: "),
               main_config->FiretailApiToken.data,
               main_config->FiretailApiToken.len);
    *(x_ft_api_key + strlen("x-ft-api-key: ") +
      main_config->FiretailApiToken.len) = '\0';
    curl_headers = curl_slist_append(curl_headers, x_ft_api_key);

    ngx_pfree(request->pool, x_ft_api_key);
  } else {
    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "FIRETAIL_API_KEY environment variable unset. Not sending "
                  "log to Firetail.");
    return kNextResponseBodyFilter(request, chain_head);
  }

  // Add the headers to the request
  curl_easy_setopt(curlHandler, CURLOPT_HTTPHEADER, curl_headers);

  // Our request body is just the log_root as a JSON string. When we add more
  // logs we'll need to add '\n' characters to separate them
  curl_easy_setopt(curlHandler, CURLOPT_POSTFIELDS,
                   json_object_to_json_string(log_root));

  // We're making a POST request to the /logs/bulk endpoint/
  curl_easy_setopt(curlHandler, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(curlHandler, CURLOPT_URL,
                   "https://api.logging.eu-west-1.prod.firetail.app/logs/bulk");

  // Do the request
  curl_multi_add_handle(multiHandler, curlHandler);
  // CURLcode res = curl_easy_perform(curlHandler);
  curl_multi_perform(multiHandler, &still_running);

  // remove handle
  curl_multi_remove_handle(multiHandler, curlHandler);
  // curl cleanup
  curl_easy_cleanup(curlHandler);
  ngx_pfree(request->pool, full_uri);

  // Pass the chain onto the next response body filter
  return kNextResponseBodyFilter(request, chain_head);
}
