ARG NGINX_VERSION=1.24.0

FROM golang:1.21.0-bullseye AS build-golang

# Make a /src and /dist directory, with our workdir set to /src
WORKDIR /src
RUN mkdir /dist

# Copy in our go source for the validator
COPY src/validator /src

# Build the go source as a c-shared lib, outputting to /dist as /dist/validator.so
# NOTE: this will also create a /dist/validator.h which is not needed, so it's discarded it afterwards
RUN CGO_ENABLED=1 go build -buildmode c-shared -o /dist/firetail-validator.so .
RUN rm /dist/firetail-validator.h

FROM debian:bullseye-slim AS build-c
ARG NGINX_VERSION

# Curl the tarball of the nginx version matching that of the container we want to modify
RUN apt update && apt install -y curl
RUN curl http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz -o /tmp/nginx-${NGINX_VERSION}.tar.gz && \
    cd /tmp && \
    tar xvzf nginx-${NGINX_VERSION}.tar.gz

# Install dependencies for our dynamic module
RUN apt install -y build-essential libpcre++-dev zlib1g-dev libcurl4-openssl-dev libjson-c-dev

# Build our dynamic module
COPY src/nginx_module /tmp/ngx-firetail-module
RUN cd /tmp/nginx-${NGINX_VERSION} && \
    ./configure --with-compat --add-dynamic-module=/tmp/ngx-firetail-module && \
    make modules

# Copy our dynamic module & its dependencies into the image for the nginx version we want to use
FROM nginx:${NGINX_VERSION} AS firetail-nginx
RUN apt-get update && apt-get install -y libjson-c-dev
COPY --from=build-golang /dist/firetail-validator.so /etc/nginx/modules/
COPY --from=build-c /tmp/nginx-${NGINX_VERSION}/objs/ngx_firetail_module.so /etc/nginx/modules/

# An image for local dev with a custom nginx.conf and index.html
FROM firetail-nginx as firetail-nginx-dev
COPY dev/appspec.yml /etc/nginx/appspec.yml
COPY dev/nginx.conf /etc/nginx/nginx.conf
COPY dev/index.html /usr/share/nginx/html/
CMD ["nginx-debug", "-g", "daemon off;"]

FROM firetail-nginx as firetail-nginx-dev-php
RUN apt-get install php7.4 php7.4-fpm -y && mkdir /var/www/html -p && chmod -R 777 /var/www/html && echo "<?php phpinfo(); ?>" >> /var/www/html/info.php
COPY dev/appspec.yml /etc/nginx/appspec.yml
COPY examples/php/nginx.conf /etc/nginx/nginx.conf
COPY examples/php/hello.php /var/www/html/hello.php
COPY examples/php/hello.css /var/www/html/hello.css
COPY examples/php/http.js /etc/nginx/http.js
COPY examples/php/startup.sh /startup.sh
RUN chmod +x /startup.sh
RUN mkdir -p /data/nginx/cache
ENTRYPOINT ["/bin/bash", "/startup.sh"]

# An image for Kubernetes ingress
FROM nginx/nginx-ingress:3.7.0 as firetail-nginx-ingress
USER root
RUN mkdir -p /var/lib/apt/lists/partial && apt-get update && apt-get install -y libjson-c-dev
COPY --from=build-golang /dist/firetail-validator.so /etc/nginx/modules/
COPY --from=build-c /tmp/nginx-${NGINX_VERSION}/objs/ngx_firetail_module.so /etc/nginx/modules/
USER nginx

# A dev image for Kubernetes ingress
FROM firetail-nginx-ingress AS firetail-nginx-ingress-dev
COPY --chown=nginx:nginx examples/kubernetes/appspec.yml /etc/nginx/appspec.yml 
