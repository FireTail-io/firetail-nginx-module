FROM debian:bullseye-slim AS build

ENV NGINX_VERSION 1.24.0

# Curl the tarball of the nginx version matching that of the container we want to modify
RUN apt update && apt install -y curl
RUN curl http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz -o /tmp/nginx-${NGINX_VERSION}.tar.gz && \
    cd /tmp && \
    tar xvzf nginx-${NGINX_VERSION}.tar.gz

# Install dependencies for our dynamic module
RUN apt install -y build-essential libpcre++-dev zlib1g-dev libcurl4-openssl-dev libjson-c-dev

# Build our dynamic module
COPY src /tmp/ngx-firetail-module
RUN cd /tmp/nginx-${NGINX_VERSION} && \
    ./configure --with-compat --add-dynamic-module=/tmp/ngx-firetail-module && \
    make modules

# Copy our dynamic module & its dependencies into the image for the nginx version we want to use
FROM nginx:1.24.0 AS firetail-nginx
RUN apt-get update && apt-get install -y libjson-c-dev
COPY --from=build /tmp/nginx-${NGINX_VERSION}/objs/* /etc/nginx/modules/

# An image for local dev with a custom nginx.conf and index.html
FROM firetail-nginx as firetail-nginx-dev
COPY dev/nginx.conf /etc/nginx/nginx.conf
COPY dev/index.html /usr/share/nginx/html/
