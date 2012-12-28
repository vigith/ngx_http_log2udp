ngx_http_log2udp
================

Send out a custom access log copy to UDP for further processing.

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
* Download Nginx Source
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

(start the nginx server)
$ sudo /usr/local/nginx/sbin/nginx

you can now start the udp.pl (a useless script which with STDOUT the udp packet)
perl /path/path/ngx_http_log2udp/udp.pl

Later if you wanna change any log2udp setting, edit nginx.conf and do "sudo nginx -s reload".


Miscellaneous
=============

My dev env was Mac OS X 10.8.2. I was using nginx-1.0.15 for my dev purpose. I am quite new to nginx and my testing 
was quite minimal using 'curl' and hitting the default / (GET /). 

Author: Vigith Maurice <vigith@sharethis.com> || <v@vigith.com>
License: Same as of Nginx


