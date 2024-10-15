# Firetail NGINX Module

The Firetail NGINX Module is a [dynamic module](https://docs.nginx.com/nginx/admin-guide/dynamic-modules/dynamic-modules/).



## Build Process

The easiest way to attain the binaries for the FireTail NGINX module is to use [the provided Dockerfile](./Dockerfile). By default, it uses NGINX 1.24.0 - you need to ensure this matches the version of NGINX you are using. You can adjust the version used via the build argument NGINX_VERSION. For example, to build for NGINX 1.25.0:

```bash
git clone git@github.com:FireTail-io/firetail-nginx-module.git
cd firetail-nginx-module
docker build -t firetail-nginx . --target firetail-nginx --build-arg="NGINX_VERSION=1.25.0"
docker run -v ./dist:/dist firetail-nginx cp /usr/lib/nginx/modules/{ngx_firetail_module.so,firetail-validator.so} /dist
```

You should then find the binaries `ngx_firetail_module.so` and `firetail-validator.so` in a `dist` directory, which you will need to place in your NGINX instances' modules directory - Conventionally this is `/etc/nginx/modules` - then load it using [the `load_module` directive](https://nginx.org/en/docs/ngx_core_module.html#load_module).

If you want to DIY, you can follow [the guidance below](#DIY-Build-Process).



## Configuration

To setup the Firetail NGINX Module, you will need to modify your `nginx.conf` to load it using the `load_module` directive:

```
load_module modules/ngx_firetail_module.so;
```

You can then use the `firetail_api_token` directive to provide your Firetail logging API token inside a http block like so:

```
  firetail_api_token "YOUR-API-TOKEN";
```

See [dev/nginx.conf](./dev/nginx.conf) for an example of this in action.

You should use a module such as the [ngx_http_lua_module](https://github.com/openresty/lua-nginx-module) to avoid placing plaintext credentials in your `nginx.conf`, and instead make use of [system environment variables](https://github.com/openresty/lua-nginx-module#system-environment-variable-support).

Once you've configured your `nginx.conf` you will also need to provide an OpenAPI specification. The FireTail NGINX Module expects to find your OpenAPI specification at `/etc/nginx/appspec.yml`.



## Kubernetes Example Setup

An example setup for the Firetail NGINX plugin installed on an NGINX-ingress image for Kubernetes is included in the [Dockerfile](./Dockerfile) in this repository. You can build it as follows:

```bash
git clone git@github.com:FireTail-io/firetail-nginx-module.git
cd firetail-nginx-module
docker build -t firetail-nginx-ingress-dev . --target firetail-nginx-ingress-dev --build-arg="NGINX_VERSION=1.27.1"
```

You can then modify the [complete example](https://github.com/nginxinc/kubernetes-ingress/tree/main/examples/ingress-resources/complete-example) in the [nginxinc/kubernetes-ingress repository](https://github.com/nginxinc/kubernetes-ingress) to use the NGINX-ingress image you just built. First, clone the repository:

```bash
git clone git@github.com:nginxinc/kubernetes-ingress.git
cd kubernetes-ingress
```

You'll then need to change the image used in `deployments/daemon-set/nginx-ingress.yaml` for the `nginx-ingress` container to `firetail-nginx-ingress-dev`.

```yaml
...
      containers:
        - image: firetail-nginx-ingress-dev
          name: nginx-ingress
...
```

Follow [the instructions linked from the example](https://docs.nginx.com/nginx-ingress-controller/installation/installing-nic/installation-with-manifests/) to setup the nginx-ingress:

```bash
kubectl apply -f deployments/common/ns-and-sa.yaml
kubectl apply -f deployments/rbac/rbac.yaml
kubectl apply -f examples/shared-examples/default-server-secret/default-server-secret.yaml
kubectl apply -f deployments/common/nginx-config.yaml
kubectl apply -f deployments/common/ingress-class.yaml
kubectl apply -f https://raw.githubusercontent.com/nginxinc/kubernetes-ingress/v3.7.0/deploy/crds.yaml
kubectl apply -f deployments/daemon-set/nginx-ingress.yaml
kubectl create -f deployments/service/nodeport.yaml
```

You should then be able to see the `nginx-ingress` pod in a `Running` state:

```bash
kubectl get pods --namespace=nginx-ingress
```

```bash
NAME                  READY   STATUS    RESTARTS   AGE
nginx-ingress-g6tss   1/1     Running   0          7s
```

Then follow the instructions for the [complete example](https://github.com/nginxinc/kubernetes-ingress/tree/main/examples/ingress-resources/complete-example) in the [nginxinc/kubernetes-ingress repository](https://github.com/nginxinc/kubernetes-ingress):

```bash
kubectl create -f examples/ingress-resources/complete-example/cafe.yaml
kubectl create -f examples/ingress-resources/complete-example/cafe-secret.yaml
kubectl create -f examples/ingress-resources/complete-example/cafe-ingress.yaml
```

Find the port used by the `nginx-ingress`:

```bash
kubectl get service --namespace=nginx-ingress
```

```bash
NAME            TYPE       CLUSTER-IP       EXTERNAL-IP   PORT(S)                      AGE
nginx-ingress   NodePort   10.106.182.250   <none>        80:32724/TCP,443:32334/TCP   13s
```

You should then be able to `curl` the tea or coffee endpoint as follows:

```bash
export CAFE_PORT=32334
curl --resolve cafe.example.com:$CAFE_PORT:0.0.0.0 https://cafe.example.com:$CAFE_PORT/tea --insecure
```

```
Server address: 10.1.0.78:8080
Server name: tea-df5655878-7blfk
Date: 08/Oct/2024:11:06:13 +0000
URI: /tea
Request ID: 8292a274a2774d7e5257c53dcb8adbe6
```

In order for the `nginx-ingest` to load the FireTail module we need to add a `load_module` directive to the main block, and `firetail_api_token` and `firetail_url` directives to the `nginx.conf`. This can be done using a `ConfigMap` like this:

```yaml
kind: ConfigMap
apiVersion: v1
metadata:
  name: nginx-config
  namespace: nginx-ingress
data:
  main-snippets: |
    load_module modules/ngx_firetail_module.so;
  http-snippets: |
    firetail_api_token "YOUR_API_TOKEN_HERE";
    firetail_url "https://api.logging.eu-west-1.prod.firetail.app/logs/bulk";
```

Modify this file to include your own API token from the FireTail platform, and update the Firetail URL to match the region you're using. You can then save it and apply it like so:

```bash
kubectl apply -f my-firetail-config-map.yaml
```

This will update the `nginx.conf` file in the `nginx-ingress` container to load the FireTail module and provide your API token and FireTail URL.

You should still be able to curl the `/tea` endpoint, as it is included in [the example OpenAPI specification used in the  `firetail-nginx-ingress-dev` image](./dev/appspec.yml):

```bash
curl --resolve cafe.example.com:$CAFE_PORT:0.0.0.0 https://cafe.example.com:$CAFE_PORT/tea --insecure
```

```
Server address: 10.1.0.77:8080
Server name: tea-df5655878-s5rbl
Date: 08/Oct/2024:11:09:40 +0000
URI: /tea
Request ID: 2a094910b76a06a3a11a5820df10d56c
```

However, if you try and curl the `/coffee` endpoint your request should be blocked by the FireTail module as it is not defined in the OpenAPI specification.

```bash
curl --resolve cafe.example.com:$CAFE_PORT:0.0.0.0 https://cafe.example.com:$CAFE_PORT/coffee --insecure
```

```json
{"code":404,"title":"the resource \"/coffee\" could not be found","detail":"a path for \"/coffee\" could not be found in your appspec"}
```



## Local Development

A [Dockerfile](./Dockerfile) is provided which will build the module, install it in [an NGINX docker image](https://hub.docker.com/_/nginx), and setup a custom [nginx.conf](./dev/nginx.conf) and [index.html](./dev/index.html). It should be as simple as:

```bash
git clone git@github.com:FireTail-io/firetail-nginx-module.git
cd firetail-nginx-module
docker build -t firetail-nginx . --target firetail-nginx-dev
docker run -p 8080:80 firetail-nginx
```

When you first make a request to [localhost:8080](http://localhost:8080) you will likely eventually see a log similar to the following:

```
2023/07/14 11:44:37 [debug] 29#29: *1 Status code from POST request to Firetail /logs/bulk endpoint: 401
```

The Firetail NGINX Module will be receiving 401 responses as you have not yet configured your [nginx.conf](./nginx.conf) with a valid Firetail logs API token. You will need to update the following directive with a valid token:

```
  firetail_api_token "YOUR-API-TOKEN";
```



### VSCode

For local development with VSCode you'll probably want to download the nginx tarball matching the version you're developing for, and configure it:

```bash
curl -O http://nginx.org/download/nginx-1.24.0.tar.gz
tar xvzf nginx-1.24.0.tar.gz
cd nginx-1.24.0
./configure
```

Then install VSCode's [Microsoft C/C++ Extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). By default it should happily be finding your nginx headers. If it's not, go to its settings using the command `C/C++: Edit configurations (UI)` and add the following `includePath`:

```
${workspaceFolder}/**
```

You'll then need to install some of nginx's dependencies (`pcre2`), and the dependenceis of the firetail NGINX module itself (`curl` and `json-c`). You can use [vcpkg](https://vcpkg.io/) for this:

```bash
vcpkg install pcre2 curl json-c
```



### Request Validation

To demonstrate request validation a `POST /proxy/profile/{username}/comment` operation is defined in the provided [appspec.yml](./dev/appspec.yml), with one profile defined in the provided [nginx.conf](./dev/nginx.conf): `POST /proxy/profile/alice/comment`. A `proxy_pass` directive is used to forward requests to `/profile/alice/comment` as the request body validation will not occur on locations where the `return` directive is used.

Making a curl request to `POST /profile/alice/comment` should yield the following result, which validates successfully against the provided appspec:

```bash
curl localhost:8080/proxy/profile/alice/comment -X POST -H "Content-Type: application/json" -d '{"comment":"Hello world!"}'
```

```json
{"message":"Success!"}
```

If you alter the request body in any way that deviates from the appspec you will receive an error response from the Firetail nginx module instead:

```bash
curl localhost:8080/proxy/profile/alice/comment -X POST -H "Content-Type: application/json" -d '{"comment":12345}'
```

```json
{"code":400,"title":"something's wrong with your request body","detail":"the request's body did not match your appspec: request body has an error: doesn't match the schema: Error at \"/comment\": field must be set to string or not be present\nSchema:\n  {\n    \"type\": \"string\"\n  }\n\nValue:\n  \"number, integer\"\n"}
```



### Response Validation

To demonstrate response validation a `GET /profile/{username}` operation is defined in the provided [appspec.yml](./dev/appspec.yml), and two profiles are defined in the provided [nginx.conf](./dev/nginx.conf): `GET /profile/alice` and `GET /profile/bob`.

Making a curl request to `GET /profile/alice` should yield the following result, which validates successfully against the provided appspec, as it returns only Alice's username and friend count:

```bash
curl localhost:8080/profile/alice
```

```json
{"username":"alice", "friends": 123456789}
```

Making a curl request to `GET /profile/bob` will yield a different result, as the response body defined in our nginx.conf erroneously includes Bob's address. This does not validate against our appspec, so the response body is overwritten by the Firetail middleware, which can protect Bob from having his personally identifiable information disclosed by our faulty application. For the purposes of this demo, the Firetail library's debugging responses are enabled so we get a verbose explanation of the problem:

```bash
curl localhost:8080/profile/bob
```

```json
{
    "code": 500,
    "title": "internal server error",
    "detail": "the response's body did not match your appspec: response body doesn't match the schema: property \"address\" is unsupported\nSchema:\n  {\n    \"additionalProperties\": false,\n    \"properties\": {\n      \"friends\": {\n        \"minimum\": 0,\n        \"type\": \"integer\"\n      },\n      \"username\": {\n        \"type\": \"string\"\n      }\n    },\n    \"type\": \"object\"\n  }\n\nValue:\n  {\n    \"address\": \"Oh dear, this shouldn't be public!\",\n    \"friends\": 123456789,\n    \"username\": \"bob\"\n  }\n"
}
```





## DIY Build Process

If you want to DIY, you can follow the [documentation on the NGINX blog](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/) for how to compile third-party dynamic modules for NGINX and NGINX Plus.

First clone this repository:

```bash
git clone git@github.com:FireTail-io/firetail-nginx-module.git
cd firetail-nginx-module
```

Then obtain the open source NGINX release matching the version of NGINX you use:

```bash
curl -O http://nginx.org/download/nginx-1.24.0.tar.gz
tar xvzf nginx-1.24.0.tar.gz
cd nginx-1.24.0
```

Then install the Firetail NGINX Module's dependencies, [curl](https://github.com/curl/curl) and [json-c](https://github.com/json-c/json-c).

You can then use the `configure` command to generate a `makefile` to build the dynamic module. You may need to use the `--with-ld-opt` and `--with-cc-opt` options.

```bash
./configure --with-compat --add-dynamic-module=../src/nginx_module --with-ld-opt="-L/opt/homebrew/lib" --with-cc-opt="-I/opt/homebrew/include"
```

The `configure` command will create a makefile you can then use to build the module:

```bash
make modules
```

This should yield a file called `ngx_firetail_module.so` in the `objs` directory - this is the plugin, however it is dependant on a second binary which also needs to be compiled:

```bash
cd ../src/validator
CGO_ENABLED=1 go build -buildmode c-shared -o firetail-validator.so .
```

This should yield a file called `firetail-validator.so`.

You will need to take copies of the `ngx_firetail_module.so` and `firetail-validator.so` binaries and place them in `/etc/nginx/modules/`.
