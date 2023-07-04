# Firetail NGINX Module

The Firetail NGINX Module is a [dynamic module](https://docs.nginx.com/nginx/admin-guide/dynamic-modules/dynamic-modules/).



## Local Development

A [Dockerfile](./dev/Dockerfile) is provided which will build the module, install it in [an NGINX docker image](https://hub.docker.com/_/nginx), and setup a custom [nginx.conf](./dev/nginx.conf) and [index.html](./dev/index.html). It should be as simple as:

```bash
git clone git@github.com:FireTail-io/firetail-nginx-module.git
cd firetail-nginx-module
docker build -t firetail-nginx . --target firetail-nginx-dev -f dev/Dockerfile
docker run -p 8080:80 firetail-nginx
```



## Compilation

You can follow the [documentation on the NGINX blog](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/) for how to compile third-party dynamic modules for NGINX and NGINX Plus.

First clone this repository:

```bash
git clone git@github.com:FireTail-io/firetail-nginx-module.git
cd firetail-nginx-module
```

Then obtain the open source NGINX release matching the version of NGINX you use:

```bash
curl -O http://nginx.org/download/nginx-1.24.0.tar.gz
tar xvzf nginx-1.24.0.tar.gz
```

You can then use the `configure` command to generate a `makefile` to build the dynamic module:

```bash
cd nginx-1.24.0
./configure --with-compat --add-dynamic-module=../src
make modules
```

You will then need to install the Firetail NGINX Module's dependencies, [curl](https://github.com/curl/curl) and [json-c](https://github.com/json-c/json-c), before running:

```bash
make modules
```



## Configuration

To setup the Firetail NGINX Module, you will need to modify your `nginx.conf` to load it using the `load_module` directive:

```
load_module modules/ngx_firetail_module.so;
```

You can then use the `enable_firetail` directive like so:

```
enable_firetail "apikey" "firetail url";
```

See [dev/nginx.conf](./dev/nginx.conf) for an example of this in action.

