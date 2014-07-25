/*******************************************************************************
 output_bucket.c - A library for writing memory / file buckets.

 For licensing see the LICENSE file
******************************************************************************/

#include <nginx.h>
#include <ngx_http.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "output_bucket.h"
#include <stdlib.h>
#include <string.h>
#include "mp4_io.h"

extern bucket_t *bucket_init(ngx_http_request_t *r) {
  bucket_t *bucket = (bucket_t *)ngx_pcalloc(r->pool, sizeof(bucket_t));
  bucket->r = r;
  bucket->first = 0;
  bucket->chain = &bucket->first;
  bucket->content_length = 0;

  return bucket;
}

extern void bucket_insert(bucket_t *bucket, void const *buf, uint64_t size) {
  ngx_buf_t *b = ngx_pcalloc(bucket->r->pool, sizeof(ngx_buf_t));
  if(b == NULL) return;

  b->pos = ngx_pcalloc(bucket->r->pool, size);
  if(b->pos == NULL) return;

  if(bucket->first != 0) {
    (*bucket->chain)->buf->last_buf = 0;
    (*bucket->chain)->buf->last_in_chain = 0;
    bucket->chain = &(*bucket->chain)->next;
  }
  *bucket->chain = ngx_pcalloc(bucket->r->pool, sizeof(ngx_chain_t));
  if(*bucket->chain == NULL) return;

  b->last = b->pos + size;
  b->memory = 1;
  memcpy(b->pos, buf, size);
  b->last_buf = 1;
  b->last_in_chain = 1;

  (*bucket->chain)->buf = b;
  (*bucket->chain)->next = NULL;

  bucket->content_length += size;
}

// End Of File

