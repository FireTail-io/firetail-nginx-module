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

When you first make a request to [localhost:8080](http://localhost:8080) you will likely see a log similar to the following:

```
2023/07/14 11:44:37 [debug] 29#29: *1 Status code from POST request to Firetail /logs/bulk endpoint: 401
```

The Firetail NGINX Module will be receiving 401 responses as you have not yet configured your [nginx.conf](./nginx.conf) with a valid Firetail logs API token. You will need to update the following directive with a valid token:

```
  firetail_api_token "YOUR-API-TOKEN";
```



### Request Validation

To demonstrate request validation a `POST /proxy/profile/{username}/comment` operation is defined in the provided [appspec.yml](./dev/appspec.yml), with one profile defined in the provided [nginx.conf](./dev/nginx.conf): `POST /proxy/profile/alice/comment`. A `proxy_pass` directive is used to forward requests to `/profile/alice/comment` as the request body validation will not occur on locations where the `return` directive is used.

Making a curl request to `POST /profile/alice/comment` should yield the following result, which validates successfully against the provided appspec:

```bash
curl localhost:8080/proxy/profile/alice/comment -X POST -H "Content-Type: application/json" -d '{"comment":"Hello world!"}
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
./configure --with-compat --add-dynamic-module=../src/nginx_module
make modules
```

You will then need to install the Firetail NGINX Module's dependencies, [curl](https://github.com/curl/curl) and [json-c](https://github.com/json-c/json-c), before running:

```bash
make modules
```

The Firetail NGINX module is also dependent upon a validator module, written in Go.

// TODO: docs for building the Golang validator 



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

