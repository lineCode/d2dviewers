// main.cpp = 320000  using libfaad randomly broke
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "ts.h"
#include "bda.h"
#include "../curl/curl.h"
#include "../libfaad/neaacdec.h"
#include "../common/winAudio.h"

#pragma comment(lib,"libcurl.lib")
#pragma comment(lib,"libfaad.lib")

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"swscale.lib")
//}}}
#define r3
//{{{  const
#ifdef _WIN64
  #define maxAudFrames 1000000
  #define maxVidFrames 10000
#else
  #define maxAudFrames 100000
  #define maxVidFrames 600
#endif

#ifdef bbc1
  static const char* root = "http://vs-hls-uk-live.edgesuite.net/pool_4/live/bbc_one_hd/bbc_one_hd.isml/";
  static const char* bitrate = "bbc_one_hd-audio_2%3d96000-video%3d1374000.norewind.m3u8";
#endif

#ifdef bbc2
  static const char* root = "http://vs-hls-uk-live.edgesuite.net/pool_5/live/bbc_two_hd/bbc_two_hd.isml/";
 static const char* bitrate = "bbc_two_hd-audio_2%3d96000-video%3d1374000.norewind.m3u8";
# endif

#ifdef bbc5
  static const char* root = "http://vs-hls-uk-live.edgesuite.net/pool_10/live/bbc_radio_five_live_video/bbc_radio_five_live_video.isml/";
  static const char* bitrate = "bbc_radio_five_live_video-audio_1=128000-video=1374000.m3u8";
#endif

#ifdef r4
  static const char* root = "http://as-hls-uk-live.bbcfmt.vo.llnwd.net/pool_6/live/bbc_radio_fourfm/bbc_radio_fourfm.isml/";
  static const char* bitrate = "bbc_radio_fourfm-audio%3d48000.m3u8?dvr_window_length=24";
  //static const char* bitrate = "bbc_radio_fourfm-audio%3d320000.m3u8?dvr_window_length=24";
#endif

#ifdef r3
  static const char* root = "http://as-hls-uk-live.bbcfmt.vo.llnwd.net/pool_7/live/bbc_radio_three/bbc_radio_three.isml/";
  static const char* bitrate = "bbc_radio_three-audio%3d128000.m3u8?dvr_window_length=24";
  //static const char* bitrate = "bbc_radio_three-audio%3d320000.m3u8?dvr_window_length=24";
#endif

#ifdef r6
  static const char* root = "http://as-hls-uk-live.bbcfmt.vo.llnwd.net/pool_6/live/bbc_6music/bbc_6music.isml/";
  static const char* bitrate = "bbc_6music-audio%3d128000.m3u8?dvr_window_length=24";
  //static const char* bitrate = "bbc_6music-audio%3d320000.m3u8?dvr_window_length=24";
#endif

//static const char* today = "http://www.bbc.co.uk/radio4/programmes/schedules/fm/today.json";
static const char *ot[6] = { "NULL", "MAIN AAC", "LC AAC", "SSR AAC", "LTP AAC", "HE AAC" };
static const unsigned long srates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };
//}}}
//{{{
class cAppWindow : public cD2dWindow {
public:
  cAppWindow() {}
  ~cAppWindow() {}

  //{{{
  static cAppWindow* create (wchar_t* title, int width, int height) {

    cAppWindow* appWindow = new cAppWindow();
    appWindow->initialise (title, width, height);
    return appWindow;
    }
  //}}}

protected:
  bool onKey (int key);
  void onMouseMove (bool right, int x, int y, int xInc, int yInc);
  void onMouseUp (bool right, bool mouseMoved, int x, int y);
  void onDraw (ID2D1DeviceContext* deviceContext);
  };
//}}}
//{{{  vars
cAppWindow* appWindow = NULL;

char* m3u8Buf = NULL;
int m3u8BufEndIndex = 0;

unsigned char* tsBuf = NULL;
int tsBufEndIndex = 0;

char* filename;

int vidStream = -1;
int audStream = -1;
int subStream = -1;

float audFramesPerSec = 0.0;
unsigned char channels = 2;
unsigned long sampleRate = 48000;
int samples = 1024;

bool stopped = false;
float playFrame = 0.0;
float markFrame = 0.0;
int vidPlayFrame = 0;

int lastVidFrame = -1;
ID2D1Bitmap* mD2D1Bitmap = NULL;

int audFramesLoaded = 0;
void* audFrames [maxAudFrames];
float audFramesPowerL [maxAudFrames];
float audFramesPowerR [maxAudFrames];

int vidFramesLoaded = 0;
IWICBitmap* vidFrames [maxVidFrames];

static bool bdaGraph = false;
//}}}
//{{{  macros
#define BPP 1

#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
  unsigned int v = ((const uint32_t *)(s))[0];\
  a = (v >> 24) & 0xff;\
  r = (v >> 16) & 0xff;\
  g = (v >> 8) & 0xff;\
  b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
  unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
  a = (val >> 24) & 0xff;\
  y = (val >> 16) & 0xff;\
  u = (val >> 8) & 0xff;\
  v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
  ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}
//}}}
//#define PLAYLIST_DEBUG
//#define PACKET_DEBUG

// hls helpers
//{{{
static size_t writeM3u8 (void* ptr, size_t itemSize, size_t items, void* stream) {

  m3u8Buf = (char*)realloc (m3u8Buf, m3u8BufEndIndex + items);
  memcpy (m3u8Buf + m3u8BufEndIndex, ptr, items);
  m3u8BufEndIndex += (int)items;

  return items;
  }
//}}}
//{{{
static size_t writeTs (void* ptr, size_t itemSize, size_t items, void* stream) {

  tsBuf = (unsigned char*)realloc (tsBuf, tsBufEndIndex + items);
  memcpy (tsBuf + tsBufEndIndex, ptr, items);
  tsBufEndIndex += (int)items;

  return items;
  }
//}}}
//{{{
static void getPlaylist (CURL* curl, char* filename, int* sequenceNum, int* numSeqFiles) {

  // set curl m3u8 url
  char m3u8Url [200];
  strcpy (m3u8Url, root);
  strcat (m3u8Url, bitrate);
  curl_easy_setopt (curl, CURLOPT_URL, m3u8Url);

  // GET m3u8
  curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, 0L);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, writeM3u8);
  curl_easy_perform (curl);

#ifdef PLAYLIST_DEBUG
  for (int i = 0; i < m3u8BufEndIndex; i++)
    printf ("%c", m3u8Buf[i]);
  printf ("\n");
#endif

  // find #EXT-X-MEDIA-SEQUENCE in m3u8 - extract sequence num
  char* extSequence = strstr (m3u8Buf, "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
  char* extSequenceEnd = strchr (extSequence, '\n');
  int sequenceLen = int(extSequenceEnd - extSequence);

  char sequenceStr [200];
  strncpy (sequenceStr, extSequence, sequenceLen);
  sequenceStr[sequenceLen] = '\0';
  *sequenceNum = atoi (sequenceStr);

  printf ("#EXT-X-MEDIA-SEQUENCE sequenceStr:%s sequenceNum:%d\n", sequenceStr, *sequenceNum);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, writeTs);

  // find first #EXTINF in m3u8 - extract filename, sequenceStr pos, numSeqFiles in playlist
  char* extinf = strstr (m3u8Buf, "#EXTINF");
  char* extFilename = strchr (extinf, '\n') + 1;
  char* extFilenameEnd = strchr (extFilename, '\n');
  int filenameLen = int(extFilenameEnd - extFilename);

  strncpy (filename, extFilename, filenameLen);
  filename[filenameLen] = '\0';

  char* sequenceNumStr = strstr (filename, sequenceStr);
  *sequenceNumStr = '\0';

  int files = 1;
  m3u8Buf = extFilenameEnd;
  while (true) {
    char* nextExtinf = strstr (m3u8Buf, "#EXTINF");
    if (nextExtinf == 0)
      break;
    files++;
    m3u8Buf = nextExtinf + 1;
    }
  *numSeqFiles = files;

  printf ("#EXTINF filename:%s numSeqFiles:%d\n", filename, files);
  }
//}}}

//{{{
static DWORD WINAPI hlsLoaderThread (LPVOID arg) {

  curl_global_init (CURL_GLOBAL_ALL);
  CURL* curl = curl_easy_init();

#ifdef PLAYLIST_DEBUG
  curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
#endif

  //{{{  get playlist sequenceNum and tsFile filename root, start at last but one
  char filename [200];
  int sequenceNum = 0;
  int numSeqFiles = 0;
  getPlaylist (curl, filename, &sequenceNum, &numSeqFiles);
  //}}}
  sequenceNum += numSeqFiles - 4;

  //{{{  init vidCodecContext, vidCodec, vidParser, vidFrame
  av_register_all();

  AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_H264);;
  AVCodecContext* vidCodecContext = avcodec_alloc_context3 (vidCodec);;
  avcodec_open2 (vidCodecContext, vidCodec, NULL);

  AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_H264);;

  AVFrame* vidFrame = av_frame_alloc();;
  struct SwsContext* swsContext = NULL;
  //}}}
  NeAACDecHandle decoder = NeAACDecOpen();

  unsigned char* pesAud = NULL;
  unsigned char* pesVid = (unsigned char*)malloc (2000000);
  while (true) {
    printf ("loading tsFile:%d", sequenceNum);
    //{{{  set ts file url
    char tsUrl [200];
    sprintf (tsUrl, "%s%s%d.ts", root, filename, sequenceNum);
    curl_easy_setopt (curl, CURLOPT_URL, tsUrl);
    //}}}
    //{{{  load ts file
    tsBufEndIndex = 0;
    int nn = curl_easy_perform (curl);
    //}}}
    if (tsBufEndIndex < 100) {
      printf (" not found\n");
      Sleep (2000);
      }
    else {
      int pesAudIndex = 0;
      int pesAudLen = 0;
      int lastAudPidContinuity = -1;

      int pesVidIndex = 0;
      int lastVidPidContinuity = -1;

      int tsIndex = 0;
      while (tsIndex < tsBufEndIndex) {
        int tsFrameIndex = 0;
        if (tsBuf[tsIndex] == 0x47) {
          //{{{  tsFrame syncCode found, extract pes from tsFrames
          bool payStart  =  (tsBuf[tsIndex+tsFrameIndex+1] & 0x40) != 0;
          int pid        = ((tsBuf[tsIndex+tsFrameIndex+1] & 0x1F) << 8) | tsBuf[tsIndex+2];
          bool adapt     =  (tsBuf[tsIndex+tsFrameIndex+3] & 0x20) != 0;
          bool payload   =  (tsBuf[tsIndex+tsFrameIndex+3] & 0x10) != 0;
          int continuity =   tsBuf[tsIndex+tsFrameIndex+3] & 0x0F;
          tsFrameIndex += 4;

          if (adapt) {
            int adaptLen = tsBuf[tsIndex+tsFrameIndex];
            tsFrameIndex += 1 + adaptLen;
            }

          if (pid == 34) {
            //{{{  handle audio
            if (payload && ((lastAudPidContinuity == -1) || (continuity == ((lastAudPidContinuity+1) & 0x0F)))) {
              while (tsFrameIndex < 188) {
                if (pesAudLen == 0) {
                  // look for PES startCode
                  if (payStart &&
                      (tsBuf[tsIndex+tsFrameIndex] == 0) &&
                      (tsBuf[tsIndex+tsFrameIndex+1] == 0) &&
                      (tsBuf[tsIndex+tsFrameIndex+2] == 1) &&
                      (tsBuf[tsIndex+tsFrameIndex+3] == 0xC0)) {
                    // PES start code found
                    pesAudLen = (tsBuf[tsIndex+tsFrameIndex+4] << 8) | tsBuf[tsIndex+tsFrameIndex+5];
                    int ph1 = tsBuf[tsIndex+tsFrameIndex+6];
                    int ph2 = tsBuf[tsIndex+tsFrameIndex+7];

                    int pesAudHeadLen = tsBuf[tsIndex+tsFrameIndex+8];
                    tsFrameIndex += 9 + pesAudHeadLen;
                    pesAudLen -= 3 + pesAudHeadLen;
                    pesAud = (unsigned char*)realloc (pesAud, pesAudIndex+pesAudLen);

                    if (tsFrameIndex >= 188)
                      printf ("*** overflow tsFrameIndex:%d\n", tsFrameIndex);
                    }
                  else {
                    printf ("skiping %d looking for PES start\n", tsFrameIndex);
                    tsFrameIndex++;
                    }
                  }
                else {
                  // copy all pes frags from start of tsBuf, always behind ts read ptr
                  pesAud[pesAudIndex++] = tsBuf[tsIndex+tsFrameIndex++];
                  pesAudLen--;
                  }
                }
              }
            else
              printf ("--- ts continuity break last:%d this:%d\n", lastAudPidContinuity, continuity);

            lastAudPidContinuity = continuity;
            }
            //}}}
          else if (pid == 33) {
            //{{{  handle vid
            if (payload && ((lastVidPidContinuity == -1) || (continuity == ((lastVidPidContinuity+1) & 0x0F)))) {
              // look for PES startCode
              if (payStart &&
                  (tsBuf[tsIndex+tsFrameIndex] == 0) &&
                  (tsBuf[tsIndex+tsFrameIndex+1] == 0) &&
                  (tsBuf[tsIndex+tsFrameIndex+2] == 1) &&
                  (tsBuf[tsIndex+tsFrameIndex+3] == 0xE0)) {
                // PES start code found
                int pesHeadLen = tsBuf[tsIndex+tsFrameIndex+8];
                tsFrameIndex += 9 + pesHeadLen;
                }
              while (tsFrameIndex < 188)
                pesVid[pesVidIndex++] = tsBuf[tsIndex+tsFrameIndex++];
              }
            else
              printf ("--- ts vid continuity break last:%d this:%d\n", lastVidPidContinuity, continuity);

            lastVidPidContinuity = continuity;
            }
            //}}}
          //else
          //  printf ("skipping pid:%d\n", pid);

          tsIndex += 188;
          }
          //}}}
        else
          tsIndex++;
        }

      if (audFramesLoaded == 0) {
        //{{{  init aud decoder with firstframe
        decoder = NeAACDecOpen();

        NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (decoder);
        config->outputFormat = FAAD_FMT_16BIT;
        NeAACDecSetConfiguration (decoder, config);
        NeAACDecInit (decoder, pesAud, FAAD_MIN_STREAMSIZE, &sampleRate, &channels);

        // dummy decode of first frame
        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode (decoder, &frameInfo, pesAud, FAAD_MIN_STREAMSIZE);
        }
        //}}}

      printf (" ts:%d audPes:%d vidPes:%d\n", tsBufEndIndex, pesAudIndex, pesVidIndex);

      for (int index = 0; index < pesAudIndex;) {
        //{{{  decode audPes into audFrames
        // alloc a frame and copy from pesAud
        int len = ((pesAud[index+3]&0x03)<<11) | (pesAud[index+4]<<3) | ((pesAud[index+5]&0xE0)>>5);
        audFrames[audFramesLoaded] = malloc (len);
        memcpy (audFrames[audFramesLoaded], pesAud+index, len);

        // decode, accum frameLR power
        NeAACDecFrameInfo frameInfo;
        short* ptr = (short*)NeAACDecDecode (decoder, &frameInfo, (unsigned char*)audFrames[audFramesLoaded], FAAD_MIN_STREAMSIZE);
        samples = frameInfo.samples / 2;
        audFramesPerSec = (float)sampleRate / samples;


        #ifdef PACKET_DEBUG
        printf ("audio frame len:%d audFramesLoaded:%d frameInfo.samples:%d\n",
                len, audFramesLoaded, frameInfo.samples);
        for (int i = 0; i < 32; i++)
          printf ("%02x ", (*((char*)audFrames[audFramesLoaded] + i)) &0xFF);
        printf ("\n");
        #endif


        double valueL = 0;
        double valueR = 0;
        for (int i = 0; i < samples; i++) {
          valueL += pow(*ptr++, 2);
          valueR += pow(*ptr++, 2);
          }
        audFramesPowerL[audFramesLoaded] = (float)sqrt(valueL) / frameInfo.samples;
        audFramesPowerR[audFramesLoaded] = (float)sqrt(valueR) / frameInfo.samples;

        audFramesLoaded++;

        index += len;
        }
        //}}}

      if (pesVidIndex > 0)
        if (vidFramesLoaded < maxVidFrames-2) {
          //{{{  decode vidPes into vidFrames
          AVPacket vidPacket;
          av_init_packet (&vidPacket);
          vidPacket.data = pesVid;
          vidPacket.size = 0;

          while (pesVidIndex > 0) {
            uint8_t* data = NULL;
            av_parser_parse2 (vidParser, vidCodecContext,
                              &data, &vidPacket.size, vidPacket.data, pesVidIndex, 0, 0, AV_NOPTS_VALUE);

            int gotPicture = 0;
            int len = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
            vidPacket.data += len;
            pesVidIndex -= len;

            if (gotPicture != 0) {
              if (!swsContext)
                swsContext = sws_getContext (vidCodecContext->width, vidCodecContext->height, vidCodecContext->pix_fmt,
                                             vidCodecContext->width, vidCodecContext->height, AV_PIX_FMT_BGR24,
                                             SWS_BICUBIC, NULL, NULL, NULL);

              // create wicBitmap
              appWindow->getWicImagingFactory()->CreateBitmap (vidCodecContext->width, vidCodecContext->height,
                                               GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand,
                                               &vidFrames[vidFramesLoaded]);
              //{{{  lock wicBitmap
              WICRect wicRect = { 0, 0, vidCodecContext->width, vidCodecContext->height };
              IWICBitmapLock* wicBitmapLock = NULL;
              vidFrames[vidFramesLoaded]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);
              //}}}
              //{{{  get wicBitmap buffer
              UINT bufferLen = 0;
              BYTE* buffer = NULL;
              wicBitmapLock->GetDataPointer (&bufferLen, &buffer);
              //}}}
              int linesize = vidCodecContext->width * 3;
              sws_scale (swsContext, vidFrame->data, vidFrame->linesize, 0, vidCodecContext->height, &buffer, &linesize);
              wicBitmapLock->Release();

              vidFramesLoaded++;
              }
            }
          }
          //}}}

      appWindow->changed();
      sequenceNum++;
      }
    }

  av_free (vidFrame);
  avcodec_close (vidCodecContext);
  NeAACDecClose (decoder);
  return 0;
  }
//}}}
//{{{
static DWORD WINAPI hlsPlayerThread (LPVOID arg) {

  CoInitialize (NULL);

  float lastPlayFrame = 0.0f;

  // wait for frame to complete init
  while (audFramesLoaded < 1)
    Sleep (40);

  // init decoder
  NeAACDecHandle decoder = NeAACDecOpen();
  NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (decoder);
  config->outputFormat = FAAD_FMT_16BIT;
  NeAACDecSetConfiguration (decoder, config);
  NeAACDecInit (decoder, (unsigned char*)audFrames[0], FAAD_MIN_STREAMSIZE, &sampleRate, &channels);

  // dummy decode of first frame
  NeAACDecFrameInfo frameInfo;
  void* sampleBuffer = NeAACDecDecode (decoder, &frameInfo, (unsigned char*)audFrames[0], FAAD_MIN_STREAMSIZE);
  printf ("Player ADTS sampleRate:%d channels:%d %d\n", sampleRate, channels, frameInfo.samples);

  winAudioOpen (sampleRate, 16, channels);

  while (true) {
    if (stopped || ((int(playFrame)+1) >= audFramesLoaded))
      Sleep (40);
    else if (audFrames[int(playFrame)]) {
      NeAACDecFrameInfo frameInfo;
      void* wavBuf = NeAACDecDecode (decoder, &frameInfo, (unsigned char*)audFrames[int(playFrame)], FAAD_MIN_STREAMSIZE);

      winAudioPlay (wavBuf, channels * frameInfo.samples,  1.0f);
      lastPlayFrame = playFrame;

      if (!appWindow->getMouseDown())
        if ((int(playFrame)+1) < audFramesLoaded) {
          playFrame += 1.0f;
          appWindow->changed();
          }
      }
    }

  winAudioClose();
  NeAACDecClose (decoder);
  CoUninitialize();
  return 0;
  }
//}}}
//{{{
static DWORD WINAPI fileLoaderThread (LPVOID arg) {

  AVFormatContext* avFormatContext = (AVFormatContext*)arg;
  //{{{  aud init
  AVCodecContext* audCodecContext = NULL;
  AVCodec* audCodec = NULL;
  AVFrame* audFrame = NULL;

  if (audStream >= 0) {
    audCodecContext = avFormatContext->streams[audStream]->codec;
    audCodec = avcodec_find_decoder (audCodecContext->codec_id);
    avcodec_open2 (audCodecContext, audCodec, NULL);
    audFrame = av_frame_alloc();
    }
  //}}}
  //{{{  vid init
  AVCodecContext* vidCodecContext = NULL;
  AVCodec* vidCodec = NULL;
  AVFrame* vidFrame = NULL;
  struct SwsContext* swsContext = NULL;

  if (vidStream >= 0) {
    vidCodecContext = avFormatContext->streams[vidStream]->codec;
    vidCodec = avcodec_find_decoder (vidCodecContext->codec_id);
    avcodec_open2 (vidCodecContext, vidCodec, NULL);
    vidFrame = av_frame_alloc();
    swsContext = sws_getContext (vidCodecContext->width, vidCodecContext->height, vidCodecContext->pix_fmt,
                                 vidCodecContext->width, vidCodecContext->height, AV_PIX_FMT_BGR24,
                                 SWS_BICUBIC, NULL, NULL, NULL);
    }
  //}}}
  //{{{  sub init
  AVCodecContext* subCodecContext = NULL;
  AVCodec* subCodec = NULL;

  if (subStream >= 0) {
    subCodecContext = avFormatContext->streams[subStream]->codec;
    subCodec = avcodec_find_decoder (subCodecContext->codec_id);
    avcodec_open2 (subCodecContext, subCodec, NULL);
    }
  //}}}
  AVSubtitle sub;
  memset (&sub, 0, sizeof(AVSubtitle));

  audFramesLoaded = 0;
  vidFramesLoaded = 0;
  AVPacket avPacket;
  while (true) {
    while (av_read_frame (avFormatContext, &avPacket) >= 0) {
      if (avPacket.stream_index == audStream) {
        // aud packet
        int gotPicture = 0;
        avcodec_decode_audio4 (audCodecContext, audFrame, &gotPicture, &avPacket);
        if (gotPicture) {
          samples = audFrame->nb_samples;
          audFramesPerSec = (float)sampleRate / samples;
          audFrames[audFramesLoaded] = malloc (channels * samples * 2);

          if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
            //{{{  16bit signed planar
            short* lptr = (short*)audFrame->data[0];
            short* rptr = (short*)audFrame->data[1];
            short* ptr = (short*)audFrames[audFramesLoaded];

            double valueL = 0;
            double valueR = 0;
            for (int i = 0; i < audFrame->nb_samples; i++) {
              *ptr = *lptr++;
              valueL += pow(*ptr++, 2);
              *ptr = *rptr++;
              valueR += pow(*ptr++, 2);
              }

            audFramesPowerL[audFramesLoaded] = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
            audFramesPowerR[audFramesLoaded] = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
            audFramesLoaded++;
            }
            //}}}
          else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
            //{{{  32bit float planar
            float* lptr = (float*)audFrame->data[0];
            float* rptr = (float*)audFrame->data[1];
            short* ptr = (short*)audFrames[audFramesLoaded];

            double valueL = 0;
            double valueR = 0;
            for (int i = 0; i < audFrame->nb_samples; i++) {
              *ptr = (short)(*lptr++ * 0x8000);
              valueL += pow(*ptr++, 2);
              *ptr = (short)(*rptr++ * 0x8000);
              valueR += pow(*ptr++, 2);
              }

            audFramesPowerL[audFramesLoaded] = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
            audFramesPowerR[audFramesLoaded] = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
            audFramesLoaded++;
            }
            //}}}
          else
            printf ("new sample_fmt:%d\n",audCodecContext->sample_fmt);
          }
        }

      else if (avPacket.stream_index == vidStream) {
        // vid packet
        if (vidFramesLoaded < 2000) {
          int gotPicture = 0;
          avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &avPacket);
          if (gotPicture) {
            // create wicBitmap
            appWindow->getWicImagingFactory()->CreateBitmap (vidCodecContext->width, vidCodecContext->height,
                                             GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand,
                                             &vidFrames[vidFramesLoaded]);
            //{{{  lock wicBitmap
            WICRect wicRect = { 0, 0, vidCodecContext->width, vidCodecContext->height };
            IWICBitmapLock* wicBitmapLock = NULL;
            vidFrames[vidFramesLoaded]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);
            //}}}
            //{{{  get wicBitmap buffer
            UINT bufferLen = 0;
            BYTE* buffer = NULL;
            wicBitmapLock->GetDataPointer (&bufferLen, &buffer);
            //}}}
            int linesize = vidCodecContext->width * 3;
            sws_scale (swsContext, vidFrame->data, vidFrame->linesize, 0, vidCodecContext->height, &buffer, &linesize);

            // add subtitle
            for (unsigned int r = 0; r < sub.num_rects; r++) {
              uint8_t* src = sub.rects[r]->pict.data[0];
              uint32_t* palette = (uint32_t*)sub.rects[r]->pict.data[1];

              for (int j = 0; j < sub.rects[r]->h; j++) {
                uint8_t* dst = buffer + ((sub.rects[r]->y + j)* linesize) + (sub.rects[r]->x * 3);
                for (int i = 0; i < sub.rects[r]->w; i++) {
                  uint32_t abgr = *(palette + *src++);
                  uint8_t a = abgr >> 24;
                  if (a) {
                    *dst++ = abgr & 0xFF;
                    *dst++ = (abgr >> 8) & 0xFF;
                    *dst++ = (abgr >> 16) & 0xFF;
                    }
                  else
                    dst += 3;
                  }
                }
              }

            wicBitmapLock->Release();

            vidFramesLoaded++;
            }
          }
        }
      else if (avPacket.stream_index == subStream) {
        int gotPicture = 0;
        avsubtitle_free (&sub);
        avcodec_decode_subtitle2 (subCodecContext, &sub, &gotPicture, &avPacket);
        }
      av_free_packet (&avPacket);
      }
    }

  av_free (vidFrame);
  av_free (audFrame);
  avcodec_close (vidCodecContext);
  avcodec_close (audCodecContext);
  avformat_close_input (&avFormatContext);

  return 0;
  }
//}}}
//{{{
static DWORD WINAPI filePlayerThread (LPVOID arg) {

  CoInitialize (NULL);
  // wait for first aud frame to load for sampleRate, channels
  while (audFramesLoaded < 1)
    Sleep (40);

  winAudioOpen (sampleRate, 16, channels);

  while (true) {
    if (stopped || (int(playFrame)+1 >= audFramesLoaded))
      Sleep (40);
    else if (audFrames[int(playFrame)]) {
      winAudioPlay (audFrames[int(playFrame)], channels * samples*2, 1.0f);

      if (!appWindow->getMouseDown())
        if ((int(playFrame)+1) < audFramesLoaded) {
          playFrame += 1.0f;
          appWindow->changed();
          }
      }
    }

  winAudioClose();
  CoUninitialize();
  return 0;
  }
//}}}
//{{{
static DWORD WINAPI tsFileLoaderThread (LPVOID arg) {

  parseFileMapped ((wchar_t*)arg);
  printServices();
  printPids();
  printPrograms();

  return 0;
  }
//}}}
//{{{
      //renderPlayer   (mD2dContext.Get(), mClient,
      //                mTextFormat, mWhiteBrush, mBlueBrush, mBlackBrush, mGreyBrush);
      //renderPidInfo  (mD2dContext.Get(), mClient,
      //                mTextFormat, mWhiteBrush, mBlueBrush, mBlackBrush, mGreyBrush);
      //renderBDAGraph (mD2dContext.Get(), mClient,
      //                mTextFormat, mWhiteBrush, mBlueBrush, mBlackBrush, mGreyBrush);
//}}}

//{{{
bool cAppWindow::onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    case 0x20 : stopped = !stopped; break;

    case 0x21 : playFrame -= 5.0f * audFramesPerSec; changed(); break;
    case 0x22 : playFrame += 5.0f * audFramesPerSec; changed(); break;

    case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
    case 0x24 : playFrame = 0; break;

    case 0x25 : playFrame -= 1.0f * audFramesPerSec; changed(); break;
    case 0x27 : playFrame += 1.0f * audFramesPerSec; changed(); break;

    case 0x26 : stopped = true; playFrame -=2; changed(); break;
    case 0x28 : stopped = true; playFrame +=2; changed(); break;

    case 0x2d : markFrame = playFrame - 2.0f; changed(); break;
    case 0x2e : playFrame = markFrame; changed(); break;

    default   : printf ("key %x\n", key);
    }

  if (playFrame < 0)
    playFrame = 0;
  else if (playFrame >= audFramesLoaded)
    playFrame = audFramesLoaded - 1.0f;

  return false;
  }
//}}}
//{{{
void cAppWindow::onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  playFrame -= xInc;

  if (playFrame < 0)
    playFrame = 0;
  else if (playFrame >= audFramesLoaded)
    playFrame = audFramesLoaded - 1.0f;

  changed();
  }
//}}}
//{{{
void cAppWindow::onMouseUp  (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved)
    playFrame += x - (getClientF().width/2.0f);

  if (playFrame < 0)
    playFrame = 0;
  else if (playFrame >= audFramesLoaded)
    playFrame = audFramesLoaded - 1.0f;

  changed();
  }
//}}}
//{{{
void cAppWindow::onDraw (ID2D1DeviceContext* deviceContext) {

  vidPlayFrame = int((playFrame / audFramesPerSec) * 25);
  if (vidPlayFrame < 0)
     vidPlayFrame = 0;
  else if (vidPlayFrame >= vidFramesLoaded)
     vidPlayFrame = vidFramesLoaded-1;
  if ((vidPlayFrame != lastVidFrame) && vidFrames[vidPlayFrame]) {
    // make new D2D1Bitmap from each new wicBitmap
    lastVidFrame = vidPlayFrame;
    IWICFormatConverter* wicFormatConverter;
    getWicImagingFactory()->CreateFormatConverter (&wicFormatConverter);
    wicFormatConverter->Initialize (vidFrames[vidPlayFrame],
                                    GUID_WICPixelFormat32bppPBGRA,
                                    WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
    if (mD2D1Bitmap)
      mD2D1Bitmap->Release();
    if (deviceContext)
      deviceContext->CreateBitmapFromWicBitmap (wicFormatConverter, NULL, &mD2D1Bitmap);
    }

  if (mD2D1Bitmap)
    deviceContext->DrawBitmap (mD2D1Bitmap, D2D1::RectF(0.0f,0.0f,getClientF().width, getClientF().height));
  else
    deviceContext->Clear (D2D1::ColorF(D2D1::ColorF::Black));

  D2D1_RECT_F rt = D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height);
  D2D1_RECT_F offset = D2D1::RectF(2.0f, 2.0f, getClientF().width, getClientF().height);

  D2D1_RECT_F r = D2D1::RectF((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
  deviceContext->FillRectangle (r, getGreyBrush());

  int j = int(playFrame) - (int)(getClientF().width/2);
  for (float i = 0.0f; (i < getClientF().width) && (j < audFramesLoaded); i++, j++) {
    r.left = i;
    r.right = i + 1.0f;
    if (j >= 0) {
      r.top = (getClientF().height/2.0f) - audFramesPowerL[j];
      r.bottom = (getClientF().height/2.0f) + audFramesPowerR[j];
      deviceContext->FillRectangle (r, getBlueBrush());
      }
    }

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%4.1fs - %2.0f:%d of %2d:%d",
            playFrame/audFramesPerSec, playFrame, vidPlayFrame, audFramesLoaded, vidFramesLoaded);
  deviceContext->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), offset, getBlackBrush());
  deviceContext->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), rt, getWhiteBrush());
  }
//}}}

//{{{
int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);
  //{{{
  #ifndef _DEBUG
  FreeConsole();
  #endif
  //}}}

  printf ("Player %d\n", argc);
  if (argv[1] && (wcscmp (argv[1], L"bda") == 0)) {
    if (argv[2])
      CreateThread (NULL, 0, tsFileLoaderThread, argv[2], 0, NULL);
    else {
      //{{{  bda
      int freq = 674000; // 650000 674000 706000
      wchar_t fileName [100];
      swprintf (fileName, 100, L"C://Users/nnn/Desktop/%d.ts", freq);
      openTsFile (fileName);
      createBDAGraph (freq);
      bdaGraph = true;
      }
      //}}}
    }
  else if (argv[1]) {
    //{{{  file
    char filename [100];
    wcstombs (filename, argv[1], 100);

    av_register_all();
    avformat_network_init();

    AVFormatContext* avFormatContext = NULL;
    avformat_open_input (&avFormatContext, filename, NULL, NULL);
    avformat_find_stream_info (avFormatContext, NULL);

    for (unsigned int i = 0; i < avFormatContext->nb_streams; i++)
      if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (vidStream == -1))
        vidStream = i;
      else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) && (audStream == -1))
        audStream = i;
      else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) && (subStream == -1))
        subStream = i;

    if (audStream >= 0) {
      channels = avFormatContext->streams[audStream]->codec->channels;
      sampleRate = avFormatContext->streams[audStream]->codec->sample_rate;
      }

    printf ("filename:%s sampleRate:%d channels:%d streams:%d aud:%d vid:%d sub:%d\n",
            filename, sampleRate, channels,
            avFormatContext->nb_streams, audStream, vidStream, subStream);
    av_dump_format (avFormatContext, 0, filename, 0);

    if (channels > 2)
      channels = 2;

    appWindow = cAppWindow::create (argv[1],
      (vidStream >= 0) ? avFormatContext->streams[vidStream]->codec->width : 1024,
      (vidStream >= 0) ? avFormatContext->streams[vidStream]->codec->height : 256);

    CreateThread (NULL, 0, fileLoaderThread, avFormatContext, 0, NULL);

    HANDLE hFilePlayerThread = CreateThread (NULL, 0, filePlayerThread, avFormatContext, 0, NULL);
    SetThreadPriority (hFilePlayerThread, THREAD_PRIORITY_ABOVE_NORMAL);
    }
    //}}}
  else {
    //{{{  hls
    appWindow = cAppWindow::create (L"hls player", 896, 504);

    CreateThread (NULL, 0, hlsLoaderThread, NULL, 0, NULL);
    HANDLE hHlsPlayerThread  = CreateThread (NULL, 0, hlsPlayerThread, NULL, 0, NULL);
    SetThreadPriority (hHlsPlayerThread, THREAD_PRIORITY_ABOVE_NORMAL);
    }
    //}}}

  appWindow->messagePump();

  CoUninitialize();
  }
//}}}
