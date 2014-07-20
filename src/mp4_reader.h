/*******************************************************************************
 mp4_reader.h - A library for reading MPEG4.

 Copyright (C) 2007-2009 CodeShop B.V.
 http://www.code-shop.com

 For licensing see the LICENSE file
******************************************************************************/

#ifndef MP4_READER_H_AKW
#define MP4_READER_H_AKW

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
  struct moov_t;

  struct atom_read_list_t {
    uint32_t type_;
    int (*destination_)(struct mp4_context_t const *mp4_context,
                        void *parent, void *child);
    void *(*reader_)(struct mp4_context_t const *mp4_context,
                     void *parent, unsigned char *buffer, uint64_t size);
  };
  typedef struct atom_read_list_t atom_read_list_t;
  MOD_STREAMING_DLL_LOCAL extern
  int atom_reader(struct mp4_context_t const *mp4_context,
                  struct atom_read_list_t *atom_read_list,
                  unsigned int atom_read_list_size,
                  void *parent,
                  unsigned char *buffer, uint64_t size);

  MOD_STREAMING_DLL_LOCAL extern
  void *moov_read(struct mp4_context_t const *mp4_context,
                  void *parent,
                  unsigned char *buffer, uint64_t size);

  MOD_STREAMING_DLL_LOCAL extern
  void *moof_read(struct mp4_context_t const *mp4_context,
                  void *parent,
                  unsigned char *buffer, uint64_t size);

  MOD_STREAMING_DLL_LOCAL extern
  int moov_build_index(struct mp4_context_t const *mp4_context,
                       struct moov_t *moov);

  MOD_STREAMING_DLL_LOCAL extern
  void *mfra_read(struct mp4_context_t const *mp4_context,
                  void *parent,
                  unsigned char *buffer, uint64_t size);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // MP4_READER_H_AKW

// End Of File

