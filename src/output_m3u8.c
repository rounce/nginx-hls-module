/*******************************************************************************
 output_m3u8.c - A library for writing M3U8 playlists.

 Copyright (C) 2009 CodeShop B.V.
 http://www.code-shop.com

 For licensing see the LICENSE file
******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
#define __STDC_FORMAT_MACROS // C++ should define this for PRIu64
#endif

#include "output_m3u8.h"
#include "mp4_io.h"
#include "mp4_reader.h"
#include "output_bucket.h"
#include "moov.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int mp4_create_m3u8(struct mp4_context_t *mp4_context,
                           struct bucket_t *bucket, struct mp4_split_options_t const *options) {
  int result = 0;
  u_char *buffer = (u_char *)ngx_palloc(mp4_context->r->pool, 1024 * 256);
  u_char *p = buffer;
  char extra[100] = "";
  //if(options->fragment_track_id) p_extra = ngx_sprintf(p_extra, "&audio=%ud", options->fragment_track_id);
  //if(options->hash) p_extra = ngx_sprintf(p_extra, "&hash=%s", options->hash);
  strncpy(extra, (const char *)mp4_context->r->args.data, mp4_context->r->args.len < 100 ? mp4_context->r->args.len : 100);

  p = ngx_sprintf(p, "#EXTM3U\n");

  char *filename = (char *)ngx_palloc(mp4_context->r->pool, ngx_strlen(mp4_context->file->name.data) - mp4_context->root + 2);
  strcpy(filename, (const char *)(mp4_context->file->name.data + mp4_context->root));
  char *ext = strrchr(filename, '.');
  *ext = 0;
  //strcpy(ext, ".m3u8");

  if(!moov_build_index(mp4_context, mp4_context->moov)) return 0;
  moov_t const *moov = mp4_context->moov;

  // http://developer.apple.com/library/ios/#technotes/tn2288/_index.html
  // http://tools.ietf.org/html/draft-pantos-http-live-streaming-08
  unsigned int audio_tracks = 0;
  /*  if(!options->fragment_track_id) {
      unsigned int track_id;
      for(track_id = 0; track_id < moov->tracks_; ++track_id) {
        if(moov->traks_[track_id]->mdia_->hdlr_->handler_type_ == FOURCC('s', 'o', 'u', 'n')) ++audio_tracks;
      }
      if(audio_tracks > 1) {
        unsigned int i = 0;
        for(track_id = 0; track_id < moov->tracks_; ++track_id) {
          if(moov->traks_[track_id]->mdia_->hdlr_->handler_type_ == FOURCC('s', 'o', 'u', 'n')) {
  //          p = ngx_sprintf(p, "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"track\",NAME=\"%d\",AUTOSELECT=YES,DEFAULT=%s,LANGUAGE=\"%s\",URI=\"%s?audio=%d%s\"\n",
  //                          track_id, !i ? "YES" : "NO", i != audio_tracks - 1 ? "rus" : "eng",
            p = ngx_sprintf(p, "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"track\",NAME=\"%d\",DEFAULT=%s,URI=\"%s?audio=%d%s\"\n",
                            track_id, i ? "YES" : "NO",
                            filename, track_id, extra);
            ++i;
          }
        }
        unsigned int bitrate = ((trak_bitrate(moov->traks_[0]) + 999) / 1000) * 1000;

        p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,VIDEO=\"track\",BANDWIDTH=%ud\n", bitrate);
        p = ngx_sprintf(p, "%s?audio=%d%s\n", filename, 1, extra);
      }
    }*/

  if(audio_tracks < 2) {
    trak_t const *trak = moov->traks_[0];
    samples_t *cur = trak->samples_;
    samples_t *prev = cur;
    samples_t *last = trak->samples_ + trak->samples_size_ + 1;

    p = ngx_sprintf(p, "#EXT-X-TARGETDURATION:%ud\n", options->seconds ? options->seconds + 3 : 4);
    p = ngx_sprintf(p, "#EXT-X-MEDIA-SEQUENCE:0\n");
    p = ngx_sprintf(p, "#EXT-X-VERSION:4\n");

    u_int i = 0, prev_i = 0;
    while(cur != last) {
      if(!cur->is_smooth_ss_) {
        ++cur;
        continue;
      }

      if(prev != cur) {
        float duration = (float)((cur->pts_ - prev->pts_) / (float)trak->mdia_->mdhd_->timescale_) + 0.0005;
        if(duration >= (float)options->seconds || cur + 1 == last) {
          p = ngx_sprintf(p, "#EXTINF:%.3f,\n", duration);
          p = ngx_sprintf(p, "%s.hls?video=%uL&%s\n", filename, prev_i, extra);
          prev = cur;
          prev_i = i;
          ++result;
        }
      }
      ++i;
      ++cur;
    }
    p = ngx_sprintf(p, "#EXT-X-ENDLIST\n");
  }

  bucket_insert(bucket, buffer, p - buffer);
  ngx_pfree(mp4_context->r->pool, buffer);
  ngx_pfree(mp4_context->r->pool, filename);

  return result;
}

// End Of File

