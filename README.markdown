nginx-based hls module
===========

Smooth Streaming Module fork

The module ingests the mp4 file, packages one into fragments, and delivers the fragments to iOS clients in real-time.

Build
===========

cd to NGINX source directory & run this:

    ./configure --add-module=/path/to/nginx-hls-module
    make
    make install

Example nginx.conf
----------

    http {
        server {
            listen 80;
            rewrite ^(.*)\.mp4$ $1.m3u8 last;

            location ~ \.(m3u8|ts)$ {
                hls;
                hls_length 10; # length of fragment (seconds)
            }
        }
    }

Directives
==========

hls_length
----------
**syntax:** *hls_length &lt;integer&gt;*

**default:** *8*

**context:** *http, server, location*

Set the fragment length requested without the "length" argument.

hls_relative
----------
**syntax:** *hls_relative &lt;on | off&gt;*

**default:** *off*

**context:** *http, server, location*

The directive specifies whether to keep the full URL of fragments.
