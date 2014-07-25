/*******************************************************************************
 mp4_io.c - A library for general MPEG4 I/O.

 For licensing see the LICENSE file
******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
#define __STDC_FORMAT_MACROS // C++ should define this for PRIu64
#define __STDC_LIMIT_MACROS  // C++ should define this for UINT32_MAX
#endif

#include "mp4_io.h"
#include "mp4_reader.h" // for moov_read
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>  // FreeBSD doesn't define off_t in stdio.h
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef WIN32
#include <io.h>
#include <windows.h>
#define DIR_SEPARATOR '\\'
#define strdup _strdup
#define open _open
#define close _close
#define write _write
#define lseek _lseeki64
#define stat _stat64
#else
#define DIR_SEPARATOR '/'
#include <unistd.h>
#include <sys/mman.h>
#endif

extern uint64_t atoi64(const char *val) {
#ifdef WIN32
  return _atoi64(val);
#else // elif defined(HAVE_STRTOLL)
  return strtoll(val, NULL, 10);
#endif
}

extern const char *remove_path(const char *path) {
  const char *p = strrchr(path, DIR_SEPARATOR);
  if(p != NULL && *p != '\0') {
    return p + 1;
  }

  return path;
}

extern void mp4_log_trace(const mp4_context_t *mp4_context, ngx_uint_t level, const char *fmt, ...) {
  va_list arglist;
  va_start(arglist, fmt);

  char out[255];

  vsprintf(out, fmt, arglist);

  ngx_log_debug0(level, mp4_context->r->connection->log, 0, out);

  va_end(arglist);
}

static int64_t seconds_since_1970(void) {
// #ifdef WIN32
  return time(NULL);
// #else
//   struct timeval tv;
//   gettimeofday(&tv, NULL);
//   return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
// #endif
}

static int64_t seconds_since_1904(void) {
  return seconds_since_1970() + 2082844800;
}

extern unsigned int read_8(unsigned char const *buffer) {
  return buffer[0];
}

extern unsigned char *write_8(unsigned char *buffer, unsigned int v) {
  buffer[0] = (uint8_t)v;

  return buffer + 1;
}

extern uint16_t read_16(unsigned char const *buffer) {
  return (buffer[0] << 8) |
         (buffer[1] << 0);
}

extern unsigned char *write_16(unsigned char *buffer, unsigned int v) {
  buffer[0] = (uint8_t)(v >> 8);
  buffer[1] = (uint8_t)(v >> 0);

  return buffer + 2;
}

extern unsigned int read_24(unsigned char const *buffer) {
  return (buffer[0] << 16) |
         (buffer[1] << 8) |
         (buffer[2] << 0);
}

extern unsigned char *write_24(unsigned char *buffer, unsigned int v) {
  buffer[0] = (uint8_t)(v >> 16);
  buffer[1] = (uint8_t)(v >> 8);
  buffer[2] = (uint8_t)(v >> 0);

  return buffer + 3;
}

extern uint32_t read_32(unsigned char const *buffer) {
  return (buffer[0] << 24) |
         (buffer[1] << 16) |
         (buffer[2] << 8) |
         (buffer[3] << 0);
}

extern unsigned char *write_32(unsigned char *buffer, uint32_t v) {
  buffer[0] = (uint8_t)(v >> 24);
  buffer[1] = (uint8_t)(v >> 16);
  buffer[2] = (uint8_t)(v >> 8);
  buffer[3] = (uint8_t)(v >> 0);

  return buffer + 4;
}

extern uint64_t read_64(unsigned char const *buffer) {
  return ((uint64_t)(read_32(buffer)) << 32) + read_32(buffer + 4);
}

extern unsigned char *write_64(unsigned char *buffer, uint64_t v) {
  write_32(buffer + 0, (uint32_t)(v >> 32));
  write_32(buffer + 4, (uint32_t)(v >> 0));

  return buffer + 8;
}

extern uint32_t read_n(unsigned char const *buffer, unsigned int n) {
  switch(n) {
  case 8:
    return read_8(buffer);
  case 16:
    return read_16(buffer);
  case 24:
    return read_24(buffer);
  case 32:
    return read_32(buffer);
  default:
    // program error
    return 0;
  }
}

extern unsigned char *write_n(unsigned char *buffer, unsigned int n, uint32_t v) {
  switch(n) {
  case 8:
    return write_8(buffer, v);
  case 16:
    return write_16(buffer, v);
  case 24:
    return write_24(buffer, v);
  case 32:
    return write_32(buffer, v);
  }
  return NULL;
}

static unsigned int alignment() {
#ifdef _WIN32
  SYSTEM_INFO SysInfo;
  GetSystemInfo(&SysInfo);
  return (unsigned int)(SysInfo.dwAllocationGranularity);
#else
  return (unsigned int)(getpagesize());
#endif
}

static mem_range_t *mem_range_init(char const *filename, int read_only,
                                   uint64_t filesize,
                                   uint64_t offset, uint64_t len) {
  mem_range_t *mem_range = (mem_range_t *)malloc(sizeof(mem_range_t));
  mem_range->read_only_ = read_only;
  mem_range->filesize_ = filesize;
  mem_range->fd_ = -1;
  mem_range->mmap_addr_ = 0;
  mem_range->mmap_offset_ = 0;
  mem_range->mmap_size_ = 0;
#ifdef WIN32
  mem_range->fileMapHandle_ = NULL;
#endif

  mem_range->fd_ = open(filename, read_only ? O_RDONLY : (O_RDWR | O_CREAT),
#ifdef WIN32
                        S_IREAD | S_IWRITE
#else
                        0666
#endif
                       );
  if(mem_range->fd_ == -1) {
    printf("mem_range: Error opening file %s\n", filename);
    mem_range_exit(mem_range);
    return 0;
  }

  if(!read_only) {
    // shrink the file (if necessary)
    if(offset + len < filesize) {
      int result;
#ifdef WIN32
      lseek(mem_range->fd_, offset + len, SEEK_SET);
      result =
        SetEndOfFile((HANDLE)_get_osfhandle(mem_range->fd_)) == 0 ? -1 : 0;
#else
      result = truncate(filename, offset + len);
#endif
      if(result < 0) {
        printf("mem_range: Error shrinking file %s\n", filename);
        mem_range_exit(mem_range);
        return 0;
      }
    }
    // stretch the file (if necessary)
    else if(offset + len > filesize) {
      lseek(mem_range->fd_, offset + len - 1, SEEK_SET);
      if(write(mem_range->fd_, "", 1) < 0) {
        printf("mem_range: Error stretching file %s\n", filename);
        mem_range_exit(mem_range);
        return 0;
      }
    }
    mem_range->filesize_ = offset + len;
  }

#ifdef _WIN32
  {
    HANDLE hFile = (HANDLE)_get_osfhandle(mem_range->fd_);

    if(!hFile) {
      printf("%s", "Cannot create file mapping\n");
      mem_range_exit(mem_range);
      return 0;
    }

    mem_range->fileMapHandle_ = CreateFileMapping(hFile, 0,
                                read_only ? PAGE_READONLY : PAGE_READWRITE, 0, 0, NULL);

    if(!mem_range->fileMapHandle_) {
      printf("%s", "Cannot create file mapping view\n");
      mem_range_exit(mem_range);
      return 0;
    }
  }
#endif

  return mem_range;
}

mem_range_t *mem_range_init_read(char const *filename) {
  int read_only = 1;
  uint64_t offset = 0;
  struct stat status;
  // make sure regular file exists and its not empty (can't mmap 0 bytes)
  if(stat(filename, &status) ||
      (status.st_mode & S_IFMT) != S_IFREG ||
      status.st_size == 0) {
    return 0;
  }

  return mem_range_init(filename, read_only, status.st_size,
                        offset, status.st_size);
}

mem_range_t *mem_range_init_write(char const *filename,
                                  uint64_t offset, uint64_t len) {
  int read_only = 0;
  uint64_t filesize = 0;
  struct stat status;
  if(!stat(filename, &status)) {
    filesize = status.st_size;
  }

  return mem_range_init(filename, read_only, filesize, offset, len);
}

void *mem_range_map(mem_range_t *mem_range, uint64_t offset, uint32_t len) {
  // only map when necessary
  if(offset < mem_range->mmap_offset_ ||
      offset + len >= mem_range->mmap_offset_ + mem_range->mmap_size_) {
    // use 1MB of overlap, so a little random access is okay at the end of the
    // memory mapped file.
    const unsigned int overlap = 1024 * 1024;
    const unsigned int window_size = 16 * 1024 * 1024;

    unsigned int dwSysGran = alignment();
    uint64_t mmap_offset = offset > overlap ? (offset - overlap) : 0;
    len += offset > overlap ? overlap : (uint32_t)offset;
//    uint64_t mmap_offset = offset;
    mem_range->mmap_offset_ = (mmap_offset / dwSysGran) * dwSysGran;
    mem_range->mmap_size_ = (mmap_offset % dwSysGran) + len;

    if(mem_range->mmap_offset_ + mem_range->mmap_size_ > mem_range->filesize_) {
      printf("%s", "mem_range_map: invalid range for file mapping\n");
      return 0;
    }

    if(mem_range->mmap_size_ < window_size) {
      mem_range->mmap_size_ = window_size;
    }

    if(mem_range->mmap_offset_ + mem_range->mmap_size_ > mem_range->filesize_) {
      mem_range->mmap_size_ = mem_range->filesize_ - mem_range->mmap_offset_;
    }

//    printf("mem_range(%x): offset=%"PRIu64"\n", mem_range, offset);

#ifdef WIN32
    if(mem_range->mmap_addr_) {
      UnmapViewOfFile(mem_range->mmap_addr_);
    }

    mem_range->mmap_addr_ = MapViewOfFile(mem_range->fileMapHandle_,
                                          mem_range->read_only_ ? FILE_MAP_READ : FILE_MAP_WRITE,
                                          mem_range->mmap_offset_ >> 32, (uint32_t)(mem_range->mmap_offset_),
                                          (size_t)(mem_range->mmap_size_));

    if(!mem_range->mmap_addr_) {
      printf("%s", "Unable to make file mapping\n");
      return 0;
    }
#else
    if(mem_range->mmap_addr_) {
      munmap(mem_range->mmap_addr_, mem_range->mmap_size_);
    }

    mem_range->mmap_addr_ = mmap(0, mem_range->mmap_size_, mem_range->read_only_ ? PROT_READ : (PROT_READ | PROT_WRITE), mem_range->read_only_ ? MAP_PRIVATE : MAP_SHARED, mem_range->fd_, mem_range->mmap_offset_);

    if(mem_range->mmap_addr_ == MAP_FAILED) {
      printf("%s", "Unable to make file mapping\n");
      return 0;
    }

    if(mem_range->read_only_ &&
        madvise(mem_range->mmap_addr_, mem_range->mmap_size_, MADV_SEQUENTIAL) < 0) {
      printf("%s", "Unable to advise file mapping\n");
      // continue
    }
#endif
  }

  return (char *)mem_range->mmap_addr_ + (offset - mem_range->mmap_offset_);
}

void mem_range_exit(mem_range_t *mem_range) {
  if(!mem_range) {
    return;
  }

#ifdef WIN32
  CloseHandle(mem_range->fileMapHandle_);
#endif

  if(mem_range->mmap_addr_) {
#ifdef WIN32
    UnmapViewOfFile(mem_range->mmap_addr_);
#else
    munmap(mem_range->mmap_addr_, mem_range->mmap_size_);
#endif
  }

  if(mem_range->fd_ != -1) {
    close(mem_range->fd_);
  }

  free(mem_range);
}
int mp4_atom_read_header(mp4_context_t *mp4_context, mp4_atom_t *atom) {
  unsigned char atom_header[8];

  atom->start_ = mp4_context->file->offset;
  if(ngx_read_file(mp4_context->file, atom_header, 8, (off_t)atom->start_) == NGX_ERROR) {
    MP4_ERROR("%s", "Error reading atom header\n");
    return 0;
  }
  mp4_context->file->offset = atom->start_ + 8;
  atom->short_size_ = read_32(&atom_header[0]);
  atom->type_ = read_32(&atom_header[4]);

  if(atom->short_size_ == 1) {
    if(ngx_read_file(mp4_context->file, atom_header, 8, (off_t)mp4_context->file->offset) == NGX_ERROR) {
      MP4_ERROR("%s", "Error reading extended atom header\n");
      return 0;
    }
    mp4_context->file->offset += 8;
    atom->size_ = read_64(&atom_header[0]);
  } else {
    atom->size_ = atom->short_size_;
  }

  atom->end_ = atom->start_ + atom->size_;

  MP4_INFO("Atom(%c%c%c%c,%"PRIu64")\n",
           atom->type_ >> 24, atom->type_ >> 16,
           atom->type_ >> 8, atom->type_,
           atom->size_);

  if(atom->size_ < ATOM_PREAMBLE_SIZE) {
    MP4_ERROR("%s", "Error: invalid atom size\n");
    return 0;
  }

  return 1;
}

extern int mp4_atom_write_header(unsigned char *outbuffer,
                                 mp4_atom_t const *atom) {
  int write_box64 = atom->short_size_ == 1 ? 1 : 0;

  if(write_box64) write_32(outbuffer, 1);
  else write_32(outbuffer, (uint32_t)atom->size_);

  write_32(outbuffer + 4, atom->type_);

  if(write_box64) {
    write_64(outbuffer + 8, atom->size_);
    return 16;
  } else return 8;
}

extern unsigned char *read_box(mp4_context_t *mp4_context, struct mp4_atom_t *atom) {
  if(atom->size_ > 1024 * 1024 * 10) return 0;

  unsigned char *box_data = (unsigned char *)ngx_pcalloc(mp4_context->r->pool, (size_t)atom->size_);
  ssize_t n = ngx_read_file(mp4_context->file, box_data, atom->size_, (off_t)atom->start_);
  mp4_context->file->offset = atom->start_ + n;

  if(n == NGX_ERROR) {
    MP4_ERROR("Error reading %c%c%c%c atom\n",
              atom->type_ >> 24, atom->type_ >> 16,
              atom->type_ >> 8, atom->type_);
    free(box_data);
    return 0;
  }

  return box_data;
}

static mp4_context_t *mp4_context_init(ngx_http_request_t *r, ngx_file_t *file) {
  mp4_context_t *mp4_context = (mp4_context_t *)ngx_pcalloc(r->pool, sizeof(mp4_context_t));

  mp4_context->r = r;
  mp4_context->file = file;

  memset(&mp4_context->ftyp_atom, 0, sizeof(struct mp4_atom_t));
  memset(&mp4_context->moov_atom, 0, sizeof(struct mp4_atom_t));
  memset(&mp4_context->mdat_atom, 0, sizeof(struct mp4_atom_t));

  mp4_context->moov_data = 0;

  mp4_context->moov = 0;

  return mp4_context;
}

static void mp4_context_exit(struct mp4_context_t *mp4_context) {
  if(mp4_context->moov_data) ngx_pfree(mp4_context->r->pool, mp4_context->moov_data);
  if(mp4_context->moov) moov_exit(mp4_context->moov);
  ngx_pfree(mp4_context->r->pool, mp4_context);
}

extern mp4_context_t *mp4_open(ngx_http_request_t *r, ngx_file_t *file, int64_t filesize, mp4_open_flags flags) {
  mp4_context_t *mp4_context = mp4_context_init(r, file);

  while(!mp4_context->moov_atom.size_ || !mp4_context->mdat_atom.size_) {
    struct mp4_atom_t leaf_atom;

    if(!mp4_atom_read_header(mp4_context, &leaf_atom))
      break;

    switch(leaf_atom.type_) {
    case FOURCC('f', 't', 'y', 'p'):
      mp4_context->ftyp_atom = leaf_atom;
      break;
    case FOURCC('m', 'o', 'o', 'v'):
      mp4_context->moov_atom = leaf_atom;
      mp4_context->moov_data = read_box(mp4_context, &mp4_context->moov_atom);
      if(mp4_context->moov_data == NULL) {
        MP4_ERROR("%s", "No moov data\n");
        mp4_context_exit(mp4_context);
        return 0;
      }

      mp4_context->moov = (moov_t *)
                          moov_read(mp4_context, NULL,
                                    mp4_context->moov_data + ATOM_PREAMBLE_SIZE,
                                    mp4_context->moov_atom.size_ - ATOM_PREAMBLE_SIZE);

      if(mp4_context->moov == 0 || mp4_context->moov->mvhd_ == 0) {
        MP4_ERROR("%s", "Error parsing moov header\n");
        mp4_context_exit(mp4_context);
        return 0;
      }
      break;
    case FOURCC('m', 'd', 'a', 't'):
      mp4_context->mdat_atom = leaf_atom;
      break;
    }

    if(leaf_atom.end_ > (uint64_t)filesize) {
      MP4_ERROR("%s", "Reached end of file prematurely\n");
      mp4_context_exit(mp4_context);
      return 0;
    }

    file->offset = leaf_atom.end_;
  }

  return mp4_context;
}

extern void mp4_close(struct mp4_context_t *mp4_context) {
  mp4_context_exit(mp4_context);
}

////////////////////////////////////////////////////////////////////////////////

extern struct unknown_atom_t *unknown_atom_init() {
  unknown_atom_t *atom = (unknown_atom_t *)malloc(sizeof(unknown_atom_t));
  atom->atom_ = 0;
  atom->next_ = 0;

  return atom;
}

extern void unknown_atom_exit(unknown_atom_t *atom) {
  while(atom) {
    unknown_atom_t *next = atom->next_;
    free(atom->atom_);
    free(atom);
    atom = next;
  }
}


extern moov_t *moov_init() {
  moov_t *moov = (moov_t *)malloc(sizeof(moov_t));
  moov->unknown_atoms_ = 0;
  moov->mvhd_ = 0;
  moov->tracks_ = 0;
  moov->mvex_ = 0;

  moov->is_indexed_ = 0;

  return moov;
}

extern void moov_exit(moov_t *atom) {
  unsigned int i;
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->mvhd_) {
    mvhd_exit(atom->mvhd_);
  }
  for(i = 0; i != atom->tracks_; ++i) {
    trak_exit(atom->traks_[i]);
  }
  if(atom->mvex_) {
    mvex_exit(atom->mvex_);
  }
  free(atom);
}

#if 0
extern void moov_shift_offsets(moov_t *moov, int64_t offset) {
  unsigned int i;
  for(i = 0; i != moov->tracks_; ++i) {
    trak_shift_offsets(moov->traks_[i], offset);
  }
}
#endif

extern trak_t *trak_init() {
  trak_t *trak = (trak_t *)malloc(sizeof(trak_t));
  trak->unknown_atoms_ = 0;
  trak->tkhd_ = 0;
  trak->mdia_ = 0;
  trak->edts_ = 0;
  trak->chunks_size_ = 0;
  trak->chunks_ = 0;
  trak->samples_size_ = 0;
  trak->samples_ = 0;

//  trak->fragment_pts_ = 0;

  return trak;
}

extern unsigned int trak_bitrate(trak_t const *trak) {
  uint32_t trak_time_scale = trak->mdia_->mdhd_->timescale_;
  uint64_t duration;
  unsigned int bps;

  samples_t const *first = trak->samples_;
  samples_t const *last = trak->samples_ + trak->samples_size_;
  uint64_t sample_size = 0;
  while(first != last) {
    sample_size += first->size_;
    ++first;
  }
  duration = first->pts_;

  bps = (unsigned int)(sample_size * trak_time_scale / duration * 8);

  return bps;
}

#if 0
extern void trak_shift_offsets(trak_t *trak, int64_t offset) {
  stco_t *stco = trak->mdia_->minf_->stbl_->stco_;
  stco_shift_offsets(stco, (int32_t)offset);
}
#endif

extern void trak_exit(trak_t *trak) {
  if(trak->unknown_atoms_) {
    unknown_atom_exit(trak->unknown_atoms_);
  }
  if(trak->tkhd_) {
    tkhd_exit(trak->tkhd_);
  }
  if(trak->mdia_) {
    mdia_exit(trak->mdia_);
  }
  if(trak->edts_) {
    edts_exit(trak->edts_);
  }
  if(trak->chunks_) {
    free(trak->chunks_);
  }
  if(trak->samples_) {
    free(trak->samples_);
  }
  free(trak);
}

extern mvhd_t *mvhd_init() {
  unsigned int i;
  mvhd_t *atom = (mvhd_t *)malloc(sizeof(mvhd_t));

  atom->version_ = 1;
  atom->flags_ = 0;
  atom->creation_time_ =
    atom->modification_time_ = seconds_since_1904();
  atom->timescale_ = 10000000;
  atom->duration_ = 0;
  atom->rate_ = (1 << 16);
  atom->volume_ = (1 << 8);
  atom->reserved1_ = 0;
  for(i = 0; i != 2; ++i) {
    atom->reserved2_[i] = 0;
  }
  for(i = 0; i != 9; ++i) {
    atom->matrix_[i] = 0;
  }
  atom->matrix_[0] = 0x00010000;
  atom->matrix_[4] = 0x00010000;
  atom->matrix_[8] = 0x40000000;
  for(i = 0; i != 6; ++i) {
    atom->predefined_[i] = 0;
  }
  atom->next_track_id_ = 1;

  return atom;
}

extern mvhd_t *mvhd_copy(mvhd_t const *rhs) {
  mvhd_t *atom = (mvhd_t *)malloc(sizeof(mvhd_t));

  memcpy(atom, rhs, sizeof(mvhd_t));

  return atom;
}

extern void mvhd_exit(mvhd_t *atom) {
  free(atom);
}

extern tkhd_t *tkhd_init() {
  unsigned int i;
  tkhd_t *tkhd = (tkhd_t *)malloc(sizeof(tkhd_t));

  tkhd->version_ = 1;
  tkhd->flags_ = 7;           // track_enabled, track_in_movie, track_in_preview
  tkhd->creation_time_ =
    tkhd->modification_time_ = seconds_since_1904();
  tkhd->track_id_ = 0;
  tkhd->reserved_ = 0;
  tkhd->duration_ = 0;
  for(i = 0; i != 2; ++i) {
    tkhd->reserved2_[i] = 0;
  }
  tkhd->layer_ = 0;
  tkhd->predefined_ = 0;
  tkhd->volume_ = (1 << 8) + 0;
  tkhd->reserved3_ = 0;
  for(i = 0; i != 9; ++i) {
    tkhd->matrix_[i] = 0;
  }
  tkhd->matrix_[0] = 0x00010000;
  tkhd->matrix_[4] = 0x00010000;
  tkhd->matrix_[8] = 0x40000000;
  tkhd->width_ = 0;
  tkhd->height_ = 0;

  return tkhd;
}

extern struct tkhd_t *tkhd_copy(tkhd_t const *rhs) {
  tkhd_t *tkhd = (tkhd_t *)malloc(sizeof(tkhd_t));

  memcpy(tkhd, rhs, sizeof(tkhd_t));

  return tkhd;
}

extern void tkhd_exit(tkhd_t *tkhd) {
  free(tkhd);
}

extern struct mdia_t *mdia_init() {
  mdia_t *atom = (mdia_t *)malloc(sizeof(mdia_t));
  atom->unknown_atoms_ = 0;
  atom->mdhd_ = 0;
  atom->hdlr_ = 0;
  atom->minf_ = 0;

  return atom;
}

extern void mdia_exit(mdia_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->mdhd_) {
    mdhd_exit(atom->mdhd_);
  }
  if(atom->hdlr_) {
    hdlr_exit(atom->hdlr_);
  }
  if(atom->minf_) {
    minf_exit(atom->minf_);
  }
  free(atom);
}

extern elst_t *elst_init() {
  elst_t *elst = (elst_t *)malloc(sizeof(elst_t));

  elst->version_ = 1;
  elst->flags_ = 0;
  elst->entry_count_ = 0;
  elst->table_ = 0;

  return elst;
}

extern void elst_exit(elst_t *elst) {
  if(elst->table_) {
    free(elst->table_);
  }
  free(elst);
}

extern edts_t *edts_init() {
  edts_t *edts = (edts_t *)malloc(sizeof(edts_t));

  edts->unknown_atoms_ = 0;
  edts->elst_ = 0;

  return edts;
}

extern void edts_exit(edts_t *edts) {
  if(edts->unknown_atoms_) {
    unknown_atom_exit(edts->unknown_atoms_);
  }
  if(edts->elst_) {
    elst_exit(edts->elst_);
  }
  free(edts);
}

extern mdhd_t *mdhd_init() {
  unsigned int i;
  mdhd_t *mdhd = (mdhd_t *)malloc(sizeof(mdhd_t));

  mdhd->version_ = 1;
  mdhd->flags_ = 0;
  mdhd->creation_time_ =
    mdhd->modification_time_ = seconds_since_1904();
  mdhd->timescale_ = 10000000;
  mdhd->duration_ = 0;
  for(i = 0; i != 3; ++i) {
    mdhd->language_[i] = 0x7f;
  }
  mdhd->predefined_ = 0;

  return mdhd;
}

extern mdhd_t *mdhd_copy(mdhd_t const *rhs) {
  struct mdhd_t *mdhd = (struct mdhd_t *)malloc(sizeof(struct mdhd_t));

  memcpy(mdhd, rhs, sizeof(mdhd_t));

  return mdhd;
}

extern void mdhd_exit(struct mdhd_t *mdhd) {
  free(mdhd);
}

extern hdlr_t *hdlr_init() {
  hdlr_t *atom = (hdlr_t *)malloc(sizeof(hdlr_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->predefined_ = 0;
  atom->handler_type_ = 0;
  atom->reserved1_ = 0;
  atom->reserved2_ = 0;
  atom->reserved3_ = 0;
  atom->name_ = 0;

  return atom;
}

extern hdlr_t *hdlr_copy(hdlr_t const *rhs) {
  hdlr_t *atom = (hdlr_t *)malloc(sizeof(hdlr_t));

  atom->version_ = rhs->version_;
  atom->flags_ = rhs->flags_;
  atom->predefined_ = rhs->predefined_;
  atom->handler_type_ = rhs->handler_type_;
  atom->reserved1_ = rhs->reserved1_;
  atom->reserved2_ = rhs->reserved2_;
  atom->reserved3_ = rhs->reserved3_;
  atom->name_ = rhs->name_ == NULL ? NULL : strdup(rhs->name_);

  return atom;
}

extern void hdlr_exit(struct hdlr_t *atom) {
  if(atom->name_) {
    free(atom->name_);
  }
  free(atom);
}

extern struct minf_t *minf_init() {
  struct minf_t *atom = (struct minf_t *)malloc(sizeof(struct minf_t));
  atom->unknown_atoms_ = 0;
  atom->vmhd_ = 0;
  atom->smhd_ = 0;
  atom->dinf_ = 0;
  atom->stbl_ = 0;

  return atom;
}

extern void minf_exit(struct minf_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->vmhd_) {
    vmhd_exit(atom->vmhd_);
  }
  if(atom->smhd_) {
    smhd_exit(atom->smhd_);
  }
  if(atom->dinf_) {
    dinf_exit(atom->dinf_);
  }
  if(atom->stbl_) {
    stbl_exit(atom->stbl_);
  }
  free(atom);
}

extern vmhd_t *vmhd_init() {
  unsigned int i;
  vmhd_t *atom = (vmhd_t *)malloc(sizeof(vmhd_t));

  atom->version_ = 0;
  atom->flags_ = 1;
  atom->graphics_mode_ = 0;
  for(i = 0; i != 3; ++i) {
    atom->opcolor_[i] = 0;
  }

  return atom;
}

extern vmhd_t *vmhd_copy(vmhd_t *rhs) {
  vmhd_t *atom = (vmhd_t *)malloc(sizeof(vmhd_t));

  memcpy(atom, rhs, sizeof(vmhd_t));

  return atom;
}

extern void vmhd_exit(struct vmhd_t *atom) {
  free(atom);
}

extern smhd_t *smhd_init() {
  smhd_t *atom = (smhd_t *)malloc(sizeof(smhd_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->balance_ = 0;
  atom->reserved_ = 0;

  return atom;
}

extern smhd_t *smhd_copy(smhd_t *rhs) {
  smhd_t *atom = (smhd_t *)malloc(sizeof(smhd_t));

  memcpy(atom, rhs, sizeof(smhd_t));

  return atom;
}

extern void smhd_exit(struct smhd_t *atom) {
  free(atom);
}

extern dinf_t *dinf_init() {
  dinf_t *atom = (dinf_t *)malloc(sizeof(dinf_t));

  atom->dref_ = 0;

  return atom;
}

extern dinf_t *dinf_copy(dinf_t *rhs) {
  dinf_t *atom = (dinf_t *)malloc(sizeof(dinf_t));

  atom->dref_ = dref_copy(rhs->dref_);

  return atom;
}

extern void dinf_exit(dinf_t *atom) {
  if(atom->dref_) {
    dref_exit(atom->dref_);
  }
  free(atom);
}

extern dref_t *dref_init() {
  dref_t *atom = (dref_t *)malloc(sizeof(dref_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entry_count_ = 0;
  atom->table_ = 0;

  return atom;
}

extern dref_t *dref_copy(dref_t const *rhs) {
  unsigned int i;
  dref_t *atom = (dref_t *)malloc(sizeof(dref_t));

  atom->version_ = rhs->version_;
  atom->flags_ = rhs->flags_;
  atom->entry_count_ = rhs->entry_count_;
  atom->table_ = atom->entry_count_ == 0 ? NULL : (dref_table_t *)malloc(atom->entry_count_ * sizeof(dref_table_t));
  for(i = 0; i != atom->entry_count_; ++i) {
    dref_table_assign(&atom->table_[i], &rhs->table_[i]);
  }

  return atom;
}

extern void dref_exit(dref_t *atom) {
  unsigned int i;
  for(i = 0; i != atom->entry_count_; ++i) {
    dref_table_exit(&atom->table_[i]);
  }
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

extern void dref_table_init(dref_table_t *entry) {
  entry->flags_ = 0;
  entry->name_ = 0;
  entry->location_ = 0;
}

extern void dref_table_assign(dref_table_t *lhs, dref_table_t const *rhs) {
  lhs->flags_ = rhs->flags_;
  lhs->name_ = rhs->name_ == NULL ? NULL : strdup(rhs->name_);
  lhs->location_ = rhs->location_ == NULL ? NULL : strdup(rhs->location_);
}

extern void dref_table_exit(dref_table_t *entry) {
  if(entry->name_) {
    free(entry->name_);
  }
  if(entry->location_) {
    free(entry->location_);
  }
}

extern struct stbl_t *stbl_init() {
  struct stbl_t *atom = (struct stbl_t *)malloc(sizeof(struct stbl_t));
  atom->unknown_atoms_ = 0;
  atom->stsd_ = 0;
  atom->stts_ = 0;
  atom->stss_ = 0;
  atom->stsc_ = 0;
  atom->stsz_ = 0;
  atom->stco_ = 0;
  atom->ctts_ = 0;

  return atom;
}

extern void stbl_exit(struct stbl_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->stsd_) {
    stsd_exit(atom->stsd_);
  }
  if(atom->stts_) {
    stts_exit(atom->stts_);
  }
  if(atom->stss_) {
    stss_exit(atom->stss_);
  }
  if(atom->stsc_) {
    stsc_exit(atom->stsc_);
  }
  if(atom->stsz_) {
    stsz_exit(atom->stsz_);
  }
  if(atom->stco_) {
    stco_exit(atom->stco_);
  }
  if(atom->ctts_) {
    ctts_exit(atom->ctts_);
  }

  free(atom);
}

extern unsigned int stbl_get_nearest_keyframe(struct stbl_t const *stbl,
    unsigned int sample) {
  // If the sync atom is not present, all samples are implicit sync samples.
  if(!stbl->stss_)
    return sample;

  return stss_get_nearest_keyframe(stbl->stss_, sample);
}

extern stsd_t *stsd_init() {
  stsd_t *atom = (stsd_t *)malloc(sizeof(stsd_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->sample_entries_ = 0;

  return atom;
}

extern stsd_t *stsd_copy(stsd_t const *rhs) {
  unsigned int i;
  struct stsd_t *atom = (struct stsd_t *)malloc(sizeof(struct stsd_t));

  atom->version_ = rhs->version_;
  atom->flags_ = rhs->flags_;
  atom->entries_ = rhs->entries_;
  atom->sample_entries_ =
    (sample_entry_t *)malloc(atom->entries_ * sizeof(sample_entry_t));
  for(i = 0; i != atom->entries_; ++i) {
    sample_entry_assign(&atom->sample_entries_[i], &rhs->sample_entries_[i]);
  }

  return atom;
}

extern void stsd_exit(struct stsd_t *atom) {
  unsigned int i;
  for(i = 0; i != atom->entries_; ++i) {
    sample_entry_t *sample_entry = &atom->sample_entries_[i];
    sample_entry_exit(sample_entry);
  }
  if(atom->sample_entries_) {
    free(atom->sample_entries_);
  }
  free(atom);
}

extern video_sample_entry_t *video_sample_entry_init() {
  video_sample_entry_t *video_sample_entry =
    (video_sample_entry_t *)malloc(sizeof(video_sample_entry_t));

  video_sample_entry->version_ = 0;
  video_sample_entry->revision_level_ = 0;
  video_sample_entry->vendor_ = 0;
  video_sample_entry->temporal_quality_ = 0;
  video_sample_entry->spatial_quality_ = 0;
  video_sample_entry->width_ = 0;
  video_sample_entry->height_ = 0;
  video_sample_entry->horiz_resolution_ = (72 << 16);
  video_sample_entry->vert_resolution_ = (72 << 16);
  video_sample_entry->data_size_ = 0;
  video_sample_entry->frame_count_ = 1;
  memset(video_sample_entry->compressor_name_, 0, 32);
  video_sample_entry->depth_ = 24;
  video_sample_entry->color_table_id_ = -1;

  return video_sample_entry;
}

extern audio_sample_entry_t *audio_sample_entry_init() {
  audio_sample_entry_t *audio_sample_entry =
    (audio_sample_entry_t *)malloc(sizeof(audio_sample_entry_t));

  audio_sample_entry->version_ = 0;
  audio_sample_entry->revision_ = 0;
  audio_sample_entry->vendor_ = 0;
  audio_sample_entry->channel_count_ = 2;
  audio_sample_entry->sample_size_ = 16;
  audio_sample_entry->compression_id_ = 0;
  audio_sample_entry->packet_size_ = 0;
  audio_sample_entry->samplerate_ = (0 << 16);

  return audio_sample_entry;
}

extern void sample_entry_init(sample_entry_t *sample_entry) {
  sample_entry->len_ = 0;
  sample_entry->buf_ = 0;
  sample_entry->codec_private_data_length_ = 0;
  sample_entry->codec_private_data_ = 0;

  sample_entry->video_ = 0;
  sample_entry->audio_ = 0;
//sample_entry->hint_ = 0;

  sample_entry->nal_unit_length_ = 0;
  sample_entry->sps_length_ = 0;
  sample_entry->sps_ = 0;
  sample_entry->pps_length_ = 0;
  sample_entry->pps_ = 0;

  sample_entry->wFormatTag = 0;
  sample_entry->nChannels = 2;
  sample_entry->nSamplesPerSec = 44100;
  sample_entry->nAvgBytesPerSec = 0;
  sample_entry->nBlockAlign = 0;
  sample_entry->wBitsPerSample = 16;

  sample_entry->max_bitrate_ = 0;
  sample_entry->avg_bitrate_ = 0;
}

extern void sample_entry_assign(sample_entry_t *lhs, sample_entry_t const *rhs) {
  memcpy(lhs, rhs, sizeof(sample_entry_t));
  if(rhs->buf_ != NULL) {
    lhs->buf_ = (unsigned char *)malloc(rhs->len_);
    memcpy(lhs->buf_, rhs->buf_, rhs->len_);
  }
}

extern void sample_entry_exit(sample_entry_t *sample_entry) {
  if(sample_entry->buf_) {
    free(sample_entry->buf_);
  }

  if(sample_entry->video_) {
    free(sample_entry->video_);
  }
  if(sample_entry->audio_) {
    free(sample_entry->audio_);
  }
}

static const uint32_t aac_samplerates[] = {
  96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
  16000, 12000, 11025,  8000,  7350,     0,     0,     0
};

static int mp4_samplerate_to_index(unsigned int samplerate) {
  unsigned int i;
  for(i = 0; i != 13; ++i) {
    if(aac_samplerates[i] == samplerate)
      return i;
  }
  return 4;
}

// Create an ADTS frame header
extern void sample_entry_get_adts(sample_entry_t const *sample_entry,
                                  unsigned int sample_size, uint8_t *buf) {
  unsigned int syncword = 0xfff;
  unsigned int ID = 0; // MPEG-4
  unsigned int layer = 0;
  unsigned int protection_absent = 1;
  // 0 = Main profile AAC MAIN
  // 1 = Low Complexity profile (LC) AAC LC
  // 2 = Scalable Sample Rate profile (SSR) AAC SSR
  // 3 = (reserved) AAC LTP
  unsigned int profile = 1;
  unsigned int sampling_frequency_index =
    mp4_samplerate_to_index(sample_entry->nSamplesPerSec);
  unsigned int private_bit = 0;
  unsigned int channel_configuration = sample_entry->nChannels;
  unsigned int original_copy = 0;
  unsigned int home = 0;
  unsigned int copyright_identification_bit = 0;
  unsigned int copyright_identification_start = 0;
  unsigned int aac_frame_length = 7 + sample_size;
  unsigned int adts_buffer_fullness = 0x7ff;
  unsigned int no_raw_data_blocks_in_frame = 0;
  unsigned char buffer[8];

  uint64_t adts = 0;
  adts = (adts << 12) | syncword;
  adts = (adts << 1) | ID;
  adts = (adts << 2) | layer;
  adts = (adts << 1) | protection_absent;
  adts = (adts << 2) | profile;
  adts = (adts << 4) | sampling_frequency_index;
  adts = (adts << 1) | private_bit;
  adts = (adts << 3) | channel_configuration;
  adts = (adts << 1) | original_copy;
  adts = (adts << 1) | home;
  adts = (adts << 1) | copyright_identification_bit;
  adts = (adts << 1) | copyright_identification_start;
  adts = (adts << 13) | aac_frame_length;
  adts = (adts << 11) | adts_buffer_fullness;
  adts = (adts << 2) | no_raw_data_blocks_in_frame;

  write_64(buffer, adts);

  memcpy(buf, buffer + 1, 7);
}

extern stts_t *stts_init() {
  stts_t *atom = (stts_t *)malloc(sizeof(stts_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->table_ = 0;

  return atom;
}

extern void stts_exit(struct stts_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

extern unsigned int stts_get_sample(struct stts_t const *stts, uint64_t time) {
  unsigned int stts_index = 0;
  unsigned int stts_count;

  unsigned int ret = 0;
  uint64_t time_count = 0;

  for(; stts_index != stts->entries_; ++stts_index) {
    unsigned int sample_count = stts->table_[stts_index].sample_count_;
    unsigned int sample_duration = stts->table_[stts_index].sample_duration_;
    if(time_count + (uint64_t)sample_duration * (uint64_t)sample_count >= time) {
      stts_count = (unsigned int)((time - time_count + sample_duration - 1) / sample_duration);
      time_count += (uint64_t)stts_count * (uint64_t)sample_duration;
      ret += stts_count;
      break;
    } else {
      time_count += (uint64_t)sample_duration * (uint64_t)sample_count;
      ret += sample_count;
    }
  }
  return ret;
}

extern uint64_t stts_get_time(struct stts_t const *stts, unsigned int sample) {
  uint64_t ret = 0;
  unsigned int stts_index = 0;
  unsigned int sample_count = 0;

  for(;;) {
    unsigned int table_sample_count = stts->table_[stts_index].sample_count_;
    unsigned int table_sample_duration = stts->table_[stts_index].sample_duration_;
    if(sample_count + table_sample_count > sample) {
      unsigned int stts_count = (sample - sample_count);
      ret += (uint64_t)stts_count * (uint64_t)table_sample_duration;
      break;
    } else {
      sample_count += table_sample_count;
      ret += (uint64_t)table_sample_count * (uint64_t)table_sample_duration;
      stts_index++;
    }
  }
  return ret;
}

extern uint64_t stts_get_duration(struct stts_t const *stts) {
  uint64_t duration = 0;
  unsigned int i;
  for(i = 0; i != stts->entries_; ++i) {
    unsigned int sample_count = stts->table_[i].sample_count_;
    unsigned int sample_duration = stts->table_[i].sample_duration_;
    duration += (uint64_t)sample_duration * (uint64_t)sample_count;
  }

  return duration;
}

extern unsigned int stts_get_samples(struct stts_t const *stts) {
  unsigned int samples = 0;
  unsigned int entries = stts->entries_;
  unsigned int i;
  for(i = 0; i != entries; ++i) {
    unsigned int sample_count = stts->table_[i].sample_count_;
//  unsigned int sample_duration = stts->table_[i].sample_duration_;
    samples += sample_count;
  }

  return samples;
}

extern struct stss_t *stss_init() {
  stss_t *atom = (stss_t *)malloc(sizeof(stss_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->sample_numbers_ = 0;

  return atom;
}

extern void stss_exit(struct stss_t *atom) {
  if(atom->sample_numbers_) {
    free(atom->sample_numbers_);
  }
  free(atom);
}

extern unsigned int stss_get_nearest_keyframe(struct stss_t const *stss,
    unsigned int sample) {
  // scan the sync samples to find the key frame that precedes the sample number
  unsigned int i;
  unsigned int table_sample = 0;
  for(i = 0; i != stss->entries_; ++i) {
    table_sample = stss->sample_numbers_[i];
    if(table_sample >= sample)
      break;
  }
  if(table_sample == sample)
    return table_sample;
  else
    return stss->sample_numbers_[i - 1];
}

extern stsc_t *stsc_init() {
  stsc_t *atom = (stsc_t *)malloc(sizeof(stsc_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->table_ = 0;

  return atom;
}

extern void stsc_exit(struct stsc_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

extern stsz_t *stsz_init() {
  stsz_t *atom = (stsz_t *)malloc(sizeof(stsz_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->sample_size_ = 0;
  atom->entries_ = 0;
  atom->sample_sizes_ = 0;

  return atom;
}

extern void stsz_exit(struct stsz_t *atom) {
  if(atom->sample_sizes_) {
    free(atom->sample_sizes_);
  }
  free(atom);
}

extern stco_t *stco_init() {
  stco_t *atom = (stco_t *)malloc(sizeof(stco_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->chunk_offsets_ = 0;

  return atom;
}

extern void stco_exit(stco_t *atom) {
  if(atom->chunk_offsets_) {
    free(atom->chunk_offsets_);
  }
  free(atom);
}

#if 0
extern void stco_shift_offsets(stco_t *stco, int offset) {
  unsigned int i;
  for(i = 0; i != stco->entries_; ++i)
    stco->chunk_offsets_[i] += offset;
}
#endif

extern struct ctts_t *ctts_init() {
  struct ctts_t *atom = (struct ctts_t *)malloc(sizeof(struct ctts_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->table_ = 0;

  return atom;
}

extern void ctts_exit(struct ctts_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

extern unsigned int ctts_get_samples(struct ctts_t const *ctts) {
  unsigned int samples = 0;
  unsigned int entries = ctts->entries_;
  unsigned int i;
  for(i = 0; i != entries; ++i) {
    unsigned int sample_count = ctts->table_[i].sample_count_;
//  unsigned int sample_offset = ctts->table_[i].sample_offset_;
    samples += sample_count;
  }

  return samples;
}

extern uint64_t moov_time_to_trak_time(uint64_t t, uint32_t moov_time_scale,
                                       uint32_t trak_time_scale) {
  return t * (uint64_t)trak_time_scale / moov_time_scale;
}

extern uint64_t trak_time_to_moov_time(uint64_t t, uint32_t moov_time_scale,
                                       uint32_t trak_time_scale) {
  return t * (uint64_t)moov_time_scale / trak_time_scale;
}

extern mvex_t *mvex_init() {
  mvex_t *mvex = (mvex_t *)malloc(sizeof(mvex_t));
  mvex->unknown_atoms_ = 0;
  mvex->tracks_ = 0;

  return mvex;
}

extern void mvex_exit(mvex_t *atom) {
  unsigned int i;
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  for(i = 0; i != atom->tracks_; ++i) {
    trex_exit(atom->trexs_[i]);
  }
  free(atom);
}

extern trex_t *trex_init() {
  trex_t *trex = (trex_t *)malloc(sizeof(trex_t));

  trex->version_ = 0;
  trex->flags_ = 0;
  trex->track_id_ = 0;
  trex->default_sample_description_index_ = 0;
  trex->default_sample_duration_ = 0;
  trex->default_sample_size_ = 0;
  trex->default_sample_flags_ = 0;

  return trex;
}

extern void trex_exit(trex_t *atom) {
  free(atom);
}

extern moof_t *moof_init() {
  struct moof_t *moof = (struct moof_t *)malloc(sizeof(struct moof_t));
  moof->unknown_atoms_ = 0;
  moof->mfhd_ = 0;
  moof->tracks_ = 0;

  return moof;
}

extern void moof_exit(struct moof_t *atom) {
  unsigned int i;
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->mfhd_) {
    mfhd_exit(atom->mfhd_);
  }
  for(i = 0; i != atom->tracks_; ++i) {
    traf_exit(atom->trafs_[i]);
  }
  free(atom);
}

extern mfhd_t *mfhd_init() {
  mfhd_t *mfhd = (mfhd_t *)malloc(sizeof(mfhd_t));
  mfhd->version_ = 0;
  mfhd->flags_ = 0;
  mfhd->sequence_number_ = 0;

  return mfhd;
}

extern void mfhd_exit(mfhd_t *atom) {
  free(atom);
}

extern traf_t *traf_init() {
  traf_t *traf = (traf_t *)malloc(sizeof(traf_t));
  traf->unknown_atoms_ = 0;
  traf->tfhd_ = 0;
  traf->trun_ = 0;
  traf->uuid0_ = 0;
  traf->uuid1_ = 0;

  return traf;
}

extern void traf_exit(traf_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->tfhd_) {
    tfhd_exit(atom->tfhd_);
  }
  if(atom->trun_) {
    trun_t *trun = atom->trun_;
    while(trun) {
      trun_t *next = trun->next_;
      trun_exit(trun);
      trun = next;
    }
  }
  if(atom->uuid0_) {
    uuid0_exit(atom->uuid0_);
  }
  if(atom->uuid1_) {
    uuid1_exit(atom->uuid1_);
  }
  free(atom);
}

extern tfhd_t *tfhd_init() {
  tfhd_t *tfhd = (tfhd_t *)malloc(sizeof(tfhd_t));

  tfhd->version_ = 0;
  tfhd->flags_ = 0;

  return tfhd;
}

extern void tfhd_exit(tfhd_t *atom) {
  free(atom);
}

extern tfra_t *tfra_init() {
  tfra_t *tfra = (tfra_t *)malloc(sizeof(tfra_t));
  tfra->table_ = 0;

  return tfra;
}

extern void tfra_exit(tfra_t *tfra) {
  if(tfra->table_) {
//    free(tfra->table_);
  }
  free(tfra);
}

extern void tfra_add(tfra_t *tfra, tfra_table_t const *table) {
  tfra_table_t *tfra_table;

  // allocate one more entry
  tfra->table_ = (tfra_table_t *)
                 realloc(tfra->table_, (tfra->number_of_entry_ + 1) * sizeof(tfra_table_t));

  tfra_table = &tfra->table_[tfra->number_of_entry_];

  tfra_table->time_ = table->time_;
  tfra_table->moof_offset_ = table->moof_offset_;
  tfra_table->traf_number_ = table->traf_number_;
  tfra_table->trun_number_ = table->trun_number_;
  tfra_table->sample_number_ = table->sample_number_;
  ++tfra->number_of_entry_;
}

extern mfra_t *mfra_init() {
  mfra_t *mfra = (mfra_t *)malloc(sizeof(mfra_t));
  mfra->unknown_atoms_ = 0;
  mfra->tracks_ = 0;

  return mfra;
}

extern void mfra_exit(mfra_t *atom) {
  unsigned int i;
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  for(i = 0; i != atom->tracks_; ++i) {
    tfra_exit(atom->tfras_[i]);
  }
  free(atom);
}

extern trun_t *trun_init() {
  trun_t *trun = (trun_t *)malloc(sizeof(trun_t));
  trun->version_ = 0;
  trun->flags_ = 0;
  trun->sample_count_ = 0;
  trun->data_offset_ = 0;
  trun->first_sample_flags_ = 0;
  trun->table_ = 0;
  trun->next_ = 0;

  return trun;
}

extern void trun_exit(struct trun_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

extern uuid0_t *uuid0_init() {
  uuid0_t *uuid = (uuid0_t *)malloc(sizeof(uuid0_t));

  uuid->pts_ = 0;
  uuid->duration_ = 0;

  return uuid;
}

extern void uuid0_exit(uuid0_t *atom) {
  free(atom);
}

extern uuid1_t *uuid1_init() {
  unsigned int i;
  uuid1_t *uuid = (uuid1_t *)malloc(sizeof(uuid1_t));
  uuid->entries_ = 0;
  for(i = 0; i != 2; ++i) {
    uuid->pts_[i] = 0;
    uuid->duration_[i] = 0;
  }

  return uuid;
}

extern void uuid1_exit(uuid1_t *atom) {
  free(atom);
}

// End Of File

