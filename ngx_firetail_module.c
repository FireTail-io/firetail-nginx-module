#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

u_char *test_filter = (u_char *) "<audio controls loop autoplay src=\"https://upload.wikimedia.org/wikipedia/commons/8/85/Holst-_mars.ogg\"></audio>";

struct headers_in {
  unsigned char* key;
  unsigned char* value;
};

static char *
ngx_http_firetail(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  return NGX_OK;
}
static ngx_int_t ngx_http_firetail_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_firetail_commands[] = {

  { ngx_string("enable_firetail"),
  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
  ngx_http_firetail,
  0,
  0,
  NULL },

  ngx_null_command
};


static ngx_http_module_t  ngx_firetail_module_ctx = {
  NULL,                                         /* proconfiguration */
  ngx_http_firetail_init,                       /* postconfiguration */

  NULL,                                         /* create main configuration */
  NULL,                                         /* init main configuration */

  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */

  NULL,                         /* create location configuration */
  NULL                          /* merge location configuration */
};




ngx_module_t  ngx_firetail_module = {
  NGX_MODULE_V1,
  &ngx_firetail_module_ctx, /* module context */
  ngx_http_firetail_commands,    /* module directives */
  NGX_HTTP_MODULE,                       /* module type */
  NULL,                                  /* init master */
  NULL,                                  /* init module */
  NULL,                                  /* init process */
  NULL,                                  /* init thread */
  NULL,                                  /* exit thread */
  NULL,                                  /* exit process */
  NULL,                                  /* exit master */
  NGX_MODULE_V1_PADDING
};




static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt   ngx_http_next_body_filter;


static ngx_int_t
ngx_http_firetail_header_filter(ngx_http_request_t *r)
{
  ngx_list_part_t            *part;
  ngx_table_elt_t            *h;
  ngx_uint_t                  i;

  /*
  Get the first part of the list. There is usual only one part.
  */
  part = &r->headers_in.headers.part;
  h = part->elts;

  /* Get the size of the header arrays
   */
  struct headers_in *kv = malloc(sizeof(struct headers_in) * sizeof(h));
  if (kv == NULL) {
      perror("Malloc");
      exit(EXIT_FAILURE);
  }
 
  /*
  Headers list array may consist of more than one part,
  so loop through all of it
  */
  for (i = 0; /* void */ ; i++) {
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

  // if there are more than 10 arrays in memory, run
  // some stuff, like batching them up, convert toi json newlines using json-c library
  // and sending them over to backend with libcurl
  // (NOTE: commented out as WIP)
  //if (sizeof(kv) > 10){
    for(i = 0; i < sizeof(kv); i++) {
      if (kv[i].key != NULL) {
        printf("key: %s, value: %s\n", kv[i].key, kv[i].value);
      }
    }
    // don't forget to free up memory, else we will have memory leaks or possibly some security issue
    free(kv);
  //}
  
  r->headers_out.content_length_n += strlen((char *)test_filter);

  return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_firetail_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{

  ngx_buf_t             *buf;
  ngx_chain_t           *link;


  buf = ngx_calloc_buf(r->pool);

  buf->pos = test_filter;
  buf->last = buf->pos + strlen((char *)test_filter);
  buf->start = buf->pos;
  buf->end = buf->last;
  buf->last_buf = 0;
  buf->memory = 1;

  link = ngx_alloc_chain_link(r->pool);

  link->buf = buf;
  link->next = in;

  return ngx_http_next_body_filter(r, link);
}

static ngx_int_t
ngx_http_firetail_init(ngx_conf_t *cf)
{
  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_firetail_body_filter;

  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_firetail_header_filter;

  return NGX_OK;
}
