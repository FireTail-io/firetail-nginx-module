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

// This header filter currently collects up all the headers & logs them, POSTS a
// partial request log to localhost:3000 and updates the content-length response
// header to be valid after the prepending of `firetail_message`
struct headers_in {
  unsigned char *key;
  unsigned char *value;
};

typedef struct batched_headers {
  struct headers_in *value;
  struct node *next;
} batched_headers_node_t;

static ngx_int_t ngx_http_firetail_header_filter(ngx_http_request_t *r) {
  ngx_list_part_t *part;
  ngx_table_elt_t *h;
  ngx_uint_t i;

  /*
  Get the first part of the list. There is usual only one part.
  */
  part = &r->headers_in.headers.part;
  h = part->elts;

  /* Get the size of the header arrays
   */
  struct headers_in *kv = malloc(sizeof(struct headers_in) * sizeof(h));
  if (kv == NULL) {
    perror("malloc error");
    exit(EXIT_FAILURE);
  }

  batched_headers_node_t *batched_headers = NULL;
  batched_headers =
      (batched_headers_node_t *)malloc(sizeof(batched_headers_node_t));
  if (batched_headers == NULL) {
    perror("malloc error");
    exit(EXIT_FAILURE);
  }

  /*
  Headers list array may consist of more than one part,
  so loop through all of it
  */
  for (i = 0; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        /* The last part, search is done. */
        break;
      }

      part = part->next;
      h = part->elts;
      i = 0;
    }

    /*
    Ta-da, we got one!
    Note, we'v stop the search at the first matched header
    while more then one header may fit.
    */
    // Check of the headers contains any NULLs
    if (h[i].key.data != NULL && h[i].value.data != NULL) {
      kv[i].key = h[i].key.data;
      kv[i].value = h[i].value.data;
    }
  }

  batched_headers->value = kv;
  // batched_headers->next = (batched_headers_node_t batched_headers_node_t));
  batched_headers->next = NULL;

  // below code is for http client
  unsigned char *protocol = r->http_protocol.data;
  unsigned char *method_name = r->method_name.data;
  unsigned char *ip_address = r->connection->addr_text.data;
  unsigned char *url = r->unparsed_uri.data;

  // initialize json object
  json_object *json_obj = json_object_new_object();

  // version
  json_object *version = json_object_new_string("1.0.0-alpha");
  json_object_object_add(json_obj, "version", version);

  // current time
  json_object *datetime = json_object_new_int(time(NULL));
  json_object_object_add(json_obj, "dateCreated", datetime);

  // initialize request object
  json_object *requests = json_object_new_object();

  // request http protocol
  json_object *http_protocol = json_object_new_string((char *)protocol);
  json_object_object_add(requests, "httpProtocol", http_protocol);

  // request method
  json_object *method = json_object_new_string((char *)method_name);
  json_object_object_add(requests, "method", method);

  // request IP
  json_object *ip = json_object_new_string((char *)ip_address);
  json_object_object_add(requests, "ip", ip);

  // request url
  json_object *url_path = json_object_new_string((char *)url);
  json_object_object_add(requests, "url", url_path);

  // add request object into request json key
  json_object_object_add(json_obj, "request", requests);

  // send data to firetail backend
  CURL *curlHandler = curl_easy_init();

  if (curlHandler) {
    // header stuff
    struct curl_slist *hs = NULL;
    hs = curl_slist_append(hs, "Content-Type: application/nd-json");

    // just setup a simple webserver that binds to localhost:3000 to debug the
    // json payload this url should point to firetail backend after debugging
    // this code
    curl_easy_setopt(curlHandler, CURLOPT_URL, "http://localhost:3000");
    curl_easy_setopt(curlHandler, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curlHandler, CURLOPT_POSTFIELDS,
                     json_object_to_json_string(json_obj));
    curl_easy_setopt(curlHandler, CURLOPT_HTTPHEADER, hs);

    CURLcode res = curl_easy_perform(curlHandler);

    if (res != CURLE_OK)
      fprintf(stderr, "CURL failed: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curlHandler);
    free(json_obj);
  }

  for (i = 0; i < sizeof(kv); i++) {
    if (kv[i].key != NULL) {
      printf("key: %s, value: %s\n", kv[i].key, kv[i].value);
    }
  }
  // don't forget to free up memory, else we will have memory leaks or possibly
  // some security issue
  free(kv);

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