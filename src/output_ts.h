/*******************************************************************************
 output_ts.h - A library for reading and writing Fragmented MPEG4.

 Copyright (C) 2009 CodeShop B.V.
 http://www.code-shop.com

 For licensing see the LICENSE file
******************************************************************************/

#ifndef OUTPUT_TS_H_AKW
#define OUTPUT_TS_H_AKW

#include "mod_streaming_export.h"

#ifndef _MSC_VER
#include <inttypes.h>
#else
#include "inttypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HAVE_OUTPUT_TS

  struct mp4_context_t;
  struct bucket_t;
  struct mp4_split_options_t;

// Using the Movie Fragment Random Access (MFRA) box

  MOD_STREAMING_DLL_LOCAL extern
  int output_ts(struct mp4_context_t *mp4_context,
                struct bucket_t *bucket,
                struct mp4_split_options_t const *options);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // OUTPUT_TS_H_AKW

// End Of File

