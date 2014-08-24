/*******************************************************************************
 ngx_http_streaming - An Nginx module for streaming MPEG4 files

 For licensing see the LICENSE file
******************************************************************************/

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <inttypes.h>

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
#define MAX_TRACKS 8

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

typedef struct {
    ngx_uint_t	length;
    ngx_flag_t	relative;
    size_t	buffer_size;
    size_t	max_buffer_size;
} hls_conf_t;

struct moov_t {
    struct unknown_atom_t *unknown_atoms_;
    struct mvhd_t *mvhd_;
    unsigned int tracks_;
    struct trak_t *traks_[MAX_TRACKS];
    struct mvex_t *mvex_;

    int is_indexed_;
};
typedef struct moov_t moov_t;

struct mp4_atom_t {
    uint32_t type_;
    uint32_t short_size_;
    uint64_t size_;
    uint64_t start_;
};
typedef struct mp4_atom_t mp4_atom_t;

struct mp4_context_t {
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
    u_char	*buffer;
    off_t	offset;
    size_t	buffer_size;
    off_t	filesize;
    ngx_flag_t	alignment;
};
typedef struct mp4_context_t mp4_context_t;

static char *ngx_streaming(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_hls_create_conf(ngx_conf_t *cf);
static char *ngx_http_hls_merge_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_command_t ngx_streaming_commands[] = {
    { ngx_string("hls"),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
      ngx_streaming,
      0,
      0,
      NULL },
    { ngx_string("hls_length"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(hls_conf_t, length),
      NULL },
    { ngx_string("hls_relative"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(hls_conf_t, relative),
      NULL },
    { ngx_string("hls_mp4_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(hls_conf_t, buffer_size),
      NULL },

    { ngx_string("hls_mp4_max_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(hls_conf_t, max_buffer_size),
      NULL },

  ngx_null_command
};

static ngx_http_module_t ngx_streaming_module_ctx = {
  NULL,                          /* preconfiguration */
  NULL,                          /* postconfiguration */

  NULL,                          /* create main configuration */
  NULL,                          /* init main configuration */

  NULL,                          /* create server configuration */
  NULL,                          /* merge server configuration */

  ngx_http_hls_create_conf,      /* create location configuration */
  ngx_http_hls_merge_conf        /* merge location configuration */
};

ngx_module_t ngx_http_streaming_module = {
  NGX_MODULE_V1,
  &ngx_streaming_module_ctx,     /* module context */
  ngx_streaming_commands,        /* module directives */
  NGX_HTTP_MODULE,               /* module type */
  NULL,                          /* init master */
  NULL,                          /* init module */
  NULL,                          /* init process */
  NULL,                          /* init thread */
  NULL,                          /* exit thread */
  NULL,                          /* exit process */
  NULL,                          /* exit master */
  NGX_MODULE_V1_PADDING
};

// End Of File

