#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
#define __STDC_FORMAT_MACROS // C++ should define this for PRIu64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ngx_md5.h>
#include <curl/curl.h>

#include "view_count.h"
#include "mp4_io.h"

static ngx_uint_t ngx_hls_shm_size;
static ngx_shm_zone_t *ngx_hls_shm_zone;
static ngx_rbtree_t *ngx_hls_rbtree;

extern void view_count(struct mp4_context_t *mp4_context, char *filename, char *hash, u_char action[50]) {
  if(hash == NULL) return;
  char str[256];
  char file[256];
  strcpy(file, filename);
  char *fkey = strrchr(file, '/');
  *fkey = 0;
  fkey++;
  char *key = strrchr(file, '/') + 1;

  sprintf(str, "http://p.krasview.ru:9000/?action=%s&key=%s&hash=%s", action, key, hash);
  MP4_INFO("uri: %s", str);
  CURL *curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, str);
    curl_easy_perform(curl);

    curl_easy_cleanup(curl);
  }
}

// End Of File

