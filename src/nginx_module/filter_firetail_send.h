ngx_int_t ngx_http_firetail_send(ngx_http_request_t *r, ngx_buf_t *b, char *error);
ngx_int_t ngx_http_firetail_request(ngx_http_request_t *request, ngx_buf_t *b,
                                    ngx_chain_t *chain_head, char *error);
ngx_buf_t * ngx_http_filter_buffer(ngx_http_request_t *request, char *response);
