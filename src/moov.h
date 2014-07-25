/*******************************************************************************
 moov.h - A library for splitting Quicktime/MPEG4 files.

 For licensing see the LICENSE file
******************************************************************************/

#ifndef MOOV_H_AKW
#define MOOV_H_AKW

#include "mod_streaming_export.h"

#ifndef _MSC_VER
#include <inttypes.h>
#else
#include "inttypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

  struct mp4_context_t;
  struct bucket_t;

  enum input_format_t {
    INPUT_FORMAT_MP4,
    INPUT_FORMAT_FLV
  };
  typedef enum input_format_t input_format_t;

  struct mp4_split_options_t {
    float start;
    uint64_t start_integer;
    float end;
    int fragments;
    enum input_format_t input_format;
    unsigned int fragment_bitrate;
    unsigned int fragment_track_id;
    uint64_t fragment_start;
    unsigned int seconds;
    char *hash;
  };
  typedef struct mp4_split_options_t mp4_split_options_t;

  MOD_STREAMING_DLL_LOCAL extern
  mp4_split_options_t *mp4_split_options_init(ngx_http_request_t *r);
  MOD_STREAMING_DLL_LOCAL extern
  int mp4_split_options_set(ngx_http_request_t *r, mp4_split_options_t *options,
                            const char *args_data,
                            unsigned int args_size);
  MOD_STREAMING_DLL_LOCAL extern
  void mp4_split_options_exit(ngx_http_request_t *r, mp4_split_options_t *options);

  /* Returns true when the test string is a prefix of the input */
  MOD_STREAMING_DLL_LOCAL extern
  int starts_with(const char *input, const char *test);
  /* Returns true when the test string is a suffix of the input */
  MOD_STREAMING_DLL_LOCAL extern
  int ends_with(const char *input, const char *test);

  MOD_STREAMING_DLL_LOCAL extern
  int mp4_split(struct mp4_context_t *mp4_context,
                unsigned int *trak_sample_start,
                unsigned int *trak_sample_end,
                mp4_split_options_t const *options);

  MOD_STREAMING_DLL_LOCAL extern uint64_t get_filesize(const char *path);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // MOOV_H_AKW

// End Of File

