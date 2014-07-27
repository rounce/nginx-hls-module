/*******************************************************************************
 output_m3u8.c - A library for writing M3U8 playlists.

 For licensing see the LICENSE file
******************************************************************************/

#ifndef OUTPUT_M3U8_H_AKW
#define OUTPUT_M3U8_H_AKW

#include "mod_streaming_export.h"

#ifdef __cplusplus
extern "C" {
#endif

  struct mp4_context_t;
  struct bucket_t;
  struct mp4_split_options_t;

  MOD_STREAMING_DLL_LOCAL extern
  int mp4_create_m3u8(struct mp4_context_t *mp4_context,
                      struct bucket_t *bucket, unsigned int length);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // OUTPUT_M3U8_H_AKW

// End Of File

