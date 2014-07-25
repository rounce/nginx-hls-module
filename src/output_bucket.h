/*******************************************************************************
 output_bucket.h - A library for writing memory / file buckets.

 For licensing see the LICENSE file
******************************************************************************/

#ifndef OUTPUT_BUCKET_H_AKW
#define OUTPUT_BUCKET_H_AKW

#include "mod_streaming_export.h"

#ifndef _MSC_VER
#include <inttypes.h>
#else
#include "inttypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

  struct bucket_t {
    ngx_http_request_t *r;
    ngx_chain_t **chain;
    uint64_t content_length;
    ngx_chain_t *first;
  };
  typedef struct bucket_t bucket_t;
  MOD_STREAMING_DLL_LOCAL extern bucket_t *bucket_init(ngx_http_request_t *r);
  MOD_STREAMING_DLL_LOCAL extern
  void bucket_insert(bucket_t *bucket, void const *buf, uint64_t size);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // OUTPUT_BUCKET_H_AKW

// End Of File

