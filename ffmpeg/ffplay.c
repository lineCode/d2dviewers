// ffplay.c
//{{{  includes
#include "config.h"

#include <inttypes.h>
#include <stdint.h>
#include <math.h>

#include "libavfilter/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/avutil.h"

#include "libavcodec/avfft.h"
#include "libavutil/avstring.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"

#include <SDL.h>
#include <SDL_ttf.h>
#define SDL_main main
//}}}
//{{{  const
#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 5

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define BPP 1

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

#define FF_ALLOC_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT  (SDL_USEREVENT + 2)
//}}}
//{{{  typedef
//{{{
typedef struct MyAVPacketList {
  AVPacket pkt;
  struct MyAVPacketList* next;
  int serial;
  } MyAVPacketList;
//}}}
//{{{
typedef struct PacketQueue {
  MyAVPacketList* first_pkt;
  MyAVPacketList* last_pkt;

  int num_packets;
  int size;
  int abort_request;
  int serial;

  SDL_mutex* mutex;
  SDL_cond* cond;
  } PacketQueue;
//}}}

//{{{
typedef struct Clock {
  double pts;           /* clock base */
  double pts_drift;     /* clock base minus time at which we updated the clock */
  double last_updated;
  double speed;
  int serial;           /* clock is based on a packet with this serial */
  int paused;
  int* queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
  } Clock;
//}}}
//{{{
typedef struct AudioParams {
  int freq;
  int channels;
  int64_t channel_layout;
  enum AVSampleFormat fmt;
  int frame_size;
  int bytes_per_sec;
  } AudioParams;
//}}}

//{{{
typedef struct Frame {
  AVFrame* frame;
  AVSubtitle sub;
  SDL_Overlay* bmp;

  int serial;
  double pts;      /* presentation timestamp for the frame */
  double duration; /* estimated duration of the frame */
  int64_t pos;     /* byte position of the frame in the input file */

  int allocated;
  int reallocate;

  AVRational sar;
  int width;
  int height;

  } Frame;
//}}}
//{{{
typedef struct FrameQueue {
  Frame queue[FRAME_QUEUE_SIZE];

  int rindex;
  int windex;
  int size;
  int max_size;
  int keep_last;
  int rindex_shown;

  SDL_mutex* mutex;
  SDL_cond* cond;
  PacketQueue* pktq;
  } FrameQueue;
//}}}

//{{{
typedef struct Decoder {
  AVPacket pkt;
  AVPacket pkt_temp;
  PacketQueue* queue;
  AVCodecContext* avctx;

  int pkt_serial;
  int finished;
  int packet_pending;

  SDL_cond* empty_queue_cond;

  int64_t start_pts;
  AVRational start_pts_tb;

  int64_t next_pts;
  AVRational next_pts_tb;

  SDL_Thread* decoder_tid;
  } Decoder;
//}}}
//{{{
typedef struct VideoState {
  char filename[1024];
  SDL_Thread* readThread;
  SDL_cond* continueReadThread;
  int abort_request;
  int force_refresh;
  int paused;
  int last_paused;
  int seek_req;
  int64_t seek_pos;
  int64_t seek_rel;
  int read_pause_return;
  int eof;
  int step;

  AVFormatContext* ic;

  Clock extclk;
  double max_frame_duration;
  int frame_drops_early;
  int frame_drops_late;

  //{{{
  enum ShowMode {
    SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT
    } show_mode;
  //}}}

  // audio
  int audio_stream;
  int last_audio_stream;
  AVStream* audio_st;
  PacketQueue audioq;
  Clock audclk;
  FrameQueue sampq;
  Decoder auddec;

  double audio_clock;
  int audio_clock_serial;
  double audio_diff_cum; /* used for AV difference average computation */
  double audio_diff_avg_coef;
  double audio_diff_threshold;
  int audio_diff_avg_count;

  int audio_hw_buf_size;
  uint8_t silence_buf[SDL_AUDIO_MIN_BUFFER_SIZE];
  uint8_t* audio_buf;
  uint8_t* audio_buf1;
  unsigned int audio_buf_size; /* in bytes */
  unsigned int audio_buf1_size;
  int audio_buf_index; /* in bytes */
  int audio_write_buf_size;
  struct AudioParams audio_src;
  struct AudioParams audio_filter_src;
  struct AudioParams audio_tgt;
  struct SwrContext* swr_ctx;

  int16_t sample_array[SAMPLE_ARRAY_SIZE];
  int sample_array_index;
  int last_i_start;
  RDFTContext* rdft;
  int rdft_bits;
  FFTSample* rdft_data;
  int xpos;

  AVFilterContext* in_audio_filter;   // the first filter in the audio chain
  AVFilterContext* out_audio_filter;  // the last filter in the audio chain
  AVFilterGraph* agraph;              // audio filter graph

  // video
  int video_stream;
  int last_video_stream;
  AVStream* video_st;
  PacketQueue videoq;
  Clock vidclk;
  FrameQueue pictq;
  Decoder viddec;

  int viddec_width;
  int viddec_height;
  int width, height, xleft, ytop;
  SDL_Rect last_display_rect;

  int vfilter_idx;
  AVFilterContext* in_video_filter;   // the first filter in the video chain
  AVFilterContext* out_video_filter;  // the last filter in the video chain

  // subtitle
  int subtitle_stream;
  int last_subtitle_stream;
  AVStream* subtitle_st;
  PacketQueue subtitleq;
  FrameQueue subpq;
  Decoder subdec;
  struct SwsContext* subSwsContext;

  double frame_timer;
  double frame_last_returned_time;
  double frame_last_filter_delay;
  double last_vis_time;
  } VideoState;
//}}}
//}}}
//{{{  vars
static SDL_Surface* screen;
TTF_Font* font = NULL;

static int is_full_screen;
static int fs_screen_width;
static int fs_screen_height;
static int default_width  = 640;
static int default_height = 480;
static int screen_width  = 0;
static int screen_height = 0;

double rdftspeed = 0.02;

static int cursor_hidden = 0;
static int64_t cursor_last_shown;

static int num_vfilters = 0;
static char** vfilters_list = NULL;
static char* afilters = NULL;

static int64_t audioCallbackTime;

static AVPacket flush_pkt;

static int64_t last_time = 0;
static char infoStr[100] = "";
//}}}

//{{{  Clock
//{{{
static double get_clock (Clock* c)
{
  if (*c->queue_serial != c->serial)
    return NAN;

  if (c->paused)
    return c->pts;
  else {
    double time = av_gettime_relative() / 1000000.0;
    return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }

  }
//}}}
//{{{
static double get_master_clock (VideoState* vs) {

  return (vs->audio_st) ? get_clock (&vs->audclk) : get_clock (&vs->extclk);
  }
//}}}

//{{{
static void set_clock_at (Clock* c, double pts, int serial, double time) {

  c->pts = pts;
  c->last_updated = time;
  c->pts_drift = c->pts - time;
  c->serial = serial;
  }
//}}}
//{{{
static void set_clock (Clock* c, double pts, int serial) {

  double time = av_gettime_relative() / 1000000.0;
  set_clock_at (c, pts, serial, time);
  }
//}}}
//{{{
static void set_clock_speed (Clock* c, double speed) {

  set_clock (c, get_clock(c), c->serial);
  c->speed = speed;
  }
//}}}

//{{{
static void init_clock (Clock* c, int* queue_serial) {

  c->speed = 1.0;
  c->paused = 0;
  c->queue_serial = queue_serial;
  set_clock (c, NAN, -1);
  }
//}}}
//{{{
static void sync_clock_to_slave (Clock* c, Clock *slave) {

  double clock = get_clock (c);
  double slave_clock = get_clock (slave);

  if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
    set_clock (c, slave_clock, slave->serial);
  }
//}}}

//{{{
static double compute_target_delay (double delay, VideoState* vs) {

  double sync_threshold, diff = 0;

  if (vs->audio_st) {
    // if video is slave, we try to correct big delays by duplicating or deleting a frame
    diff = get_clock (&vs->vidclk) - get_master_clock (vs);

    // skip or repeat frame
    sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN (AV_SYNC_THRESHOLD_MAX, delay));
    if (!isnan (diff) && fabs (diff) < vs->max_frame_duration) {
      if (diff <= -sync_threshold)
        delay = FFMAX(0, delay + diff);
      else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
        delay = delay + diff;
      else if (diff >= sync_threshold)
        delay = 2 * delay;
      }
    }

  av_log (NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n", delay, -diff);

  return delay;
  }
//}}}
//{{{
static double vp_duration (VideoState* vs, Frame* vp, Frame* nextvp) {

  if (vp->serial == nextvp->serial) {
    double duration = nextvp->pts - vp->pts;
    if (isnan (duration) || duration <= 0 || duration > vs->max_frame_duration)
      return vp->duration;
    else
      return duration;
    }
  else
    return 0.0;
  }
//}}}
//}}}
//{{{  PacketQueue
//{{{
static int packet_queue_put_private (PacketQueue* q, AVPacket* pkt) {

  MyAVPacketList *pkt1;

  if (q->abort_request)
    return -1;

  pkt1 = av_malloc(sizeof(MyAVPacketList));
  if (!pkt1)
    return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;
  if (pkt == &flush_pkt)
    q->serial++;
  pkt1->serial = q->serial;

  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->num_packets++;
  q->size += pkt1->pkt.size + sizeof(*pkt1);

  /* XXX: should duplicate packet data in DV case */
  SDL_CondSignal(q->cond);
  return 0;
  }
//}}}
//{{{
static int packet_queue_put (PacketQueue* q, AVPacket* pkt) {

  int ret;

  /* duplicate the packet */
  if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
    return -1;

  SDL_LockMutex (q->mutex);
  ret = packet_queue_put_private (q, pkt);
  SDL_UnlockMutex (q->mutex);

  if (pkt != &flush_pkt && ret < 0)
    av_free_packet (pkt);

  return ret;
  }
//}}}
//{{{
static int packet_queue_put_nullpacket (PacketQueue* q, int streamIndex) {

  AVPacket pkt1, *pkt = &pkt1;
  av_init_packet (pkt);

  pkt->data = NULL;
  pkt->size = 0;
  pkt->stream_index = streamIndex;
  return packet_queue_put (q, pkt);
  }
//}}}

//{{{
/* packet queue handling */
static void packet_queue_init (PacketQueue* q) {

  memset (q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
  q->abort_request = 1;
  }
//}}}
//{{{
static void packet_queue_flush (PacketQueue* q) {

  MyAVPacketList *pkt, *pkt1;

  SDL_LockMutex (q->mutex);
  for (pkt = q->first_pkt; pkt; pkt = pkt1) {
    pkt1 = pkt->next;
    av_free_packet (&pkt->pkt);
    av_freep (&pkt);
    }

  q->last_pkt = NULL;
  q->first_pkt = NULL;
  q->num_packets = 0;
  q->size = 0;

  SDL_UnlockMutex (q->mutex);
  }
//}}}
//{{{
static void packet_queue_destroy (PacketQueue* q) {

  packet_queue_flush (q);

  SDL_DestroyMutex (q->mutex);
  SDL_DestroyCond (q->cond);
  }
//}}}
//{{{
static void packet_queue_abort (PacketQueue* q) {

  SDL_LockMutex (q->mutex);

  q->abort_request = 1;
  SDL_CondSignal (q->cond);

  SDL_UnlockMutex (q->mutex);
  }
//}}}
//{{{
static void packet_queue_start (PacketQueue* q) {

  SDL_LockMutex (q->mutex);

  q->abort_request = 0;
  packet_queue_put_private (q, &flush_pkt);

  SDL_UnlockMutex (q->mutex);
  }
//}}}

//{{{
/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get (PacketQueue* q, AVPacket* pkt, int block, int* serial) {

  MyAVPacketList *pkt1;
  int ret;

  SDL_LockMutex (q->mutex);

  for (;;) {
    if (q->abort_request) {
      ret = -1;
      break;
      }

    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt)
        q->last_pkt = NULL;
      q->num_packets--;
      q->size -= pkt1->pkt.size + sizeof (*pkt1);
      *pkt = pkt1->pkt;
      if (serial)
        *serial = pkt1->serial;
      av_free (pkt1);
      ret = 1;
      break;
      }
    else if (!block) {
      ret = 0;
      break;
      }
    else
      SDL_CondWait (q->cond, q->mutex);
    }

  SDL_UnlockMutex (q->mutex);

  return ret;
  }
//}}}
//}}}
//{{{  FrameQueue
//{{{
static void frame_queue_unref_item (Frame* vp) {

  av_frame_unref (vp->frame);
  avsubtitle_free (&vp->sub);
  }
//}}}

//{{{
static int frame_queue_init (FrameQueue* f, PacketQueue* pktq, int max_size, int keep_last) {

  memset (f, 0, sizeof(FrameQueue));

  if (!(f->mutex = SDL_CreateMutex()))
    return AVERROR(ENOMEM);
  if (!(f->cond = SDL_CreateCond()))
    return AVERROR(ENOMEM);

  f->pktq = pktq;
  f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
  f->keep_last = !!keep_last;
  for (int i = 0; i < f->max_size; i++)
    if (!(f->queue[i].frame = av_frame_alloc()))
      return AVERROR(ENOMEM);

  return 0;
  }
//}}}
//{{{
static void frame_queue_destory (FrameQueue* f) {

  int i;
  for (i = 0; i < f->max_size; i++) {
    Frame* vp = &f->queue[i];
    frame_queue_unref_item (vp);
    av_frame_free (&vp->frame);
    SDL_FreeYUVOverlay (vp->bmp);
    }

  SDL_DestroyMutex (f->mutex);
  SDL_DestroyCond (f->cond);
  }
//}}}
//{{{
static void frame_queue_signal (FrameQueue* f) {

  SDL_LockMutex (f->mutex);
  SDL_CondSignal (f->cond);
  SDL_UnlockMutex (f->mutex);
  }
//}}}

//{{{
static Frame* frame_queue_peek (FrameQueue* f) {

  return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
  }
//}}}
//{{{
static Frame* frame_queue_peek_next (FrameQueue* f) {

  return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
  }
//}}}
//{{{
static Frame* frame_queue_peek_last (FrameQueue* f) {

  return &f->queue[f->rindex];
  }
//}}}
//{{{
static Frame* frame_queue_peek_writable (FrameQueue* f) {

  /* wait until we have space to put a new frame */
  SDL_LockMutex (f->mutex);

  while (f->size >= f->max_size && !f->pktq->abort_request)
    SDL_CondWait(f->cond, f->mutex);

  SDL_UnlockMutex (f->mutex);

  if (f->pktq->abort_request)
    return NULL;

  return &f->queue[f->windex];
  }
//}}}
//{{{
static Frame* frame_queue_peek_readable (FrameQueue* f) {

  /* wait until we have a readable a new frame */
  SDL_LockMutex (f->mutex);

  while (f->size - f->rindex_shown <= 0 && !f->pktq->abort_request)
    SDL_CondWait (f->cond, f->mutex);

  SDL_UnlockMutex (f->mutex);

  if (f->pktq->abort_request)
    return NULL;

  return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
  }
//}}}

//{{{
static void frame_queue_push (FrameQueue* f) {

  if (++f->windex == f->max_size)
    f->windex = 0;

  SDL_LockMutex (f->mutex);
  f->size++;

  SDL_CondSignal (f->cond);
  SDL_UnlockMutex (f->mutex);
  }
//}}}
//{{{
static void frame_queue_next (FrameQueue* f) {

  if (f->keep_last && !f->rindex_shown) {
      f->rindex_shown = 1;
    return;
    }

  frame_queue_unref_item(&f->queue[f->rindex]);
  if (++f->rindex == f->max_size)
    f->rindex = 0;

  SDL_LockMutex (f->mutex);
  f->size--;

  SDL_CondSignal (f->cond);
  SDL_UnlockMutex (f->mutex);
  }
//}}}
//{{{
/* jump back to the previous frame if available by resetting rindex_shown */
static int frame_queue_prev (FrameQueue* f) {

  int ret = f->rindex_shown;
  f->rindex_shown = 0;
  return ret;
  }
//}}}
//{{{
/* return the number of undisplayed frames in the queue */
static int frame_queue_num_remaining (FrameQueue* f) {

  return f->size - f->rindex_shown;
  }
//}}}
//{{{
/* return last shown position */
static int64_t frame_queue_last_pos (FrameQueue* f) {

  Frame* fp = &f->queue[f->rindex];
  if (f->rindex_shown && fp->serial == f->pktq->serial)
    return fp->pos;
  else
    return -1;
  }
//}}}
//}}}

//{{{
static int configFiltergraph (AVFilterGraph* graph, const char* filtergraph,
                               AVFilterContext* source_ctx, AVFilterContext* sink_ctx) {

  int ret = 0;

  AVFilterInOut* inputs = NULL;
  AVFilterInOut* outputs = NULL;
  int num_filters = graph->nb_filters;

  if (filtergraph) {
    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    if (!outputs || !inputs) {
      ret = AVERROR (ENOMEM);
      goto exit;
      }

    outputs->name = av_strdup ("in");
    outputs->filter_ctx = source_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup ("out");
    inputs->filter_ctx = sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr (graph, filtergraph, &inputs, &outputs, NULL)) < 0)
      goto exit;
    }
  else {
    if ((ret = avfilter_link (source_ctx, 0, sink_ctx, 0)) < 0)
      goto exit;
    }

  // Reorder the filters to ensure that inputs of the custom filters are merged first
  for (unsigned int i = 0; i < graph->nb_filters - num_filters; i++)
    FFSWAP (AVFilterContext*, graph->filters[i], graph->filters[i + num_filters]);

  ret = avfilter_graph_config (graph, NULL);

exit:
  avfilter_inout_free (&outputs);
  avfilter_inout_free (&inputs);

  return ret;
  }
//}}}
//{{{
static int configVideoFilters (AVFilterGraph* graph, VideoState* vs, const char* vfilters, AVFrame* frame) {

  static const enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
  char buffersrc_args[256];
  int ret;
  AVFilterContext* filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
  AVCodecContext* codec = vs->video_st->codec;
  AVRational fr = av_guess_frame_rate(vs->ic, vs->video_st, NULL);

  graph->scale_sws_opts = 0;

  snprintf (buffersrc_args, sizeof(buffersrc_args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            frame->width, frame->height, frame->format,
            vs->video_st->time_base.num, vs->video_st->time_base.den,
            codec->sample_aspect_ratio.num, FFMAX(codec->sample_aspect_ratio.den, 1));

  if (fr.num && fr.den)
    av_strlcatf (buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

  if ((ret = avfilter_graph_create_filter (&filt_src, avfilter_get_by_name("buffer"),
                                           "ffplay_buffer", buffersrc_args, NULL, graph)) < 0)
    goto exit;

  ret = avfilter_graph_create_filter (&filt_out, avfilter_get_by_name("buffersink"),
                                      "ffplay_buffersink", NULL, NULL, graph);
  if (ret < 0)
    goto exit;

  if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts,  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
    goto exit;

  last_filter = filt_out;

  // macro adds a filter before the lastly added filter, processing order of filters is in reverse
  #define INSERT_FILT(name, arg) do {                                        \
    AVFilterContext *filt_ctx;                                               \
                                                                             \
    ret = avfilter_graph_create_filter (&filt_ctx,                           \
                                        avfilter_get_by_name(name),          \
                                        "ffplay_" name, arg, NULL, graph);   \
    if (ret < 0)                                                             \
      goto exit;                                                             \
                                                                             \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
    if (ret < 0)                                                             \
      goto exit;                                                             \
                                                                             \
    last_filter = filt_ctx;                                                  \
  } while (0)

  /* SDL YUV code is not handling odd width/height for some driver
   * combinations, therefore we crop the picture to an even width/height. */
  INSERT_FILT("crop", "floor(in_w/2)*2:floor(in_h/2)*2");

  if ((ret = configFiltergraph (graph, vfilters, filt_src, last_filter)) < 0)
    goto exit;

  vs->in_video_filter  = filt_src;
  vs->out_video_filter = filt_out;

exit:
  return ret;
  }
//}}}
//{{{
static int configAudioFilters (VideoState* vs, const char* afilters, int force_output_format) {

  static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
  int sample_rates[2] = { 0, -1 };
  int64_t channel_layouts[2] = { 0, -1 };
  int channels[2] = { 0, -1 };
  AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
  AVDictionaryEntry *e = NULL;
  char asrc_args[256];
  int ret;

  avfilter_graph_free (&vs->agraph);
  if (!(vs->agraph = avfilter_graph_alloc()))
    return AVERROR(ENOMEM);

  av_opt_set (vs->agraph, "aresample_swr_opts", 0, 0);

  ret = snprintf (asrc_args, sizeof(asrc_args),
                  "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d",
                  vs->audio_filter_src.freq, av_get_sample_fmt_name(vs->audio_filter_src.fmt),
                  vs->audio_filter_src.channels,
                  1, vs->audio_filter_src.freq);
  if (vs->audio_filter_src.channel_layout)
    snprintf (asrc_args + ret, sizeof(asrc_args) - ret,
              ":channel_layout=0x%"PRIx64,  vs->audio_filter_src.channel_layout);

  ret = avfilter_graph_create_filter (
    &filt_asrc, avfilter_get_by_name ("abuffer"), "ffplay_abuffer", asrc_args, NULL, vs->agraph);
  if (ret < 0)
      goto exit;

  ret = avfilter_graph_create_filter (
    &filt_asink, avfilter_get_by_name ("abuffersink"), "ffplay_abuffersink", NULL, NULL, vs->agraph);
  if (ret < 0)
    goto exit;

  if ((ret = av_opt_set_int_list (filt_asink, "sample_fmts", sample_fmts,  AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
    goto exit;
  if ((ret = av_opt_set_int (filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN)) < 0)
    goto exit;

  if (force_output_format) {
    channel_layouts[0] = vs->audio_tgt.channel_layout;
    channels [0] = vs->audio_tgt.channels;
    sample_rates [0] = vs->audio_tgt.freq;
    if ((ret = av_opt_set_int (filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN)) < 0)
      goto exit;
    if ((ret = av_opt_set_int_list (filt_asink, "channel_layouts", channel_layouts,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
      goto exit;
    if ((ret = av_opt_set_int_list (filt_asink, "channel_counts" , channels       ,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
      goto exit;
    if ((ret = av_opt_set_int_list (filt_asink, "sample_rates"   , sample_rates   ,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
      goto exit;
    }

  if ((ret = configFiltergraph (vs->agraph, afilters, filt_asrc, filt_asink)) < 0)
    goto exit;

  vs->in_audio_filter  = filt_asrc;
  vs->out_audio_filter = filt_asink;

exit:
  if (ret < 0)
    avfilter_graph_free (&vs->agraph);

  return ret;
  }
//}}}

// audio
//{{{
static int audioDecodeFrame (VideoState* vs) {
// Decode one audio frame and return its uncompressed size.
// The processed audio frame is decoded, converted, stored in vs->audio_buf, size in bytes return value.

  int ret_data_size;
  Frame* af;
  do {
    if (!(af = frame_queue_peek_readable (&vs->sampq)))
      return -1;
    frame_queue_next (&vs->sampq);
    } while (af->serial != vs->audioq.serial);

  int data_size = av_samples_get_buffer_size (
    NULL, av_frame_get_channels(af->frame), af->frame->nb_samples, af->frame->format, 1);

  int64_t channel_layout =
    (af->frame->channel_layout &&
     av_frame_get_channels (af->frame) == av_get_channel_layout_nb_channels (af->frame->channel_layout)) ?
      af->frame->channel_layout : av_get_default_channel_layout (av_frame_get_channels(af->frame));
  int wanted_num_samples = af->frame->nb_samples;

  if (af->frame->format != vs->audio_src.fmt ||
      channel_layout != vs->audio_src.channel_layout ||
      af->frame->sample_rate != vs->audio_src.freq ||
      (wanted_num_samples != af->frame->nb_samples && !vs->swr_ctx)) {
    swr_free (&vs->swr_ctx);
    vs->swr_ctx = swr_alloc_set_opts (
      NULL, vs->audio_tgt.channel_layout, vs->audio_tgt.fmt, vs->audio_tgt.freq,
      channel_layout, af->frame->format, af->frame->sample_rate, 0, NULL);

    vs->audio_src.channel_layout = channel_layout;
    vs->audio_src.channels = av_frame_get_channels(af->frame);
    vs->audio_src.freq = af->frame->sample_rate;
    vs->audio_src.fmt = af->frame->format;
    }

  if (vs->swr_ctx) {
    //{{{  resample
    const uint8_t** in = (const uint8_t**)af->frame->extended_data;
    uint8_t** out = &vs->audio_buf1;
    int out_count = (int64_t)wanted_num_samples * vs->audio_tgt.freq / af->frame->sample_rate + 256;
    int out_size  = av_samples_get_buffer_size (NULL, vs->audio_tgt.channels, out_count, vs->audio_tgt.fmt, 0);
    int len2;
    if (out_size < 0) {
      av_log (NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
      return -1;
      }

    if (wanted_num_samples != af->frame->nb_samples) {
      if (swr_set_compensation (
          vs->swr_ctx,
          (wanted_num_samples - af->frame->nb_samples) * vs->audio_tgt.freq / af->frame->sample_rate,
          wanted_num_samples * vs->audio_tgt.freq / af->frame->sample_rate) < 0) {
          av_log (NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
          return -1;
          }
      }

    av_fast_malloc (&vs->audio_buf1, &vs->audio_buf1_size, out_size);
    if (!vs->audio_buf1)
      return AVERROR (ENOMEM);

    len2 = swr_convert (vs->swr_ctx, out, out_count, in, af->frame->nb_samples);
    if (len2 < 0) {
      av_log (NULL, AV_LOG_ERROR, "swr_convert() failed\n");
      return -1;
      }

    if (len2 == out_count) {
      av_log (NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
      if (swr_init (vs->swr_ctx) < 0)
        swr_free (&vs->swr_ctx);
      }

    vs->audio_buf = vs->audio_buf1;
    ret_data_size = len2 * vs->audio_tgt.channels * av_get_bytes_per_sample(vs->audio_tgt.fmt);
    }
    //}}}
  else {
    vs->audio_buf = af->frame->data[0];
    ret_data_size = data_size;
    }

  vs->audio_clock = isnan (af->pts) ? NAN : af->pts + ((double)af->frame->nb_samples / af->frame->sample_rate);
  vs->audio_clock_serial = af->serial;

  return ret_data_size;
  }
//}}}
//{{{
static void updateSamples (VideoState* vs, short* samples, int samples_size) {
// update sample array for display

  int size = samples_size / sizeof (short);
  while (size > 0) {
    int len = SAMPLE_ARRAY_SIZE - vs->sample_array_index;
    if (len > size)
      len = size;
    memcpy (vs->sample_array + vs->sample_array_index, samples, len * sizeof(short));

    samples += len;
    vs->sample_array_index += len;
    if (vs->sample_array_index >= SAMPLE_ARRAY_SIZE)
      vs->sample_array_index = 0;
    size -= len;
    }
  }
//}}}
//{{{
static void sdlAudioCallback (void* opaqueVs, Uint8* stream, int len) {

  VideoState* vs = opaqueVs;
  audioCallbackTime = av_gettime_relative();

  while (len > 0) {
    if (vs->audio_buf_index >= (int)vs->audio_buf_size) {
      int audio_size = vs->paused ? -1 : audioDecodeFrame (vs);
      if (audio_size < 0) {
        vs->audio_buf = vs->silence_buf;
        vs->audio_buf_size = sizeof (vs->silence_buf) / vs->audio_tgt.frame_size * vs->audio_tgt.frame_size;
        }
      else {
        updateSamples (vs, (int16_t *)vs->audio_buf, audio_size);
        vs->audio_buf_size = audio_size;
        }
      vs->audio_buf_index = 0;
      }

    int len1 = vs->audio_buf_size - vs->audio_buf_index;
    if (len1 > len)
      len1 = len;
    memcpy (stream, (uint8_t*)vs->audio_buf + vs->audio_buf_index, len1);

    len -= len1;
    stream += len1;
    vs->audio_buf_index += len1;
    }
  vs->audio_write_buf_size = vs->audio_buf_size - vs->audio_buf_index;

  /* Let's assume the audio driver that is used by SDL has two periods. */
  if (!isnan (vs->audio_clock)) {
    set_clock_at (&vs->audclk,
                  vs->audio_clock - (double)(2 * vs->audio_hw_buf_size + vs->audio_write_buf_size) / vs->audio_tgt.bytes_per_sec, vs->audio_clock_serial,
                  audioCallbackTime / 1000000.0);
    sync_clock_to_slave (&vs->extclk, &vs->audclk);
    }
  }
//}}}
//{{{
static int audioOpen (VideoState* vs, int64_t wanted_channel_layout, int wanted_num_channels,
                      int wanted_sample_rate, struct AudioParams* audio_hw_params) {

  static const int next_num_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
  static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
  int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

  const char* env= SDL_getenv ("SDL_AUDIO_CHANNELS");
  if (env) {
    wanted_num_channels = atoi (env);
    wanted_channel_layout = av_get_default_channel_layout (wanted_num_channels);
    }

  if (!wanted_channel_layout || wanted_num_channels != av_get_channel_layout_nb_channels (wanted_channel_layout)) {
    wanted_channel_layout = av_get_default_channel_layout (wanted_num_channels);
    wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }

  wanted_num_channels = av_get_channel_layout_nb_channels (wanted_channel_layout);

  SDL_AudioSpec wanted_spec;
  wanted_spec.channels = wanted_num_channels;
  wanted_spec.freq = wanted_sample_rate;
  if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
    av_log (NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
    return -1;
    }
  while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
    next_sample_rate_idx--;

  wanted_spec.format = AUDIO_S16SYS;
  wanted_spec.silence = 0;
  wanted_spec.samples = FFMAX (SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2 (wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
  wanted_spec.callback = sdlAudioCallback;
  wanted_spec.userdata = vs;

  SDL_AudioSpec spec;
  while (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
    av_log (NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
            wanted_spec.channels, wanted_spec.freq, SDL_GetError());
    wanted_spec.channels = next_num_channels[FFMIN (7, wanted_spec.channels)];
    if (!wanted_spec.channels) {
      wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
      wanted_spec.channels = wanted_num_channels;
      if (!wanted_spec.freq) {
        av_log (NULL, AV_LOG_ERROR, "No more combinations to try, audio open failed\n");
        return -1;
        }
      }
    wanted_channel_layout = av_get_default_channel_layout (wanted_spec.channels);
    }

  if (spec.format != AUDIO_S16SYS) {
    av_log (NULL, AV_LOG_ERROR, "SDL advised audio format %d is not supported!\n", spec.format);
    return -1;
    }

  if (spec.channels != wanted_spec.channels) {
    wanted_channel_layout = av_get_default_channel_layout (spec.channels);
    if (!wanted_channel_layout) {
      av_log (NULL, AV_LOG_ERROR, "SDL advised channel count %d is not supported!\n", spec.channels);
      return -1;
      }
    }

  audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
  audio_hw_params->freq = spec.freq;
  audio_hw_params->channel_layout = wanted_channel_layout;
  audio_hw_params->channels =  spec.channels;
  audio_hw_params->frame_size = av_samples_get_buffer_size (NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
  audio_hw_params->bytes_per_sec = av_samples_get_buffer_size (NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
  if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
    av_log (NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
    return -1;
    }

  return spec.size;
  }
//}}}

// video
//{{{
static void calcDisplayRect (SDL_Rect* rect,
                             int scr_xleft, int scr_ytop, int scr_width, int scr_height,
                             int pic_width, int pic_height, AVRational pic_sar) {


  float aspect_ratio = (pic_sar.num == 0) ? 0.0F : (float)av_q2d (pic_sar);
  if (aspect_ratio <= 0.0F)
    aspect_ratio = 1.0F;
  aspect_ratio *= (float)pic_width / (float)pic_height;

  int height = scr_height;
  int width = ((int)rint (height * aspect_ratio)) & ~1;
  if (width > scr_width) {
    width = scr_width;
    height = ((int)rint (width / aspect_ratio)) & ~1;
    }

  int x = (scr_width - width) / 2;
  int y = (scr_height - height) / 2;

  rect->x = scr_xleft + x;
  rect->y = scr_ytop  + y;
  rect->w = FFMAX (width,  1);
  rect->h = FFMAX (height, 1);
  }
//}}}
//{{{
static void setDefaultWindowSize (int width, int height, AVRational sar) {

  SDL_Rect rect;
  calcDisplayRect (&rect, 0, 0, INT_MAX, height, width, height, sar);

  default_width  = rect.w;
  default_height = rect.h;
  }
//}}}
//{{{
static int videoOpen (VideoState* vs, int force_set_video_mode, Frame* vp) {

  if (vp && vp->width)
    setDefaultWindowSize (vp->width, vp->height, vp->sar);

  int w,h;
  if (is_full_screen && fs_screen_width) {
    w = fs_screen_width;
    h = fs_screen_height;
    }
  else if (!is_full_screen && screen_width) {
    w = screen_width;
    h = screen_height;
    }
  else {
    w = default_width;
    h = default_height;
    }

  w = FFMIN (16383, w);
  if (!screen || force_set_video_mode ||
      vs->width != screen->w || screen->w != w ||
      vs->height != screen->h || screen->h != h) {

    int flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | ((is_full_screen) ? SDL_FULLSCREEN : SDL_RESIZABLE);
    screen = SDL_SetVideoMode (w, h, 0, flags);

    vs->width  = screen->w;
    vs->height = screen->h;
    }

  return 0;
  }
//}}}
//{{{
static void fillRectangle (SDL_Surface* screen, int x, int y, int w, int h, int color, int update) {

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;

  SDL_FillRect (screen, &rect, color);

  if (update && w > 0 && h > 0)
    SDL_UpdateRect (screen, x, y, w, h);
  }
//}}}
//{{{
static void fillBorder (int xleft, int ytop, int width, int height,
                        int x, int y, int w, int h, int color, int update) {

  int w1 = (x < 0) ? 0 : x;
  int w2 = ((width - (x + w)) < 0) ? 0 : width - (x + w);
  int h1 = (y < 0) ? 0 : y;
  int h2 = ((height - (y + h)) < 0) ? 0 : height - (y + h);

  fillRectangle (screen, xleft, ytop, w1, height, color, update);
  fillRectangle(screen, xleft + width - w2, ytop, w2, height, color, update);
  fillRectangle(screen, xleft + w1, ytop, width - w1 - w2, h1, color, update);
  fillRectangle(screen, xleft + w1, ytop + height - h2, width - w1 - w2, h2, color, update);
  }
//}}}
//{{{
static void blendSubtitle (AVPicture* dst, const AVSubtitleRect* avSubtitleRect, int imgw, int imgh) {

  int x, y, Y, U, V, A;
  uint8_t* lum,*cb, *cr;
  int dstx, dsty, dstw, dsth;

  const AVPicture* srcAVPicture = &avSubtitleRect->pict;

  dstw = av_clip (avSubtitleRect->w, 0, imgw);
  dsth = av_clip (avSubtitleRect->h, 0, imgh);
  dstx = av_clip (avSubtitleRect->x, 0, imgw - dstw);
  dsty = av_clip (avSubtitleRect->y, 0, imgh - dsth);

  lum = dst->data[0] + dstx + dsty * dst->linesize[0];
  cb  = dst->data[1] + dstx/2 + (dsty >> 1) * dst->linesize[1];
  cr  = dst->data[2] + dstx/2 + (dsty >> 1) * dst->linesize[2];

  for (y = 0; y<dsth; y++) {
    for (x = 0; x<dstw; x++) {
      Y = srcAVPicture->data[0][x + y*srcAVPicture->linesize[0]];
      A = srcAVPicture->data[3][x + y*srcAVPicture->linesize[3]];
      lum[0] = ALPHA_BLEND(A, lum[0], Y, 0);
      lum++;
      }
    lum += dst->linesize[0] - dstw;
    }

  for (y = 0; y<dsth/2; y++) {
    for (x = 0; x<dstw/2; x++) {
      U = srcAVPicture->data[1][x + y * srcAVPicture->linesize[1]];
      V = srcAVPicture->data[2][x + y * srcAVPicture->linesize[2]];
      A = srcAVPicture->data[3][2*x + 2*y * srcAVPicture->linesize[3]]
        + srcAVPicture->data[3][2*x + 1 + 2*y * srcAVPicture->linesize[3]]
        + srcAVPicture->data[3][2*x + 1 + (2*y+1) * srcAVPicture->linesize[3]]
        + srcAVPicture->data[3][2*x + (2*y+1) * srcAVPicture->linesize[3]];
      cb[0] = ALPHA_BLEND (A>>2, cb[0], U, 0);
      cr[0] = ALPHA_BLEND (A>>2, cr[0], V, 0);
      cb++;
      cr++;
      }

    cb += dst->linesize[1] - dstw/2;
    cr += dst->linesize[2] - dstw/2;
    }
  }
//}}}
//{{{
static void videoImageDisplay (VideoState* vs) {

  Frame* vp = frame_queue_peek (&vs->pictq);
  if (vp->bmp) {
    if (vs->subtitle_st) {
      if (frame_queue_num_remaining (&vs->subpq) > 0) {
        Frame* sp = frame_queue_peek (&vs->subpq);

        if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) {
          SDL_LockYUVOverlay (vp->bmp);

          AVPicture pict;
          pict.data[0] = vp->bmp->pixels[0];
          pict.data[1] = vp->bmp->pixels[2];
          pict.data[2] = vp->bmp->pixels[1];

          pict.linesize[0] = vp->bmp->pitches[0];
          pict.linesize[1] = vp->bmp->pitches[2];
          pict.linesize[2] = vp->bmp->pitches[1];

          for (unsigned int i = 0; i < sp->sub.num_rects; i++)
            blendSubtitle (&pict, sp->sub.rects[i], vp->bmp->w, vp->bmp->h);

          SDL_UnlockYUVOverlay (vp->bmp);
          }
        }
      }

    SDL_Rect rect;
    calcDisplayRect(&rect, vs->xleft, vs->ytop, vs->width, vs->height, vp->width, vp->height, vp->sar);
    SDL_DisplayYUVOverlay (vp->bmp, &rect);

    if (rect.x != vs->last_display_rect.x || rect.y != vs->last_display_rect.y ||
        rect.w != vs->last_display_rect.w || rect.h != vs->last_display_rect.h ||
        vs->force_refresh) {
      int bgcolor = SDL_MapRGB (screen->format, 0x00, 0x00, 0x00);
      fillBorder (vs->xleft, vs->ytop, vs->width, vs->height, rect.x, rect.y, rect.w, rect.h, bgcolor, 1);
      vs->last_display_rect = rect;
      }
    }
  }
//}}}
//{{{
static void videoAudioDisplay (VideoState* vs) {

  int i, i_start, x, y1, y, ys, delay, n, num_display_channels;
  int ch, channels, h, h2, fgcolor;

  int rdft_bits;
  for (rdft_bits = 1; (1 << rdft_bits) < 2 * vs->height; rdft_bits++);
  int num_freq = 1 << (rdft_bits - 1);

  // compute display index, center on current output
  channels = vs->audio_tgt.channels;
  num_display_channels = channels;
  if (!vs->paused) {
    int data_used = vs->show_mode == SHOW_MODE_WAVES ? vs->width : (2*num_freq);
    n = 2 * channels;
    delay = vs->audio_write_buf_size;
    delay /= n;

    // to be more precise, we take into account the time spent since the last buffer computation
    if (audioCallbackTime) {
      int64_t time_diff = av_gettime_relative() - audioCallbackTime;
      delay -= (int)((time_diff * vs->audio_tgt.freq) / 1000000);
      }

    delay += 2 * data_used;
    if (delay < data_used)
      delay = data_used;

    int a = vs->sample_array_index - delay * channels;
    i_start = (a < 0) ? (a % SAMPLE_ARRAY_SIZE + SAMPLE_ARRAY_SIZE) : (a % SAMPLE_ARRAY_SIZE);
    x = i_start;
    if (vs->show_mode == SHOW_MODE_WAVES) {
      h = INT_MIN;
      for (i = 0; i < 1000; i += channels) {
        int idx = (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
        int a = vs->sample_array[idx];
        int b = vs->sample_array[(idx + 4 * channels) % SAMPLE_ARRAY_SIZE];
        int c = vs->sample_array[(idx + 5 * channels) % SAMPLE_ARRAY_SIZE];
        int d = vs->sample_array[(idx + 9 * channels) % SAMPLE_ARRAY_SIZE];
        int score = a - d;
        if (h < score && (b ^ c) < 0) {
          h = score;
          i_start = idx;
          }
        }
      }
    vs->last_i_start = i_start;
    }
  else
    i_start = vs->last_i_start;

  if (vs->show_mode == SHOW_MODE_WAVES) {
    //{{{  show waves
    fgcolor = SDL_MapRGB (screen->format, 0xff, 0xff, 0xff);

    /* total height for one channel */
    h = vs->height / num_display_channels;
    /* graph height / 2 */
    h2 = (h * 9) / 20;

    for (ch = 0; ch < num_display_channels; ch++) {
      i = i_start + ch;
      y1 = vs->ytop + ch * h + (h / 2); /* position of center line */
      for (x = 0; x < vs->width; x++) {
        y = (vs->sample_array[i] * h2) >> 15;
        if (y < 0) {
          y = -y;
          ys = y1 - y;
          }
        else
          ys = y1;

        fillRectangle(screen, vs->xleft + x, ys, 1, y, fgcolor, 0);
        i += channels;
        if (i >= SAMPLE_ARRAY_SIZE)
          i -= SAMPLE_ARRAY_SIZE;
        }
      }

    fgcolor = SDL_MapRGB (screen->format, 0x00, 0x00, 0xff);

    for (ch = 1; ch < num_display_channels; ch++) {
      y = vs->ytop + ch * h;
      fillRectangle(screen, vs->xleft, y, vs->width, 1, fgcolor, 0);
      }

    SDL_UpdateRect (screen, vs->xleft, vs->ytop, vs->width, vs->height);
    }
    //}}}
  else {
    //{{{  show rdft
    num_display_channels = FFMIN (num_display_channels, 2);

    if (rdft_bits != vs->rdft_bits) {
      av_rdft_end (vs->rdft);
      av_free (vs->rdft_data);

      vs->rdft = av_rdft_init (rdft_bits, DFT_R2C);
      vs->rdft_bits = rdft_bits;
      vs->rdft_data = av_malloc_array (num_freq, 4 *sizeof(*vs->rdft_data));
      }

    if (!vs->rdft || !vs->rdft_data) {
      av_log (NULL, AV_LOG_ERROR, "Failed to allocate buffers for RDFT, switching to waves display\n");
      vs->show_mode = SHOW_MODE_WAVES;
      }

    else {
      FFTSample* data[2];
      for (ch = 0; ch < num_display_channels; ch++) {
        data[ch] = vs->rdft_data + 2 * num_freq * ch;
        i = i_start + ch;
        for (x = 0; x < 2 * num_freq; x++) {
          float w = (x-num_freq) * (1.0f / num_freq);
          data[ch][x] = vs->sample_array[i] * (1.0f - w * w);
          i += channels;
          if (i >= SAMPLE_ARRAY_SIZE)
            i -= SAMPLE_ARRAY_SIZE;
          }
        av_rdft_calc (vs->rdft, data[ch]);
        }

      // inefficient way, we should directly access it but it is more than fast enough
      for (y = 0; y < vs->height; y++) {
        double w = 1 / sqrt (num_freq);
        int a = (int)sqrt(w * sqrt(data[0][2 * y + 0] * data[0][2 * y + 0] +
                              data[0][2 * y + 1] * data[0][2 * y + 1]));
        int b = (num_display_channels == 2 ) ?
                  (int)sqrt(w * sqrt(data[1][2 * y + 0] * data[1][2 * y + 0] +
                                data[1][2 * y + 1] * data[1][2 * y + 1])) : a;
        a = FFMIN (a, 255);
        b = FFMIN (b, 255);
        fgcolor = SDL_MapRGB (screen->format, a, b, (a + b) / 2);

        fillRectangle(screen, vs->xpos, vs->height-y, 1, 1, fgcolor, 0);
        }
      }

    SDL_UpdateRect (screen, vs->xpos, vs->ytop, 1, vs->height);

    if (!vs->paused)
      vs->xpos++;

    if (vs->xpos >= vs->width)
      vs->xpos= vs->xleft;
    }
    //}}}
  }
//}}}
//{{{
static void videoDisplay (VideoState* vs) {

  if (!screen)
    videoOpen (vs, 0, NULL);

  if (vs->video_st && vs->show_mode != SHOW_MODE_RDFT)
    videoImageDisplay (vs);
  else
    fillRectangle (screen, vs->xleft, vs->ytop, vs->width, vs->height,
                   SDL_MapRGB (screen->format, 0x00, 0x00, 0x00), 0);

  if (vs->audio_st && vs->show_mode != SHOW_MODE_VIDEO)
    videoAudioDisplay (vs);

  if (font) {
    SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };
    SDL_Color black = { 0x00, 0x00, 0x00, 0 };
    SDL_Surface* text = TTF_RenderText_Solid (font, infoStr, white);

    SDL_Rect dstrect;
    dstrect.x = 4;
    dstrect.y = 4;
    dstrect.w = text->w;
    dstrect.h = text->h;
    SDL_BlitSurface (text, NULL, screen, &dstrect);
    SDL_UpdateRects (screen, 1, &dstrect);

    SDL_FreeSurface (text);
    }
  }
//}}}
//{{{
static void allocPicture (VideoState* vs) {

  Frame* vp = &vs->pictq.queue[vs->pictq.windex];
  SDL_FreeYUVOverlay (vp->bmp);

  videoOpen (vs, 0, vp);
  vp->bmp = SDL_CreateYUVOverlay (vp->width, vp->height, SDL_YV12_OVERLAY, screen);

  SDL_LockMutex (vs->pictq.mutex);
  vp->allocated = 1;
  SDL_CondSignal (vs->pictq.cond);
  SDL_UnlockMutex (vs->pictq.mutex);
  }
//}}}
//{{{
static int queuePicture (VideoState* vs, AVFrame* srcAVframe,
                         double pts, double duration, int64_t pos, int serial) {

  Frame* frame;
  if (!(frame = frame_queue_peek_writable (&vs->pictq)))
    return -1;

  frame->sar = srcAVframe->sample_aspect_ratio;

  if (!frame->bmp || frame->reallocate || !frame->allocated ||
      (frame->width  != srcAVframe->width) || (frame->height != srcAVframe->height)) {
    //{{{  alloc pic
    frame->allocated  = 0;
    frame->reallocate = 0;
    frame->width = srcAVframe->width;
    frame->height = srcAVframe->height;

    // allocate in main thread to avoid locking problems
    SDL_Event event;
    event.type = FF_ALLOC_EVENT;
    event.user.data1 = vs;
    SDL_PushEvent (&event);

    SDL_LockMutex (vs->pictq.mutex);
    // wait until the picture is allocated
    while (!frame->allocated && !vs->videoq.abort_request)
      SDL_CondWait (vs->pictq.cond, vs->pictq.mutex);

    if (vs->videoq.abort_request && SDL_PeepEvents (&event, 1, SDL_GETEVENT, SDL_EVENTMASK(FF_ALLOC_EVENT)) != 1)
      // if the queue is aborted, we have to pop the pending ALLOC event or wait for the allocation to complete
      while (!frame->allocated && !vs->abort_request)
        SDL_CondWait (vs->pictq.cond, vs->pictq.mutex);
    SDL_UnlockMutex (vs->pictq.mutex);

    if (vs->videoq.abort_request)
      return -1;
    }
    //}}}

  if (frame->bmp) {
    SDL_LockYUVOverlay (frame->bmp);

    AVPicture pict = { { 0 } };
    pict.data[0] = frame->bmp->pixels[0];
    pict.data[1] = frame->bmp->pixels[2];
    pict.data[2] = frame->bmp->pixels[1];

    pict.linesize[0] = frame->bmp->pitches[0];
    pict.linesize[1] = frame->bmp->pitches[2];
    pict.linesize[2] = frame->bmp->pitches[1];

    av_picture_copy (&pict, (AVPicture*)srcAVframe, srcAVframe->format, frame->width, frame->height);
    SDL_UnlockYUVOverlay (frame->bmp);

    frame->pts = pts;
    frame->duration = duration;
    frame->pos = pos;
    frame->serial = serial;

    frame_queue_push (&vs->pictq);
    }

  return 0;
  }
//}}}

// stream
//{{{
static int decodeFrame (Decoder* decoder, AVFrame* avFrame, AVSubtitle* avSubtitle) {

  int gotFrame = 0;
  do {
    int ret = -1;
    if (decoder->queue->abort_request)
      return -1;

    if (!decoder->packet_pending || decoder->queue->serial != decoder->pkt_serial) {
      AVPacket pkt;
      do {
        if (decoder->queue->num_packets == 0)
          SDL_CondSignal (decoder->empty_queue_cond);
        if (packet_queue_get (decoder->queue, &pkt, 1, &decoder->pkt_serial) < 0)
          return -1;
        if (pkt.data == flush_pkt.data) {
          avcodec_flush_buffers (decoder->avctx);
          decoder->finished = 0;
          decoder->next_pts = decoder->start_pts;
          decoder->next_pts_tb = decoder->start_pts_tb;
          }
        } while (pkt.data == flush_pkt.data || decoder->queue->serial != decoder->pkt_serial);
      av_free_packet (&decoder->pkt);
      decoder->pkt_temp = decoder->pkt = pkt;
      decoder->packet_pending = 1;
      }

    switch (decoder->avctx->codec_type) {
      case AVMEDIA_TYPE_VIDEO:
        //{{{  decode video
        ret = avcodec_decode_video2 (decoder->avctx, avFrame, &gotFrame, &decoder->pkt_temp);
        if (gotFrame)
          avFrame->pts = av_frame_get_best_effort_timestamp (avFrame);

        break;
        //}}}
      case AVMEDIA_TYPE_AUDIO:
        //{{{  decode audio
        ret = avcodec_decode_audio4 (decoder->avctx, avFrame, &gotFrame, &decoder->pkt_temp);

        if (gotFrame) {
          AVRational tb = (AVRational){1, avFrame->sample_rate};
          if (avFrame->pts != AV_NOPTS_VALUE)
            avFrame->pts = av_rescale_q (avFrame->pts, decoder->avctx->time_base, tb);

          else if (avFrame->pkt_pts != AV_NOPTS_VALUE)
            avFrame->pts = av_rescale_q (avFrame->pkt_pts, av_codec_get_pkt_timebase (decoder->avctx), tb);

          else if (decoder->next_pts != AV_NOPTS_VALUE)
            avFrame->pts = av_rescale_q (decoder->next_pts, decoder->next_pts_tb, tb);

          if (avFrame->pts != AV_NOPTS_VALUE) {
            decoder->next_pts = avFrame->pts + avFrame->nb_samples;
            decoder->next_pts_tb = tb;
            }
          }

        break;
        //}}}
      case AVMEDIA_TYPE_SUBTITLE:
        //{{{  decode subtitle
        ret = avcodec_decode_subtitle2 (decoder->avctx, avSubtitle, &gotFrame, &decoder->pkt_temp);

        break;
        //}}}
      }

    if (ret < 0)
      decoder->packet_pending = 0;
    else {
      decoder->pkt_temp.dts = AV_NOPTS_VALUE;
      decoder->pkt_temp.pts = AV_NOPTS_VALUE;
      if (decoder->pkt_temp.data) {
        if (decoder->avctx->codec_type != AVMEDIA_TYPE_AUDIO)
          ret = decoder->pkt_temp.size;
        decoder->pkt_temp.data += ret;
        decoder->pkt_temp.size -= ret;
        if (decoder->pkt_temp.size <= 0)
          decoder->packet_pending = 0;
        }
      else if (!gotFrame) {
        decoder->packet_pending = 0;
        decoder->finished = decoder->pkt_serial;
        }
      }
    } while (!gotFrame && !decoder->finished);

  return gotFrame;
  }
//}}}
//{{{
static int audioThread (void* opqaueVs) {

  VideoState* vs = opqaueVs;

  int last_serial = -1;

  AVFrame* frame = av_frame_alloc();
  if (!frame)
    return 0;

  int ret;
  do {
    int gotFrame = decodeFrame (&vs->auddec, frame, NULL);
    if (gotFrame < 0)
      goto exit;
    if (gotFrame) {
      AVRational tb = (AVRational){1, frame->sample_rate};

      int64_t dec_channel_layout = 0;
      if (frame->channel_layout &&
          av_get_channel_layout_nb_channels (frame->channel_layout) == av_frame_get_channels(frame))
        dec_channel_layout = frame->channel_layout;

      int reconfigure;
      if (vs->audio_filter_src.channels == 1 && av_frame_get_channels(frame) == 1)
        reconfigure = av_get_packed_sample_fmt (vs->audio_filter_src.fmt) != av_get_packed_sample_fmt (frame->format);
      else
        reconfigure = vs->audio_filter_src.channels != av_frame_get_channels (frame) ||
                      vs->audio_filter_src.fmt != frame->format;

      reconfigure |= vs->audio_filter_src.channel_layout != dec_channel_layout ||
                     vs->audio_filter_src.freq != frame->sample_rate ||
                     vs->auddec.pkt_serial != last_serial;

      if (reconfigure) {
        char buf1[1024], buf2[1024];
        av_get_channel_layout_string(buf1, sizeof(buf1), -1, vs->audio_filter_src.channel_layout);
        av_get_channel_layout_string(buf2, sizeof(buf2), -1, dec_channel_layout);
        av_log (NULL, AV_LOG_DEBUG,
                "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                vs->audio_filter_src.freq, vs->audio_filter_src.channels, av_get_sample_fmt_name(vs->audio_filter_src.fmt), buf1, last_serial,
                frame->sample_rate, av_frame_get_channels(frame), av_get_sample_fmt_name(frame->format), buf2, vs->auddec.pkt_serial);

        vs->audio_filter_src.fmt            = frame->format;
        vs->audio_filter_src.channels       = av_frame_get_channels(frame);
        vs->audio_filter_src.channel_layout = dec_channel_layout;
        vs->audio_filter_src.freq           = frame->sample_rate;
        last_serial                         = vs->auddec.pkt_serial;

        if (configAudioFilters (vs, afilters, 1) < 0)
          goto exit;
        }

      if (av_buffersrc_add_frame (vs->in_audio_filter, frame) < 0)
        goto exit;

      while ((ret = av_buffersink_get_frame_flags (vs->out_audio_filter, frame, 0)) >= 0) {
        tb = vs->out_audio_filter->inputs[0]->time_base;
        Frame* af = frame_queue_peek_writable (&vs->sampq);
        if (!af)
          goto exit;

        af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        af->pos = av_frame_get_pkt_pos (frame);
        af->serial = vs->auddec.pkt_serial;
        af->duration = av_q2d ((AVRational){frame->nb_samples, frame->sample_rate});

        av_frame_move_ref (af->frame, frame);
        frame_queue_push (&vs->sampq);

        if (vs->audioq.serial != vs->auddec.pkt_serial)
          break;
        }
      if (ret == AVERROR_EOF)
        vs->auddec.finished = vs->auddec.pkt_serial;
      }
    } while (ret >= 0 || ret == AVERROR (EAGAIN) || ret == AVERROR_EOF);

exit:
  avfilter_graph_free (&vs->agraph);
  av_frame_free (&frame);
  return 0;
  }
//}}}
//{{{
static int videoThread (void* opqaueVs) {

  AVFilterContext* filt_in = NULL;
  AVFilterContext* filt_out = NULL;
  int last_w = 0;
  int last_h = 0;
  enum AVPixelFormat last_format = -2;
  int last_serial = -1;
  int last_vfilter_idx = 0;

  VideoState* vs = opqaueVs;
  AVFrame* frame = av_frame_alloc();
  AVFilterGraph *graph = avfilter_graph_alloc();
  AVRational tb = vs->video_st->time_base;
  AVRational frame_rate = av_guess_frame_rate (vs->ic, vs->video_st, NULL);

  int ret = 0;
  for (;;) {
    if (decodeFrame (&vs->viddec, frame, NULL) < 0)
      goto exit;

    double dpts = NAN;
    if (frame->pts != AV_NOPTS_VALUE)
      dpts = av_q2d (vs->video_st->time_base) * frame->pts;

    frame->sample_aspect_ratio = av_guess_sample_aspect_ratio (vs->ic, vs->video_st, frame);
    vs->viddec_width  = frame->width;
    vs->viddec_height = frame->height;

    if (frame->pts != AV_NOPTS_VALUE) {
      double diff = dpts - get_master_clock (vs);
      if (!isnan (diff) &&
          (fabs (diff) < AV_NOSYNC_THRESHOLD) &&
          (diff - vs->frame_last_filter_delay < 0) &&
          (vs->viddec.pkt_serial == vs->vidclk.serial) &&
          vs->videoq.num_packets) {
        vs->frame_drops_early++;
        av_frame_unref (frame);
        continue;
        }
      }

    if (last_w != frame->width || last_h != frame->height ||
        last_format != frame->format ||
        last_serial != vs->viddec.pkt_serial ||
        last_vfilter_idx != vs->vfilter_idx) {
      //{{{  video changed size
      av_log (NULL, AV_LOG_DEBUG,
              "Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",
              last_w, last_h,
              (const char *)av_x_if_null (av_get_pix_fmt_name(last_format), "none"), last_serial,
              frame->width, frame->height,
              (const char *)av_x_if_null (av_get_pix_fmt_name (frame->format), "none"), vs->viddec.pkt_serial);
      avfilter_graph_free (&graph);
      graph = avfilter_graph_alloc();
      if (configVideoFilters (graph, vs, vfilters_list ? vfilters_list[vs->vfilter_idx] : NULL, frame) < 0) {
        SDL_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = vs;
        SDL_PushEvent (&event);
        goto exit;
        }

      filt_in  = vs->in_video_filter;
      filt_out = vs->out_video_filter;
      last_w = frame->width;
      last_h = frame->height;
      last_format = frame->format;
      last_serial = vs->viddec.pkt_serial;
      last_vfilter_idx = vs->vfilter_idx;
      frame_rate = filt_out->inputs[0]->frame_rate;
      }
      //}}}

    if (av_buffersrc_add_frame (filt_in, frame) < 0)
      goto exit;

    while (ret >= 0) {
      vs->frame_last_returned_time = av_gettime_relative() / 1000000.0;
      ret = av_buffersink_get_frame_flags (filt_out, frame, 0);
      if (ret < 0) {
        if (ret == AVERROR_EOF)
          vs->viddec.finished = vs->viddec.pkt_serial;
        ret = 0;
        break;
        }

      vs->frame_last_filter_delay = av_gettime_relative() / 1000000.0 - vs->frame_last_returned_time;
      if (fabs(vs->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
        vs->frame_last_filter_delay = 0;

      tb = filt_out->inputs[0]->time_base;
      double duration = (frame_rate.num && frame_rate.den ? av_q2d ((AVRational){frame_rate.den, frame_rate.num}) : 0);
      double pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
      ret = queuePicture (vs, frame, pts, duration, av_frame_get_pkt_pos(frame), vs->viddec.pkt_serial);
      av_frame_unref (frame);
      }

    if (ret < 0)
      goto exit;
    }

exit:
  avfilter_graph_free (&graph);
  av_frame_free (&frame);
  return 0;
  }
//}}}
//{{{
static int subtitleThread (void* opqaueVs) {

  VideoState* vs = opqaueVs;
  for (;;) {
    Frame* sp = frame_queue_peek_writable (&vs->subpq);
    if (!sp)
      return 0;

    int got_subtitle = decodeFrame (&vs->subdec, NULL, &sp->sub);
    if (got_subtitle < 0)
      break;

    double pts = 0;
    if (got_subtitle && sp->sub.format == 0) {
      if (sp->sub.pts != AV_NOPTS_VALUE)
        pts = sp->sub.pts / (double)AV_TIME_BASE;
      sp->pts = pts;
      sp->serial = vs->subdec.pkt_serial;

      for (unsigned int i = 0; i < sp->sub.num_rects; i++) {
        int in_w = sp->sub.rects[i]->w;
        int in_h = sp->sub.rects[i]->h;
        int subw = vs->subdec.avctx->width  ? vs->subdec.avctx->width  : vs->viddec_width;
        int subh = vs->subdec.avctx->height ? vs->subdec.avctx->height : vs->viddec_height;
        int out_w = vs->viddec_width  ? in_w * vs->viddec_width  / subw : in_w;
        int out_h = vs->viddec_height ? in_h * vs->viddec_height / subh : in_h;

        AVPicture newpic;
        av_image_fill_linesizes (newpic.linesize, AV_PIX_FMT_YUVA420P, out_w);
        newpic.data[0] = av_malloc(newpic.linesize[0] * out_h);
        newpic.data[3] = av_malloc(newpic.linesize[3] * out_h);
        newpic.data[1] = av_malloc(newpic.linesize[1] * ((out_h+1)/2));
        newpic.data[2] = av_malloc(newpic.linesize[2] * ((out_h+1)/2));

        vs->subSwsContext = sws_getCachedContext (vs->subSwsContext,
            in_w, in_h, AV_PIX_FMT_PAL8, out_w, out_h,
            AV_PIX_FMT_YUVA420P, SWS_BICUBIC, NULL, NULL, NULL);
        if (!vs->subSwsContext || !newpic.data[0] || !newpic.data[3] ||
            !newpic.data[1] || !newpic.data[2]) {
          av_log (NULL, AV_LOG_FATAL, "Cannot initialize the sub conversion context\n");
          exit (1);
          }

        sws_scale (vs->subSwsContext,
                   (void*)sp->sub.rects[i]->pict.data, sp->sub.rects[i]->pict.linesize,
                   0, in_h, newpic.data, newpic.linesize);

        av_free(sp->sub.rects[i]->pict.data[0]);
        av_free(sp->sub.rects[i]->pict.data[1]);
        sp->sub.rects[i]->pict = newpic;
        sp->sub.rects[i]->w = out_w;
        sp->sub.rects[i]->h = out_h;
        sp->sub.rects[i]->x = sp->sub.rects[i]->x * out_w / in_w;
        sp->sub.rects[i]->y = sp->sub.rects[i]->y * out_h / in_h;
        }

      // now we can update the picture count */
      frame_queue_push (&vs->subpq);
      }
    else if (got_subtitle)
      avsubtitle_free (&sp->sub);
    }

  return 0;
  }
//}}}
//{{{
static void subStreamOpen (VideoState* vs, int streamIndex) {

  AVFormatContext* ic = vs->ic;
  if ((streamIndex < 0) || (streamIndex >= (int)ic->nb_streams))
    return;

  AVCodecContext* avctx = ic->streams[streamIndex]->codec;
  switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      vs->last_audio_stream = streamIndex;
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      vs->last_subtitle_stream = streamIndex;
      break;
    case AVMEDIA_TYPE_VIDEO:
      vs->last_video_stream = streamIndex;
      break;
    }

  AVCodec* codec = avcodec_find_decoder (avctx->codec_id);
  if (!codec) {
    av_log (NULL, AV_LOG_WARNING, "No codec could be found with id %d\n", avctx->codec_id);
    return;
    }

  avctx->codec_id = codec->id;

  AVDictionary* opts = NULL;
  if (!av_dict_get (opts, "threads", NULL, 0))
    av_dict_set (&opts, "threads", "auto", 0);
  if (avctx->codec_type == AVMEDIA_TYPE_VIDEO ||
      avctx->codec_type == AVMEDIA_TYPE_AUDIO)
    av_dict_set (&opts, "refcounted_frames", "1", 0);

  if (avcodec_open2 (avctx, codec, &opts) < 0)
    goto exit;

  AVDictionaryEntry* t = NULL;
  if (t = av_dict_get (opts, "", NULL, AV_DICT_IGNORE_SUFFIX)) {
    av_log (NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
    goto exit;
    }

  vs->eof = 0;
  ic->streams[streamIndex]->discard = AVDISCARD_DEFAULT;
  switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO: {
      //{{{  init audio thread
      vs->audio_filter_src.freq = avctx->sample_rate;
      vs->audio_filter_src.channels = avctx->channels;
      vs->audio_filter_src.channel_layout = 0;
      if (avctx->channel_layout &&
          av_get_channel_layout_nb_channels (avctx->channel_layout) == avctx->channels)
        vs->audio_filter_src.channel_layout = avctx->channel_layout;
      vs->audio_filter_src.fmt = avctx->sample_fmt;
      if (configAudioFilters (vs, afilters, 0) < 0)
        goto exit;

      AVFilterLink* link = vs->out_audio_filter->inputs[0];
      int sample_rate = link->sample_rate;
      int num_channels = link->channels;
      int64_t channel_layout = link->channel_layout;

      /* prepare audio output */
      vs->audio_hw_buf_size = audioOpen (vs, channel_layout, num_channels, sample_rate, &vs->audio_tgt);
      vs->audio_src = vs->audio_tgt;
      vs->audio_buf_size  = 0;
      vs->audio_buf_index = 0;

      /* init averaging filter */
      vs->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
      vs->audio_diff_avg_count = 0;
      /* since we do not have a precise anough audio fifo fullness,
         we correct audio sync only if larger than this threshold */
      vs->audio_diff_threshold = (double)(vs->audio_hw_buf_size) / vs->audio_tgt.bytes_per_sec;
      vs->audio_stream = streamIndex;
      vs->audio_st = ic->streams[streamIndex];

      memset (&vs->auddec, 0, sizeof (Decoder));
      vs->auddec.avctx = avctx;
      vs->auddec.empty_queue_cond = vs->continueReadThread;
      vs->auddec.start_pts = AV_NOPTS_VALUE;
      vs->auddec.queue = &vs->audioq;
      packet_queue_start (vs->auddec.queue);
      vs->auddec.decoder_tid = SDL_CreateThread (audioThread, vs);
      SDL_PauseAudio (0);
      break;
      }
      //}}}
    case AVMEDIA_TYPE_VIDEO:
      //{{{  init video thread
      vs->video_stream = streamIndex;
      vs->video_st = ic->streams[streamIndex];

      vs->viddec_width  = avctx->width;
      vs->viddec_height = avctx->height;

      memset (&vs->viddec, 0, sizeof (Decoder));
      vs->viddec.avctx = avctx;
      vs->viddec.empty_queue_cond = vs->continueReadThread;
      vs->viddec.start_pts = AV_NOPTS_VALUE;

      vs->viddec.queue = &vs->videoq;
      packet_queue_start (vs->viddec.queue);

      vs->viddec.decoder_tid = SDL_CreateThread (videoThread, vs);
      break;
      //}}}
    case AVMEDIA_TYPE_SUBTITLE:
      //{{{  init subtitle thread
      vs->subtitle_stream = streamIndex;
      vs->subtitle_st = ic->streams[streamIndex];

      memset (&vs->subdec, 0, sizeof (Decoder));
      vs->subdec.avctx = avctx;
      vs->subdec.empty_queue_cond = vs->continueReadThread;
      vs->subdec.start_pts = AV_NOPTS_VALUE;

      vs->subdec.queue = &vs->subtitleq;
      packet_queue_start (vs->subdec.queue);

      vs->subdec.decoder_tid = SDL_CreateThread (subtitleThread, vs);
      break;
      //}}}
    }

exit:
  av_dict_free (&opts);
  }
//}}}
//{{{
static void decoderAbort (Decoder* decoder, FrameQueue* frameQueue) {

  packet_queue_abort (decoder->queue);
  frame_queue_signal (frameQueue);

  SDL_WaitThread (decoder->decoder_tid, NULL);

  decoder->decoder_tid = NULL;
  packet_queue_flush (decoder->queue);
  }
//}}}
//{{{
static void subStreamClose (VideoState* vs, int streamIndex) {

  AVFormatContext* ic = vs->ic;
  if (streamIndex < 0 || streamIndex >= (int)ic->nb_streams)
    return;

  AVCodecContext* avctx = ic->streams[streamIndex]->codec;

  switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      decoderAbort (&vs->auddec, &vs->sampq);

      SDL_CloseAudio();

      av_free_packet (&vs->auddec.pkt);
      swr_free (&vs->swr_ctx);

      av_freep (&vs->audio_buf1);
      vs->audio_buf1_size = 0;
      vs->audio_buf = NULL;

      if (vs->rdft) {
        av_rdft_end (vs->rdft);
        av_freep (&vs->rdft_data);
        vs->rdft = NULL;
        vs->rdft_bits = 0;
        }
      break;

    case AVMEDIA_TYPE_VIDEO:
      decoderAbort (&vs->viddec, &vs->pictq);
      av_free_packet (&vs->viddec.pkt);
      break;

    case AVMEDIA_TYPE_SUBTITLE:
      decoderAbort(&vs->subdec, &vs->subpq);
      av_free_packet (&vs->subdec.pkt);
      break;
    }

  ic->streams[streamIndex]->discard = AVDISCARD_ALL;
  avcodec_close (avctx);
  switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      vs->audio_st = NULL;
      vs->audio_stream = -1;
      break;

    case AVMEDIA_TYPE_VIDEO:
      vs->video_st = NULL;
      vs->video_stream = -1;
      break;

    case AVMEDIA_TYPE_SUBTITLE:
      vs->subtitle_st = NULL;
      vs->subtitle_stream = -1;
      break;
    }
  }
//}}}

//{{{
static void streamSeek (VideoState* vs, int64_t pos, int64_t rel) {

  if (!vs->seek_req) {
    vs->seek_pos = pos;
    vs->seek_rel = rel;
    vs->seek_req = 1;

    SDL_CondSignal (vs->continueReadThread);
    }
  }
//}}}
//{{{
static void streamTogglePause (VideoState* vs) {

  if (vs->paused) {
    vs->frame_timer += av_gettime_relative() / 1000000.0 - vs->vidclk.last_updated;
    if (vs->read_pause_return != AVERROR (ENOSYS))
      vs->vidclk.paused = 0;
    set_clock (&vs->vidclk, get_clock (&vs->vidclk), vs->vidclk.serial);
    }

  set_clock (&vs->extclk, get_clock (&vs->extclk), vs->extclk.serial);

  vs->paused = vs->audclk.paused = vs->vidclk.paused = vs->extclk.paused = !vs->paused;
  }
//}}}
//{{{
static void stepNextFrame (VideoState* vs) {

  if (vs->paused)
    streamTogglePause (vs);

  vs->step = 1;
  }
//}}}
//{{{
static void togglePause (VideoState* vs) {

  streamTogglePause (vs);
  vs->step = 0;
  }
//}}}
//{{{
static void seekChapter (VideoState* vs, int increment) {

  int64_t pos = (int64_t)(get_master_clock (vs) * AV_TIME_BASE);

  if (!vs->ic->nb_chapters)
    return;

  /* find the current chapter */
  unsigned int i;
  for (i = 0; i < vs->ic->nb_chapters; i++) {
    AVChapter *ch = vs->ic->chapters[i];
    if (av_compare_ts (pos, AV_TIME_BASE_Q, ch->start, ch->time_base) < 0) {
      i--;
      break;
      }
    }

  i += increment;
  i = FFMAX(i, 0);
  if (i >= vs->ic->nb_chapters)
      return;

  av_log (NULL, AV_LOG_VERBOSE, "Seeking to chapter %d.\n", i);
  streamSeek (vs, av_rescale_q (vs->ic->chapters[i]->start, vs->ic->chapters[i]->time_base, AV_TIME_BASE_Q), 0);
  }
//}}}
//{{{
static void toggleAudioDisplay (VideoState* vs) {

  int bgcolor = SDL_MapRGB (screen->format, 0x00, 0x00, 0x00);
  int next = vs->show_mode;

  do {
    next = (next + 1) % (SHOW_MODE_RDFT+1);
    } while (next != vs->show_mode &&
             (next == SHOW_MODE_VIDEO && !vs->video_st || next != SHOW_MODE_VIDEO && !vs->audio_st));

  if (vs->show_mode != next) {
    vs->force_refresh = 1;
    vs->show_mode = next;
    }
  }
//}}}
//{{{
static void streamCycleChannel (VideoState* vs, int codec_type) {

  AVFormatContext* ic = vs->ic;
  int num_streams = vs->ic->nb_streams;

  int startIndex;
  int oldIndex;
  if (codec_type == AVMEDIA_TYPE_VIDEO) {
    startIndex = vs->last_video_stream;
    oldIndex = vs->video_stream;
    }
  else if (codec_type == AVMEDIA_TYPE_AUDIO) {
    startIndex = vs->last_audio_stream;
    oldIndex = vs->audio_stream;
    }
  else {
    startIndex = vs->last_subtitle_stream;
    oldIndex = vs->subtitle_stream;
    }
  int streamIndex = startIndex;

  AVProgram* avProgram = NULL;
  if (codec_type != AVMEDIA_TYPE_VIDEO && vs->video_stream != -1) {
    avProgram = av_find_program_from_stream (ic, NULL, vs->video_stream);
    if (avProgram) {
      num_streams = avProgram->nb_stream_indexes;
      for (startIndex = 0; startIndex < num_streams; startIndex++)
        if (avProgram->stream_index[startIndex] == streamIndex)
          break;
      if (startIndex == num_streams)
        startIndex = -1;
      streamIndex = startIndex;
      }
    }

  for (;;) {
    if (++streamIndex >= num_streams) {
      if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
        streamIndex = -1;
        vs->last_subtitle_stream = -1;
        goto exit;
        }
      if (startIndex == -1)
        return;
      streamIndex = 0;
      }
    if (streamIndex == startIndex)
      return;

    AVStream* avStream = vs->ic->streams[avProgram ? avProgram->stream_index[streamIndex] : streamIndex];
    if (avStream->codec->codec_type == codec_type) {
      /* check that parameters are OK */
      switch (codec_type) {
        case AVMEDIA_TYPE_AUDIO:
          if (avStream->codec->sample_rate != 0 &&
              avStream->codec->channels != 0)
            goto exit;
          break;
        case AVMEDIA_TYPE_VIDEO:
        case AVMEDIA_TYPE_SUBTITLE:
          goto exit;
        }
      }
    }

exit:
  if (avProgram && streamIndex != -1)
    streamIndex = avProgram->stream_index[streamIndex];
  av_log (NULL, AV_LOG_INFO, "Switch %s stream from #%d to #%d\n",
          av_get_media_type_string(codec_type), oldIndex, streamIndex);

  subStreamClose (vs, oldIndex);
  subStreamOpen (vs, streamIndex);
  }
//}}}
//{{{
static void toggleFullScreen (VideoState* vs) {

  is_full_screen = !is_full_screen;
  videoOpen (vs, 1, NULL);
  }
//}}}
//{{{
static void doExit (VideoState* vs) {

  if (vs) {
    vs->abort_request = 1;
    SDL_WaitThread (vs->readThread, NULL);

    packet_queue_destroy (&vs->videoq);
    packet_queue_destroy (&vs->audioq);
    packet_queue_destroy (&vs->subtitleq);

    /* free all pictures */
    frame_queue_destory (&vs->pictq);
    frame_queue_destory (&vs->sampq);
    frame_queue_destory (&vs->subpq);

    SDL_DestroyCond (vs->continueReadThread);
    sws_freeContext (vs->subSwsContext);
    av_free (vs);
    }

  av_lockmgr_register (NULL);
  av_freep (&vfilters_list);
  avformat_network_deinit();

  SDL_Quit();

  av_log (NULL, AV_LOG_QUIET, "%s", "");

  exit (0);
  }
//}}}
//{{{
static void videoRefresh (VideoState* vs, double* remainingTime) {

  double time;
  if (!vs->video_st && vs->audio_st) {
    time = av_gettime_relative() / 1000000.0;
    if (vs->force_refresh || vs->last_vis_time + rdftspeed < time) {
      // fake video refresh
      videoDisplay (vs);
      vs->last_vis_time = time;
      }
    *remainingTime = FFMIN (*remainingTime, vs->last_vis_time + rdftspeed - time);
    }

  if (vs->video_st) {
    int redisplay = 0;
    if (vs->force_refresh)
      redisplay = frame_queue_prev (&vs->pictq);
retry:
    if (frame_queue_num_remaining (&vs->pictq) != 0) {
      double last_duration, duration, delay;
      Frame* vp, *lastvp;

      /* dequeue the picture */
      lastvp = frame_queue_peek_last (&vs->pictq);
      vp = frame_queue_peek (&vs->pictq);
      if (vp->serial != vs->videoq.serial) {
        frame_queue_next (&vs->pictq);
        redisplay = 0;
        goto retry;
        }

      if (lastvp->serial != vp->serial && !redisplay)
        vs->frame_timer = av_gettime_relative() / 1000000.0;

      if (vs->paused)
        goto display;

      /* compute nominal last_duration */
      last_duration = vp_duration (vs, lastvp, vp);
      if (redisplay)
        delay = 0.0;
      else
        delay = compute_target_delay (last_duration, vs);

      time = av_gettime_relative() / 1000000.0;
      if (time < vs->frame_timer + delay && !redisplay) {
        *remainingTime = FFMIN (vs->frame_timer + delay - time, *remainingTime);
        return;
        }

      vs->frame_timer += delay;
      if (delay > 0 && time - vs->frame_timer > AV_SYNC_THRESHOLD_MAX)
        vs->frame_timer = time;

      SDL_LockMutex (vs->pictq.mutex);
      if (!redisplay && !isnan (vp->pts)) {
        set_clock (&vs->vidclk, vp->pts, vp->serial);
        sync_clock_to_slave (&vs->extclk, &vs->vidclk);
        }
      SDL_UnlockMutex (vs->pictq.mutex);

      if (frame_queue_num_remaining (&vs->pictq) > 1) {
        Frame* nextvp = frame_queue_peek_next (&vs->pictq);
        duration = vp_duration (vs, vp, nextvp);
        if (!vs->step && time > vs->frame_timer + duration) {
          if (!redisplay)
            vs->frame_drops_late++;
          frame_queue_next (&vs->pictq);
          redisplay = 0;
          goto retry;
          }
        }

      if (vs->subtitle_st) {
        while (frame_queue_num_remaining (&vs->subpq) > 0) {
          Frame* sp = frame_queue_peek (&vs->subpq);
          Frame* sp2 = (frame_queue_num_remaining (&vs->subpq) > 1) ? frame_queue_peek_next (&vs->subpq) : NULL;
          if (sp->serial != vs->subtitleq.serial ||
              (vs->vidclk.pts > (sp->pts + ((float) sp->sub.end_display_time / 1000))) ||
              (sp2 && vs->vidclk.pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
            frame_queue_next (&vs->subpq);
          else
            break;
          }
        }

display:
      videoDisplay (vs);
      frame_queue_next (&vs->pictq);
      if (vs->step && !vs->paused)
        streamTogglePause(vs);
      }
    }
  vs->force_refresh = 0;

  //{{{  print info
  int64_t cur_time = av_gettime_relative();

  if (!last_time || (cur_time - last_time) >= 30000) {

    double av_diff = 0;
    if (vs->audio_st && vs->video_st)
      av_diff = get_clock(&vs->audclk) - get_clock (&vs->vidclk);
    else if (vs->video_st)
      av_diff = get_master_clock(vs) - get_clock (&vs->vidclk);
    else if (vs->audio_st)
      av_diff = get_master_clock(vs) - get_clock (&vs->audclk);

    sprintf (infoStr,
             "%7.2f %7.3f drop:%d aud:%5dk vid:%5dk sub:%5db dts:%"PRId64" pts:%"PRId64"",
             get_master_clock (vs), av_diff, vs->frame_drops_early + vs->frame_drops_late,
             (vs->audio_st) ? vs->audioq.size/1024 : 0,
             (vs->video_st) ? vs->videoq.size/1024 : 0,
             (vs->subtitle_st) ? vs->subtitleq.size : 0,
             vs->video_st ? vs->video_st->codec->pts_correction_num_faulty_dts : 0,
             vs->video_st ? vs->video_st->codec->pts_correction_num_faulty_pts : 0);

    last_time = cur_time;
    }
  //}}}
  }
//}}}
//{{{
static void eventLoop (VideoState* vs) {

  for (;;) {
    double remaining_time = 0.0;
    SDL_PumpEvents();
    SDL_Event event;
    while (!SDL_PeepEvents (&event, 1, SDL_GETEVENT, SDL_ALLEVENTS)) {
      if (!cursor_hidden && (av_gettime_relative() - cursor_last_shown > CURSOR_HIDE_DELAY)) {
        SDL_ShowCursor (0);
        cursor_hidden = 1;
        }
      if (remaining_time > 0.0)
        av_usleep ((unsigned int)(remaining_time * 1000000.0));
      remaining_time = REFRESH_RATE;
      if (!vs->paused || vs->force_refresh)
        videoRefresh (vs, &remaining_time);
      SDL_PumpEvents();
      }

    double x, incr, pos, frac;
    switch (event.type) {
      case SDL_KEYDOWN:
        //{{{  key down
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
          case SDLK_q:
            //{{{  exit
            doExit (vs);
            break;
            //}}}

          case SDLK_f:
            //{{{  toggle full screen
            toggleFullScreen (vs);
            vs->force_refresh = 1;
            break;
            //}}}

          case SDLK_a:
            //{{{  cycle audio
            streamCycleChannel (vs, AVMEDIA_TYPE_AUDIO);
            break;
            //}}}
          case SDLK_v:
            //{{{  cycle video
            streamCycleChannel (vs, AVMEDIA_TYPE_VIDEO);
            break;
            //}}}
          case SDLK_c:
            //{{{  cycle channels
            streamCycleChannel(vs, AVMEDIA_TYPE_VIDEO);
            streamCycleChannel(vs, AVMEDIA_TYPE_AUDIO);
            streamCycleChannel(vs, AVMEDIA_TYPE_SUBTITLE);
            break;
            //}}}
          case SDLK_t:
            //{{{  cycle subtitle
            streamCycleChannel(vs, AVMEDIA_TYPE_SUBTITLE);
            break;
            //}}}
          case SDLK_w:
            //{{{  cycle displays
            if (vs->show_mode == SHOW_MODE_VIDEO && vs->vfilter_idx < num_vfilters - 1) {
              if (++vs->vfilter_idx >= num_vfilters)
                vs->vfilter_idx = 0;
              }
            else {
              vs->vfilter_idx = 0;
              toggleAudioDisplay (vs);
              }
            break;
            //}}}

          case SDLK_p:
          case SDLK_SPACE:
            //{{{  toggle pause
            togglePause (vs);
            break;
            //}}}

          case SDLK_s:
            //{{{  step next frame
            stepNextFrame (vs);
            break;
            //}}}
          case SDLK_PAGEUP:
            //{{{  next chapter 600
            if (vs->ic->nb_chapters <= 1) {
              incr = 600.0;
              goto do_seek;
              }
            seekChapter (vs, 1);
            break;
            //}}}
          case SDLK_PAGEDOWN:
            //{{{  prev chapter -600
            if (vs->ic->nb_chapters <= 1) {
              incr = -600.0;
              goto do_seek;
              }
            seekChapter (vs, -1);
            break;
            //}}}
          case SDLK_LEFT:
            //{{{  -10
            incr = -10.0;
            goto do_seek;
            //}}}
          case SDLK_RIGHT:
            //{{{  +10
            incr = 10.0;
            goto do_seek;
            //}}}
          case SDLK_UP:
            //{{{  +60
            incr = 60.0;
            goto do_seek;
            //}}}
          case SDLK_DOWN:
            incr = -60.0;
          do_seek:
            //{{{  seek
            pos = get_master_clock (vs);
            if (isnan (pos))
              pos = (double)vs->seek_pos / AV_TIME_BASE;

            pos += incr;

            if ((vs->ic->start_time != AV_NOPTS_VALUE) && (pos < vs->ic->start_time / (double)AV_TIME_BASE))
              pos = vs->ic->start_time / (double)AV_TIME_BASE;

            streamSeek (vs, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));

            break;
            //}}}
          }

        break;
        //}}}
      case SDL_VIDEOEXPOSE:
        //{{{  refresh
        vs->force_refresh = 1;
        break;
        //}}}
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEMOTION:
        //{{{  mouse moved
        if (cursor_hidden) {
          SDL_ShowCursor(1);
          cursor_hidden = 0;
          }
        cursor_last_shown = av_gettime_relative();

        if (event.type == SDL_MOUSEBUTTONDOWN)
          x = event.button.x;
        else {
          if (event.motion.state != SDL_PRESSED)
            break;
          x = event.motion.x;
          }

        int64_t ts;
        int ns, hh, mm, ss;
        int tns, thh, tmm, tss;
        tns  = (int)(vs->ic->duration / 1000000LL);
        thh  = tns / 3600;
        tmm  = (tns % 3600) / 60;
        tss  = (tns % 60);
        frac = x / vs->width;
        ns   = (int)(frac * tns);
        hh   = ns / 3600;
        mm   = (ns % 3600) / 60;
        ss   = (ns % 60);
        av_log (NULL, AV_LOG_INFO,
                "Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)       \n", frac*100,
                 hh, mm, ss, thh, tmm, tss);

        ts = (int64_t)(frac * vs->ic->duration);

        if (vs->ic->start_time != AV_NOPTS_VALUE)
          ts += vs->ic->start_time;

        streamSeek (vs, ts, 0);
        break;
        //}}}
      case SDL_VIDEORESIZE:
        //{{{  resize
        screen = SDL_SetVideoMode (
          FFMIN(16383, event.resize.w), event.resize.h, 0,
          SDL_HWSURFACE| (is_full_screen ? SDL_FULLSCREEN : SDL_RESIZABLE) | SDL_ASYNCBLIT | SDL_HWACCEL);
        screen_width  = vs->width  = screen->w;
        screen_height = vs->height = screen->h;
        vs->force_refresh = 1;
        break;
        //}}}
      case SDL_QUIT:
      case FF_QUIT_EVENT:
        //{{{  exit
        doExit(vs);
        break;
        //}}}
      case FF_ALLOC_EVENT:
        //{{{  alloc frame
        allocPicture (event.user.data1);
        break;
        //}}}
      }
    }
  }
//}}}

//{{{
static int readInterruptCallback (void* opaqueVs) {

  VideoState* vs = opaqueVs;
  return vs->abort_request;
  }
//}}}
//{{{
static int readThread (void* opqaueVs) {

  VideoState* vs = opqaueVs;

  AVFormatContext* ic = avformat_alloc_context();
  ic->interrupt_callback.callback = readInterruptCallback;
  ic->interrupt_callback.opaque = vs;
  int err = avformat_open_input (&ic, vs->filename, NULL, NULL);
  if (err < 0)
    goto exit;

  av_dump_format (ic, 0, vs->filename, 0);

  vs->ic = ic;
  vs->max_frame_duration = 3600.0;

  if (ic->pb)
    ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

  //{{{  setup opts
  AVDictionary** opts = av_mallocz_array (ic->nb_streams, sizeof(*opts));
  for (unsigned int i = 0; i < ic->nb_streams; i++)
    opts[i] = NULL;
  int orig_num_streams = ic->nb_streams;
  err = avformat_find_stream_info (ic, opts);
  for (int i = 0; i < orig_num_streams; i++)
    av_dict_free (&opts[i]);
  av_freep (&opts);
  //}}}
  //{{{  select streams
  int st_index[AVMEDIA_TYPE_NB];
  memset (st_index, -1, sizeof(st_index));
  vs->last_video_stream = vs->video_stream = -1;
  vs->last_audio_stream = vs->audio_stream = -1;
  vs->last_subtitle_stream = vs->subtitle_stream = -1;
  vs->eof = 0;

  st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream (
    ic, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
  st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream (
    ic, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);
  st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream (
    ic, AVMEDIA_TYPE_SUBTITLE, st_index[AVMEDIA_TYPE_SUBTITLE],
    (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ? st_index[AVMEDIA_TYPE_AUDIO] : st_index[AVMEDIA_TYPE_VIDEO]), NULL, 0);

  if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
    AVStream* st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
    AVCodecContext* avctx = st->codec;
    AVRational sar = av_guess_sample_aspect_ratio (ic, st, NULL);
    if (avctx->width)
      setDefaultWindowSize(avctx->width, avctx->height, sar);
    }

  if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)
    subStreamOpen (vs, st_index[AVMEDIA_TYPE_AUDIO]);
  if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
    subStreamOpen(vs, st_index[AVMEDIA_TYPE_VIDEO]);
  if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0)
    subStreamOpen(vs, st_index[AVMEDIA_TYPE_SUBTITLE]);

  if (vs->video_stream < 0 && vs->audio_stream < 0) {
    av_log (NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n", vs->filename);
    goto exit;
    }
  //}}}
  vs->show_mode = (vs->video_stream >= 0) ? SHOW_MODE_VIDEO : SHOW_MODE_WAVES;

  int ret = 0;
  SDL_mutex* wait_mutex = SDL_CreateMutex();
  for (;;) {
    if (vs->abort_request)
      break;
    if (vs->paused != vs->last_paused) {
      //{{{  handle pause change
      vs->last_paused = vs->paused;
      if (vs->paused)
        vs->read_pause_return = av_read_pause (ic);
      else
        av_read_play (ic);
      }
      //}}}
    if (vs->seek_req) {
      int64_t seek_target = vs->seek_pos;
      int64_t seek_min = vs->seek_rel > 0 ? seek_target - vs->seek_rel + 2: INT64_MIN;
      int64_t seek_max = vs->seek_rel < 0 ? seek_target - vs->seek_rel - 2: INT64_MAX;
      ret = avformat_seek_file (vs->ic, -1, seek_min, seek_target, seek_max, 0);
      if (ret < 0)
        av_log (NULL, AV_LOG_ERROR, "%s: error while seeking\n", vs->ic->filename);
      else {
        //{{{  seek ok
        if (vs->audio_stream >= 0) {
          packet_queue_flush (&vs->audioq);
          packet_queue_put (&vs->audioq, &flush_pkt);
          }
        if (vs->subtitle_stream >= 0) {
          packet_queue_flush (&vs->subtitleq);
          packet_queue_put (&vs->subtitleq, &flush_pkt);
          }
        if (vs->video_stream >= 0) {
          packet_queue_flush (&vs->videoq);
          packet_queue_put (&vs->videoq, &flush_pkt);
          }
        set_clock (&vs->extclk, seek_target / (double)AV_TIME_BASE, 0);
        }
        //}}}
      vs->seek_req = 0;
      vs->eof = 0;
      if (vs->paused)
        stepNextFrame (vs);
      }

    if (vs->audioq.size + vs->videoq.size + vs->subtitleq.size > MAX_QUEUE_SIZE ||
        ((vs->audioq.num_packets > MIN_FRAMES || vs->audio_stream < 0 || vs->audioq.abort_request) &&
         (vs->videoq.num_packets > MIN_FRAMES || vs->video_stream < 0 || vs->videoq.abort_request) &&
         (vs->subtitleq.num_packets > MIN_FRAMES || vs->subtitle_stream < 0 || vs->subtitleq.abort_request))) {
      //{{{  queue full, no need to read more
      SDL_LockMutex (wait_mutex);
      SDL_CondWaitTimeout (vs->continueReadThread, wait_mutex, 10); // 10ms
      SDL_UnlockMutex (wait_mutex);
      continue;
      }
      //}}}

    AVPacket pkt;
    ret = av_read_frame (ic, &pkt);
    if (ret < 0) {
      //{{{  no more frames ?
      if ((ret == AVERROR_EOF || avio_feof (ic->pb)) && !vs->eof) {
        if (vs->video_stream >= 0)
          packet_queue_put_nullpacket (&vs->videoq, vs->video_stream);
        if (vs->audio_stream >= 0)
          packet_queue_put_nullpacket (&vs->audioq, vs->audio_stream);
        if (vs->subtitle_stream >= 0)
          packet_queue_put_nullpacket (&vs->subtitleq, vs->subtitle_stream);
        vs->eof = 1;
        }
      if (ic->pb && ic->pb->error)
        break;

      SDL_LockMutex (wait_mutex);
      SDL_CondWaitTimeout (vs->continueReadThread, wait_mutex, 10); // 10ms
      SDL_UnlockMutex (wait_mutex);
      continue;
      }
      //}}}
    else
      vs->eof = 0;

    if (pkt.stream_index == vs->audio_stream)
      packet_queue_put (&vs->audioq, &pkt);
    else if (pkt.stream_index == vs->video_stream)
      packet_queue_put (&vs->videoq, &pkt);
    else if (pkt.stream_index == vs->subtitle_stream)
      packet_queue_put (&vs->subtitleq, &pkt);
    else
      av_free_packet (&pkt);
    }

  while (!vs->abort_request)
    SDL_Delay (100);

  ret = 0;
exit:
  //{{{  close streams
  if (vs->audio_stream >= 0)
    subStreamClose (vs, vs->audio_stream);
  if (vs->video_stream >= 0)
    subStreamClose (vs, vs->video_stream);
  if (vs->subtitle_stream >= 0)
    subStreamClose (vs, vs->subtitle_stream);
  if (ic) {
    avformat_close_input (&ic);
    vs->ic = NULL;
    }
  //}}}
  if (ret != 0) {
    //{{{  quit event
    SDL_Event event;
    event.type = FF_QUIT_EVENT;
    event.user.data1 = vs;
    SDL_PushEvent (&event);
    }
    //}}}
  SDL_DestroyMutex (wait_mutex);
  return 0;
  }
//}}}

//{{{
static int lockmgr (void** mtx, enum AVLockOp op) {

 switch (op) {
   case AV_LOCK_CREATE:
     *mtx = SDL_CreateMutex();
     if (!*mtx)
       return 1;
     return 0;

   case AV_LOCK_OBTAIN:
     return !!SDL_LockMutex (*mtx);

   case AV_LOCK_RELEASE:
     return !!SDL_UnlockMutex (*mtx);

   case AV_LOCK_DESTROY:
     SDL_DestroyMutex (*mtx);
     return 0;
     }

  return 1;
  }
//}}}
//{{{
int main (int argc, char* argv[]) {

  av_log_set_flags (AV_LOG_SKIP_REPEATED);

  avdevice_register_all();
  avfilter_register_all();
  av_register_all();
  avformat_network_init();

  if (!argv[1]) {
    av_log (NULL, AV_LOG_FATAL, "An input file must be specified\n");
    exit (1);
    }

  int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
  if (SDL_Init (flags)) {
    av_log (NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
    exit (1);
    }
  TTF_Init();
  font = TTF_OpenFont ("C:/Windows/Fonts/lucon.ttf", 20);

  const SDL_VideoInfo* videoInfo = SDL_GetVideoInfo();
  fs_screen_width = videoInfo->current_w;
  fs_screen_height = videoInfo->current_h;

  SDL_EventState (SDL_ACTIVEEVENT, SDL_IGNORE);
  SDL_EventState (SDL_SYSWMEVENT, SDL_IGNORE);
  SDL_EventState (SDL_USEREVENT, SDL_IGNORE);

  av_lockmgr_register (lockmgr);

  av_init_packet (&flush_pkt);
  flush_pkt.data = (uint8_t*)&flush_pkt;

  // videoState init
  VideoState* vs = av_mallocz (sizeof (VideoState));
  av_strlcpy (vs->filename, argv[1], sizeof (vs->filename));

  frame_queue_init (&vs->pictq, &vs->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1);
  frame_queue_init (&vs->subpq, &vs->subtitleq, SUBPICTURE_QUEUE_SIZE, 0);
  frame_queue_init (&vs->sampq, &vs->audioq, SAMPLE_QUEUE_SIZE, 1);

  packet_queue_init (&vs->videoq);
  packet_queue_init (&vs->audioq);
  packet_queue_init (&vs->subtitleq);

  init_clock (&vs->vidclk, &vs->videoq.serial);
  init_clock (&vs->audclk, &vs->audioq.serial);
  init_clock (&vs->extclk, &vs->extclk.serial);

  vs->ytop = 0;
  vs->xleft = 0;
  vs->audio_clock_serial = -1;
  vs->continueReadThread = SDL_CreateCond();
  vs->readThread = SDL_CreateThread (readThread, vs);

  // events
  eventLoop (vs);

  return 0;
  }
//}}}
