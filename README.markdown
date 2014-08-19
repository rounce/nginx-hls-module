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

hls_mp4_buffer_size
----------
**syntax:** *hls_mp4_buffer_size &lt;size&gt;*

**default:** *512k*

**context:** *http, server, location*

Sets the initial size of the buffer used for processing MP4 files.

hls_mp4_max_buffer_size
----------
**syntax:** *hls_mp4_max_buffer_size &lt;size&gt;*

**default:** *10m*

**context:** *http, server, location*

Size of moov atom may be quite large and can't exceed the hls_mp4_max_buffer_size size.
