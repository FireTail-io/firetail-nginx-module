#/bin/bash
/etc/init.d/php7.4-fpm start
/etc/init.d/nginx start
tail -f /var/log/nginx/access.log
