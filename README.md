## Compiling
1. Clone this code to your nginx source code
2. run `./configure --add-module=./ngx-firetail-module`
3. do `make` and optionally do `make install` if you want to install it

## Configuration
Example if you want to enable firetail

```
  server {
      listen       80;
      server_name  localhost;

      location / {
          root   html;
          index  index.html index.htm;
          enable_firetail "apikey" "firetail url";
      }
 }
```
