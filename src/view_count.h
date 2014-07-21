#ifndef VIEW_COUNT_H_AKW
#define VIEW_COUNT_H_AKW

#include "mod_streaming_export.h"

#ifdef __cplusplus
extern "C" {
#endif

  struct mp4_context_t;

  MOD_STREAMING_DLL_LOCAL extern
  void view_count(struct mp4_context_t *mp4_context, char *filename, char *hash, char action[50]);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // VIEW_COUNT_H_AKW

