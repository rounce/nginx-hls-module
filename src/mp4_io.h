/*******************************************************************************
 mp4_io.h - A library for general MPEG4 I/O.

 Copyright (C) 2007-2009 CodeShop B.V.
 http://www.code-shop.com

 For licensing see the LICENSE file
******************************************************************************/

#ifndef MP4_IO_H_AKW
#define MP4_IO_H_AKW

#include "mod_streaming_export.h"

#include <nginx.h>
#include <ngx_http.h>
#ifndef _MSC_VER
#include <inttypes.h>
#else
#include "inttypes.h"
#endif
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct mp4_context_t;

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#ifdef WIN32
#define ftello _ftelli64
#define fseeko _fseeki64
// #define strdup _strdup
#endif

#define ATOM_PREAMBLE_SIZE 8

#define MAX_TRACKS 8

#define FOURCC(a, b, c, d) ((uint32_t)(a) << 24) + \
  ((uint32_t)(b) << 16) + \
  ((uint32_t)(c) << 8) + \
  ((uint32_t)(d))

#define MP4_INFO(fmt, ...); \
  mp4_log_trace(mp4_context, NGX_LOG_DEBUG_HTTP, "%s.%d: (info) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);

#define MP4_WARNING(fmt, ...) \
  mp4_log_trace(mp4_context, NGX_LOG_WARN, "%s.%d: (warning) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);

#define MP4_ERROR(fmt, ...) \
  mp4_log_trace(mp4_context, NGX_LOG_ERR, "%s.%d: (error) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);
//    mp4_log_trace(mp4_context, NGX_LOG_DEBUG_HTTP, "%s.%d: (error) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);

  MOD_STREAMING_DLL_LOCAL extern uint64_t atoi64(const char *val);

  MOD_STREAMING_DLL_LOCAL extern const char *remove_path(const char *path);
  MOD_STREAMING_DLL_LOCAL extern void mp4_log_trace(const struct mp4_context_t *mp4_context, ngx_uint_t level, const char *fmt, ...);

  MOD_STREAMING_DLL_LOCAL extern unsigned int read_8(unsigned char const *buffer);
  MOD_STREAMING_DLL_LOCAL extern unsigned char *write_8(unsigned char *buffer, unsigned int v);
  MOD_STREAMING_DLL_LOCAL extern uint16_t read_16(unsigned char const *buffer);
  MOD_STREAMING_DLL_LOCAL extern unsigned char *write_16(unsigned char *buffer, unsigned int v);
  MOD_STREAMING_DLL_LOCAL extern unsigned int read_24(unsigned char const *buffer);
  MOD_STREAMING_DLL_LOCAL extern unsigned char *write_24(unsigned char *buffer, unsigned int v);
  MOD_STREAMING_DLL_LOCAL extern uint32_t read_32(unsigned char const *buffer);
  MOD_STREAMING_DLL_LOCAL extern unsigned char *write_32(unsigned char *buffer, uint32_t v);
  MOD_STREAMING_DLL_LOCAL extern uint64_t read_64(unsigned char const *buffer);
  MOD_STREAMING_DLL_LOCAL extern unsigned char *write_64(unsigned char *buffer, uint64_t v);
  MOD_STREAMING_DLL_LOCAL extern uint32_t read_n(unsigned char const *buffer, unsigned int n);
  MOD_STREAMING_DLL_LOCAL extern unsigned char *write_n(unsigned char *buffer, unsigned int n, uint32_t v);

  struct mem_range_t {
    int read_only_;
    uint64_t filesize_;
    int fd_;

    // original base mapping
    void *mmap_addr_;
    uint64_t mmap_offset_;
    uint64_t mmap_size_;

#ifdef WIN32
    void *fileMapHandle_;
#endif
  };
  typedef struct mem_range_t mem_range_t;

  MOD_STREAMING_DLL_LOCAL extern mem_range_t *mem_range_init_read(char const *filename);
  MOD_STREAMING_DLL_LOCAL extern mem_range_t *mem_range_init_write(char const *filename,
      uint64_t offset, uint64_t len);
  MOD_STREAMING_DLL_LOCAL extern void *mem_range_map(mem_range_t *mem_range, uint64_t offset, uint32_t len);
  MOD_STREAMING_DLL_LOCAL extern void mem_range_exit(mem_range_t *mem_range);

  struct mp4_atom_t {
    uint32_t type_;
    uint32_t short_size_;
    uint64_t size_;
    uint64_t start_;
    uint64_t end_;
  };
  typedef struct mp4_atom_t mp4_atom_t;

  struct mp4_context_t;
  MOD_STREAMING_DLL_LOCAL extern
  int mp4_atom_write_header(unsigned char *outbuffer,
                            mp4_atom_t const *atom);

  struct unknown_atom_t {
    void *atom_;
    struct unknown_atom_t *next_;
  };
  typedef struct unknown_atom_t unknown_atom_t;
  MOD_STREAMING_DLL_LOCAL extern unknown_atom_t *unknown_atom_init(void);
  MOD_STREAMING_DLL_LOCAL extern void unknown_atom_exit(unknown_atom_t *atom);

  struct moov_t {
    struct unknown_atom_t *unknown_atoms_;
    struct mvhd_t *mvhd_;
    unsigned int tracks_;
    struct trak_t *traks_[MAX_TRACKS];
    struct mvex_t *mvex_;

    int is_indexed_;
  };
  typedef struct moov_t moov_t;
  MOD_STREAMING_DLL_LOCAL extern moov_t *moov_init(void);
  MOD_STREAMING_DLL_LOCAL extern void moov_exit(moov_t *atom);

  struct mvhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint64_t creation_time_;      // seconds since midnite, Jan .1 1904 (UTC)
    uint64_t modification_time_;  // seconds since midnite, Jan .1 1904 (UTC)
    uint32_t timescale_;          // time units that pass in one second
    uint64_t duration_;           // duration of the longest track
    uint32_t rate_;               // preferred playback rate (16.16)
    uint16_t volume_;             // preferred playback volume (8.8)
    uint16_t reserved1_;
    uint32_t reserved2_[2];
    uint32_t matrix_[9];
    uint32_t predefined_[6];
    uint32_t next_track_id_;
  };
  typedef struct mvhd_t mvhd_t;
  MOD_STREAMING_DLL_LOCAL extern mvhd_t *mvhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern mvhd_t *mvhd_copy(mvhd_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void mvhd_exit(mvhd_t *atom);

  struct trak_t {
    struct unknown_atom_t *unknown_atoms_;
    struct tkhd_t *tkhd_;
    struct mdia_t *mdia_;
    struct edts_t *edts_;

    unsigned int chunks_size_;
    struct chunks_t *chunks_;

    unsigned int samples_size_;
    struct samples_t *samples_;

    // current pts when reading fragments
//  uint64_t fragment_pts_;
  };
  typedef struct trak_t trak_t;
  MOD_STREAMING_DLL_LOCAL extern trak_t *trak_init(void);
  MOD_STREAMING_DLL_LOCAL extern unsigned int trak_bitrate(trak_t const *trak);
  MOD_STREAMING_DLL_LOCAL extern void trak_exit(trak_t *trak);

  struct tkhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint64_t creation_time_;      // seconds since midnite, Jan .1 1904 (UTC)
    uint64_t modification_time_;  // seconds since midnite, Jan .1 1904 (UTC)
    uint32_t track_id_;
    uint32_t reserved_;
    uint64_t duration_;           // duration of this track (mvhd.timescale)
    uint32_t reserved2_[2];
    uint16_t layer_;              // front-to-back ordering
    uint16_t predefined_;
    uint16_t volume_;             // relative audio volume (8.8)
    uint16_t reserved3_;
    uint32_t matrix_[9];          // transformation matrix
    uint32_t width_;              // visual presentation width (16.16)
    uint32_t height_;             // visual presentation height (16.16)
  };
  typedef struct tkhd_t tkhd_t;
  MOD_STREAMING_DLL_LOCAL extern tkhd_t *tkhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern tkhd_t *tkhd_copy(tkhd_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void tkhd_exit(tkhd_t *tkhd);

  struct mdia_t {
    struct unknown_atom_t *unknown_atoms_;
    struct mdhd_t *mdhd_;
    struct hdlr_t *hdlr_;
    struct minf_t *minf_;
  };
  typedef struct mdia_t mdia_t;
  MOD_STREAMING_DLL_LOCAL extern mdia_t *mdia_init(void);
  MOD_STREAMING_DLL_LOCAL extern void mdia_exit(mdia_t *atom);

  struct elst_table_t {
    uint64_t segment_duration_;
    int64_t media_time_;
    int16_t media_rate_integer_;
    int16_t media_rate_fraction_;
  };
  typedef struct elst_table_t elst_table_t;

  struct elst_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entry_count_;
    struct elst_table_t *table_;
  };
  typedef struct elst_t elst_t;
  MOD_STREAMING_DLL_LOCAL extern elst_t *elst_init(void);
  MOD_STREAMING_DLL_LOCAL extern void elst_exit(elst_t *atom);

  struct edts_t {
    struct unknown_atom_t *unknown_atoms_;
    struct elst_t *elst_;
  };
  typedef struct edts_t edts_t;
  MOD_STREAMING_DLL_LOCAL extern edts_t *edts_init(void);
  MOD_STREAMING_DLL_LOCAL extern void edts_exit(edts_t *atom);

  struct mdhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint64_t creation_time_;      // seconds since midnite, Jan .1 1904 (UTC)
    uint64_t modification_time_;  // seconds since midnite, Jan .1 1904 (UTC)
    uint32_t timescale_;          // time units that pass in one second
    uint64_t duration_;           // duration of this media
    unsigned int language_[3];    // language code for this media (ISO 639-2/T)
    uint16_t predefined_;
  };
  typedef struct mdhd_t mdhd_t;
  MOD_STREAMING_DLL_LOCAL extern struct mdhd_t *mdhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern mdhd_t *mdhd_copy(mdhd_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void mdhd_exit(struct mdhd_t *mdhd);

  struct hdlr_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t predefined_;
    uint32_t handler_type_;       // format of the contents ('vide', 'soun', ...)
    uint32_t reserved1_;
    uint32_t reserved2_;
    uint32_t reserved3_;
    char *name_;                  // human-readable name for the track type (UTF8)
  };
  typedef struct hdlr_t hdlr_t;
  MOD_STREAMING_DLL_LOCAL extern hdlr_t *hdlr_init(void);
  MOD_STREAMING_DLL_LOCAL extern hdlr_t *hdlr_copy(hdlr_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void hdlr_exit(hdlr_t *atom);

  struct minf_t {
    struct unknown_atom_t *unknown_atoms_;
    struct vmhd_t *vmhd_;
    struct smhd_t *smhd_;
    struct dinf_t *dinf_;
    struct stbl_t *stbl_;
  };
  typedef struct minf_t minf_t;
  MOD_STREAMING_DLL_LOCAL extern minf_t *minf_init(void);
  MOD_STREAMING_DLL_LOCAL extern void minf_exit(minf_t *atom);

  struct vmhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint16_t graphics_mode_;      // composition mode (0=copy)
    uint16_t opcolor_[3];
  };
  typedef struct vmhd_t vmhd_t;
  MOD_STREAMING_DLL_LOCAL extern vmhd_t *vmhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern vmhd_t *vmhd_copy(vmhd_t *rhs);
  MOD_STREAMING_DLL_LOCAL extern void vmhd_exit(vmhd_t *atom);

  struct smhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint16_t balance_;            // place mono audio tracks in stereo space (8.8)
    uint16_t reserved_;
  };
  typedef struct smhd_t smhd_t;
  MOD_STREAMING_DLL_LOCAL extern smhd_t *smhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern smhd_t *smhd_copy(smhd_t *rhs);
  MOD_STREAMING_DLL_LOCAL extern void smhd_exit(smhd_t *atom);

  struct dinf_t {
    struct dref_t *dref_;         // declares the location of the media info
  };
  typedef struct dinf_t dinf_t;
  MOD_STREAMING_DLL_LOCAL extern dinf_t *dinf_init(void);
  MOD_STREAMING_DLL_LOCAL extern dinf_t *dinf_copy(dinf_t *rhs);
  MOD_STREAMING_DLL_LOCAL extern void dinf_exit(dinf_t *atom);

  struct dref_table_t {
    unsigned int flags_;          // 0x000001 is self contained
    char *name_;                  // name is a URN
    char *location_;              // location is a URL
  };
  typedef struct dref_table_t dref_table_t;
  MOD_STREAMING_DLL_LOCAL extern void dref_table_init(dref_table_t *entry);
  MOD_STREAMING_DLL_LOCAL extern void dref_table_assign(dref_table_t *lhs, dref_table_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void dref_table_exit(dref_table_t *entry);

  struct dref_t {
    unsigned int version_;
    unsigned int flags_;
    unsigned int entry_count_;
    dref_table_t *table_;
  };
  typedef struct dref_t dref_t;
  MOD_STREAMING_DLL_LOCAL extern dref_t *dref_init(void);
  MOD_STREAMING_DLL_LOCAL extern dref_t *dref_copy(dref_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void dref_exit(dref_t *atom);

  struct stbl_t {
    struct unknown_atom_t *unknown_atoms_;
    struct stsd_t *stsd_;         // sample description
    struct stts_t *stts_;         // decoding time-to-sample
    struct stss_t *stss_;         // sync sample
    struct stsc_t *stsc_;         // sample-to-chunk
    struct stsz_t *stsz_;         // sample size
    struct stco_t *stco_;         // chunk offset
    struct ctts_t *ctts_;         // composition time-to-sample
  };
  typedef struct stbl_t stbl_t;
  MOD_STREAMING_DLL_LOCAL extern stbl_t *stbl_init(void);
  MOD_STREAMING_DLL_LOCAL extern void stbl_exit(stbl_t *atom);
  MOD_STREAMING_DLL_LOCAL extern
  unsigned int stbl_get_nearest_keyframe(stbl_t const *stbl, unsigned int sample);

  struct stsd_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct sample_entry_t *sample_entries_;
  };
  typedef struct stsd_t stsd_t;
  MOD_STREAMING_DLL_LOCAL extern stsd_t *stsd_init(void);
  MOD_STREAMING_DLL_LOCAL extern stsd_t *stsd_copy(stsd_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern void stsd_exit(stsd_t *atom);

  struct video_sample_entry_t {
    uint16_t version_;
    uint16_t revision_level_;
    uint32_t vendor_;
    uint32_t temporal_quality_;
    uint32_t spatial_quality_;
    uint16_t width_;
    uint16_t height_;
    uint32_t horiz_resolution_;   // pixels per inch (16.16)
    uint32_t vert_resolution_;    // pixels per inch (16.16)
    uint32_t data_size_;
    uint16_t frame_count_;        // number of frames in each sample
    uint8_t compressor_name_[32]; // informative purposes (pascal string)
    uint16_t depth_;              // images are in colour with no alpha (24)
    int16_t color_table_id_;
  };
  typedef struct video_sample_entry_t video_sample_entry_t;
  MOD_STREAMING_DLL_LOCAL extern video_sample_entry_t *video_sample_entry_init(void);

  struct audio_sample_entry_t {
    uint16_t version_;
    uint16_t revision_;
    uint32_t vendor_;
    uint16_t channel_count_;      // mono(1), stereo(2)
    uint16_t sample_size_;        // (bits)
    uint16_t compression_id_;
    uint16_t packet_size_;
    uint32_t samplerate_;         // sampling rate (16.16)
  };
  typedef struct audio_sample_entry_t audio_sample_entry_t;
  MOD_STREAMING_DLL_LOCAL extern audio_sample_entry_t *audio_sample_entry_init(void);

  struct sample_entry_t {
    unsigned int len_;
    uint32_t fourcc_;
    unsigned char *buf_;

    struct video_sample_entry_t *video_;
    struct audio_sample_entry_t *audio_;
//struct hint_sample_entry_t* hint_;

    unsigned int codec_private_data_length_;
    unsigned char const *codec_private_data_;

    // avcC
    unsigned int nal_unit_length_;
    unsigned int sps_length_;
    unsigned char *sps_;
    unsigned int pps_length_;
    unsigned char *pps_;

    // sound (WAVEFORMATEX) structure
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;

    unsigned int samplerate_hi_;
    unsigned int samplerate_lo_;

    // esds
    unsigned int max_bitrate_;
    unsigned int avg_bitrate_;
  };
  typedef struct sample_entry_t sample_entry_t;
  MOD_STREAMING_DLL_LOCAL extern
  void sample_entry_init(sample_entry_t *sample_entry);
  MOD_STREAMING_DLL_LOCAL extern
  void sample_entry_assign(sample_entry_t *lhs, sample_entry_t const *rhs);
  MOD_STREAMING_DLL_LOCAL extern
  void sample_entry_exit(sample_entry_t *sample_entry);
  MOD_STREAMING_DLL_LOCAL extern
  void sample_entry_get_adts(sample_entry_t const *sample_entry,
                             unsigned int sample_size, uint8_t *buf);

  struct stts_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct stts_table_t *table_;
  };
  typedef struct stts_t stts_t;
  MOD_STREAMING_DLL_LOCAL extern stts_t *stts_init(void);
  MOD_STREAMING_DLL_LOCAL extern void stts_exit(stts_t *atom);
  MOD_STREAMING_DLL_LOCAL extern unsigned int stts_get_sample(stts_t const *stts, uint64_t time);
  MOD_STREAMING_DLL_LOCAL extern uint64_t stts_get_time(stts_t const *stts, unsigned int sample);
  MOD_STREAMING_DLL_LOCAL extern uint64_t stts_get_duration(stts_t const *stts);
  MOD_STREAMING_DLL_LOCAL extern unsigned int stts_get_samples(stts_t const *stts);

  struct stts_table_t {
    uint32_t sample_count_;
    uint32_t sample_duration_;
  };
  typedef struct stts_table_t stts_table_t;

  struct stss_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    uint32_t *sample_numbers_;
  };
  typedef struct stss_t stss_t;
  MOD_STREAMING_DLL_LOCAL extern stss_t *stss_init(void);
  MOD_STREAMING_DLL_LOCAL extern void stss_exit(stss_t *atom);
  MOD_STREAMING_DLL_LOCAL extern
  unsigned int stss_get_nearest_keyframe(stss_t const *stss, unsigned int sample);

  struct stsc_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct stsc_table_t *table_;
  };
  typedef struct stsc_t stsc_t;
  MOD_STREAMING_DLL_LOCAL extern stsc_t *stsc_init(void);
  MOD_STREAMING_DLL_LOCAL extern void stsc_exit(stsc_t *atom);

  struct stsc_table_t {
    uint32_t chunk_;
    uint32_t samples_;
    uint32_t id_;
  };
  typedef struct stsc_table_t stsc_table_t;

  struct stsz_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t sample_size_;
    uint32_t entries_;
    uint32_t *sample_sizes_;
  };
  typedef struct stsz_t stsz_t;
  MOD_STREAMING_DLL_LOCAL extern stsz_t *stsz_init(void);
  MOD_STREAMING_DLL_LOCAL extern void stsz_exit(stsz_t *atom);

  struct stco_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    uint64_t *chunk_offsets_;

    void *stco_inplace_;          // newly generated stco (patched inplace)
  };
  typedef struct stco_t stco_t;
  MOD_STREAMING_DLL_LOCAL extern stco_t *stco_init(void);
  MOD_STREAMING_DLL_LOCAL extern void stco_exit(stco_t *atom);

  struct ctts_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct ctts_table_t *table_;
  };
  typedef struct ctts_t ctts_t;
  MOD_STREAMING_DLL_LOCAL extern ctts_t *ctts_init(void);
  MOD_STREAMING_DLL_LOCAL extern void ctts_exit(ctts_t *atom);
  MOD_STREAMING_DLL_LOCAL extern unsigned int ctts_get_samples(ctts_t const *ctts);

  struct ctts_table_t {
    uint32_t sample_count_;
    uint32_t sample_offset_;
  };
  typedef struct ctts_table_t ctts_table_t;

  struct samples_t {
    uint64_t pts_;                // decoding/presentation time
    unsigned int size_;           // size in bytes
    uint64_t pos_;                // byte offset
    unsigned int cto_;            // composition time offset

    unsigned int is_ss_: 1;       // sync sample
    unsigned int is_smooth_ss_: 1; // sync sample for smooth streaming
  };
  typedef struct samples_t samples_t;

  struct chunks_t {
    unsigned int sample_;         // number of the first sample in the chunk
    unsigned int size_;           // number of samples in the chunk
    int id_;                      // not used
    uint64_t pos_;                // start byte position of chunk
  };
  typedef struct chunks_t chunks_t;

  MOD_STREAMING_DLL_LOCAL extern
  uint64_t moov_time_to_trak_time(uint64_t t, uint32_t moov_time_scale,
                                  uint32_t trak_time_scale);
  MOD_STREAMING_DLL_LOCAL extern
  uint64_t trak_time_to_moov_time(uint64_t t, uint32_t moov_time_scale,
                                  uint32_t trak_time_scale);

  struct mvex_t {
    struct unknown_atom_t *unknown_atoms_;
    unsigned int tracks_;
    struct trex_t *trexs_[MAX_TRACKS];
  };
  typedef struct mvex_t mvex_t;
  MOD_STREAMING_DLL_LOCAL extern mvex_t *mvex_init(void);
  MOD_STREAMING_DLL_LOCAL extern void mvex_exit(mvex_t *mvex);

  struct trex_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t track_id_;
    uint32_t default_sample_description_index_;
    uint32_t default_sample_duration_;
    uint32_t default_sample_size_;
    uint32_t default_sample_flags_;
  };
  typedef struct trex_t trex_t;
  MOD_STREAMING_DLL_LOCAL extern trex_t *trex_init(void);
  MOD_STREAMING_DLL_LOCAL extern void trex_exit(trex_t *trex);

  struct moof_t {
    struct unknown_atom_t *unknown_atoms_;
    struct mfhd_t *mfhd_;
    unsigned int tracks_;
    struct traf_t *trafs_[MAX_TRACKS];
  };
  typedef struct moof_t moof_t;
  MOD_STREAMING_DLL_LOCAL extern moof_t *moof_init(void);
  MOD_STREAMING_DLL_LOCAL extern void moof_exit(moof_t *atom);

  struct mfhd_t {
    unsigned int version_;
    unsigned int flags_;
    // the ordinal number of this fragment, in increasing order
    uint32_t sequence_number_;
  };
  typedef struct mfhd_t mfhd_t;
  MOD_STREAMING_DLL_LOCAL extern mfhd_t *mfhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern void mfhd_exit(mfhd_t *atom);

  struct traf_t {
    struct unknown_atom_t *unknown_atoms_;
    struct tfhd_t *tfhd_;
    struct trun_t *trun_;
    struct uuid0_t *uuid0_;
    struct uuid1_t *uuid1_;
  };
  typedef struct traf_t traf_t;
  MOD_STREAMING_DLL_LOCAL extern traf_t *traf_init(void);
  MOD_STREAMING_DLL_LOCAL extern void traf_exit(traf_t *atom);

  struct tfhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t track_id_;
    // all the following are optional fields
    uint64_t base_data_offset_;
    uint32_t sample_description_index_;
    uint32_t default_sample_duration_;
    uint32_t default_sample_size_;
    uint32_t default_sample_flags_;
  };
  typedef struct tfhd_t tfhd_t;
  MOD_STREAMING_DLL_LOCAL extern tfhd_t *tfhd_init(void);
  MOD_STREAMING_DLL_LOCAL extern void tfhd_exit(tfhd_t *atom);

  struct tfra_table_t {
    uint64_t time_;
    uint64_t moof_offset_;
    uint32_t traf_number_;
    uint32_t trun_number_;
    uint32_t sample_number_;
  };
  typedef struct tfra_table_t tfra_table_t;

  struct tfra_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t track_id_;
    unsigned int length_size_of_traf_num_;
    unsigned int length_size_of_trun_num_;
    unsigned int length_size_of_sample_num_;
    uint32_t number_of_entry_;
    struct tfra_table_t *table_;
  };
  typedef struct tfra_t tfra_t;
  MOD_STREAMING_DLL_LOCAL extern tfra_t *tfra_init(void);
  MOD_STREAMING_DLL_LOCAL extern void tfra_exit(tfra_t *tfra);
  MOD_STREAMING_DLL_LOCAL extern void tfra_add(tfra_t *tfra, tfra_table_t const *table);

  struct mfra_t {
    struct unknown_atom_t *unknown_atoms_;
    unsigned int tracks_;
    struct tfra_t *tfras_[MAX_TRACKS];
  };
  typedef struct mfra_t mfra_t;
  MOD_STREAMING_DLL_LOCAL extern mfra_t *mfra_init(void);
  MOD_STREAMING_DLL_LOCAL extern void mfra_exit(mfra_t *atom);

  struct trun_table_t {
    uint32_t sample_duration_;
    uint32_t sample_size_;
    uint32_t sample_flags_;
    uint32_t sample_composition_time_offset_;
  };
  typedef struct trun_table_t trun_table_t;

  struct trun_t {
    unsigned int version_;
    unsigned int flags_;
    // the number of samples being added in this fragment; also the number of rows
    // in the following table (the rows can be empty)
    uint32_t sample_count_;
    // is added to the implicit or explicit data_offset established in the track
    // fragment header
    int32_t data_offset_;
    // provides a set of flags for the first sample only of this run
    uint32_t first_sample_flags_;

    trun_table_t *table_;

    // additional info for uuid
//  trak_t const* trak_;
//  unsigned int start_;
    struct trun_t *next_;
  };
  typedef struct trun_t trun_t;
  MOD_STREAMING_DLL_LOCAL extern struct trun_t *trun_init(void);
  MOD_STREAMING_DLL_LOCAL extern void trun_exit(struct trun_t *atom);

  struct uuid0_t {
    uint64_t pts_;
    uint64_t duration_;
  };
  typedef struct uuid0_t uuid0_t;
  MOD_STREAMING_DLL_LOCAL extern uuid0_t *uuid0_init(void);
  MOD_STREAMING_DLL_LOCAL extern void uuid0_exit(uuid0_t *atom);

  struct uuid1_t {
    unsigned int entries_;
    uint64_t pts_[2];
    uint64_t duration_[2];
  };
  typedef struct uuid1_t uuid1_t;
  MOD_STREAMING_DLL_LOCAL extern uuid1_t *uuid1_init(void);
  MOD_STREAMING_DLL_LOCAL extern void uuid1_exit(uuid1_t *atom);

// random access structure similar to mfra, but with size field
  struct rxs_t {
    uint64_t time_;
    uint64_t offset_;
    uint64_t size_;
  };
  typedef struct rxs_t rxs_t;

#define MP4_ELEMENTARY_STREAM_DESCRIPTOR_TAG   3
#define MP4_DECODER_CONFIG_DESCRIPTOR_TAG      4
#define MP4_DECODER_SPECIFIC_DESCRIPTOR_TAG    5

#define MP4_MPEG4Audio                      0x40
#define MP4_MPEG2AudioMain                  0x66
#define MP4_MPEG2AudioLowComplexity         0x67
#define MP4_MPEG2AudioScaleableSamplingRate 0x68
#define MP4_MPEG2AudioPart3                 0x69
#define MP4_MPEG1Audio                      0x6b

  struct mp4_context_t {
//  char* filename_;
    ngx_http_request_t *r;
    ngx_file_t *file;

    // the atoms as found in the stream
    mp4_atom_t ftyp_atom;
    mp4_atom_t mdat_atom;
    mp4_atom_t moov_atom;

    // the actual binary data
    unsigned char *moov_data;
    // the parsed atoms
    moov_t *moov;

    size_t root;
  };
  typedef struct mp4_context_t mp4_context_t;

  enum mp4_open_flags {
    MP4_OPEN_MOOV = 0x00000001,
    MP4_OPEN_MOOF = 0x00000002,
    MP4_OPEN_MDAT = 0x00000004,
    MP4_OPEN_MFRA = 0x00000008,
    MP4_OPEN_ALL  = 0x0000000f
  };
  typedef enum mp4_open_flags mp4_open_flags;

  MOD_STREAMING_DLL_LOCAL extern
  mp4_context_t *mp4_open(ngx_http_request_t *r, ngx_file_t *file, int64_t filesize,
                          mp4_open_flags flags);

  MOD_STREAMING_DLL_LOCAL extern void mp4_close(mp4_context_t *mp4_context);

  MOD_STREAMING_DLL_LOCAL extern unsigned char *read_box(mp4_context_t *mp4_context, struct mp4_atom_t *atom);

#ifdef __cplusplus
} /* extern C definitions */
#endif

#endif // MP4_IO_H_AKW

// End Of File

