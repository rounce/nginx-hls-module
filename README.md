# nginx-based hls module

Smooth Streaming Module fork
The mod_hls module ingests the mp4 file, packages one into fragments, and delivers the fragments to iOS clients in real-time.

### Build

cd to NGINX source directory & run this:

    ./configure --add-module=/path/to/nginx-hts-module
    make
    make install

### Example nginx.conf

http {
    server {
        listen 80;
        rewrite ^(.*)\.mp4$ $1.m3u8 last;

        location ~ \.(m3u8|hls)$ {
            hls;
        }
    }
}

