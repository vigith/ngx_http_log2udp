ngx_http_log2udp
================

Send out a custom access log copy to UDP for further processing.

The UDP packet will have each key-value pair seperated by '^A' (0x01) and each "key and value" is '^B' (0x02) seperated, eg 

    remote_user^Bbar^Aremote_addr^B127.0.0.1^Ahttp_referer^Bvigith.com^Abody_bytes_sent^B151^Arequest^BGET / HTTP/1.1^Ahttp_user_agent^Bcurl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8r zlib/1.2.5^Astatus^B200^Atime_iso8601^B2012-12-27T16:26:20-08:00

curl command for generating the above UDP packet was

    curl --user 'bar:foo' -e 'vigith.com' http://localhost/

If the UDP server is down, then the recvfrom call will be blocked for the log2udp_timeout period (setsockopt, SO_RCVTIMEO) set 
in the nginx.conf (it will retry for LOG2UDP_MAX_RETRY which is hard-set to 3). Once the UDP server is back, it will start 
recieving the events are usual. The retry will also happen even if the send_bytes != received_bytes. 

ngx_http_log2udp, wont alter the current access.log file, so you probably won't face any issue in adopting 
this. This code hooks in at NGX_HTTP_LOG_PHASE, so even if this module craps out, only the UDP part of logging
will be affected and the request should be processed fine (if you see otherwise, it is a P1-S1 bug to me :-). 

In anycase if the process dies (seg-fault etc) (the worker), it could be because of the module, please give me the steps to 
reproduce the issue and also raise a bug. The process dying won't affect the server as a whole badly, a new thread (worker) will
be created. 

Bugs are welcome, patches are most welcome :-)


INSTALLATION
============

* download ngx_http_log2udp to /some/path
* download Nginx Source
* cd /path/to/nginx_source
* ./configure --prefix=/usr/local/nginx --add-module=/path/to/ngx_http_log2udp/
(please note i put a --prefix, this is for easy cleanup if you wanna uninstall)
* make
(any error here wrt to ngx_http_log2udp, please let me know)
* sudo make install


CONFIGURATION
=============

add the following on your nginx.conf (if you used the ./configure i have mentioned earlier,
then your conf should be in /usr/local/nginx/conf/nginx.conf)

     log2udp on;
     log2udp_server "127.0.01";
     log2udp_port 54321;
     log2udp_timeout 10000;   # 10 milliseconds (if you give 0 or skip this one, it will be overriden to 5 ms)
     log2udp_debug 2; # 1 stands for debug, 2 stands for verbose  (as of now 1 == 2)


BOOTING
=======

(start the nginx server, /usr/local/nginx/sbin/nginx if you followed my ./configure)

     $ sudo nginx

you can now start the udp.pl (a useless script which just STDOUT the udp packet). Please note that i am sending the bytes 
recvd back (in perl do a pack), else logging module will find that it doesn't match and will retry.

     perl /path/path/ngx_http_log2udp/udp.pl

Later if you wanna change any log2udp setting, edit nginx.conf and do "sudo nginx -s reload".


Miscellaneous
=============

My dev env was Mac OS X 10.8.2. I was using nginx-1.0.15 for my dev purpose. I am quite new to nginx and my testing 
was quite minimal using 'curl' and hitting the default / (GET /). 

*Author*: Vigith Maurice <vigith@sharethis.com> || <v@vigith.com>

*License*: Same as of Nginx


