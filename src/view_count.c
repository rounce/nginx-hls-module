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

#include "view_count.h"
#include "mp4_io.h"

extern void view_count(struct mp4_context_t *mp4_context, char *filename, char *hash, char action[50]) {
  // Your code. For example I send to server (via curl) the watching progress of video.
}

// End Of File

