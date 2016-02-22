// tv.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cTransportStream.h"

#include "../common/cYuvFrame.h"
#include "../common/yuvrgb_sse2.h"

#include "../common/cAudFrame.h"
#include "../common/winAudio.h"
#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
//}}}
#define maxAudFrames 32
#define maxVidFrames 32

//{{{
class cDecodeTransportStream : public cTransportStream {
public:
  cDecodeTransportStream() {}
  //{{{
  virtual ~cDecodeTransportStream() {

    avcodec_close (audContext);
    av_parser_close (audParser);

    avcodec_close (vidContext);
    av_parser_close (vidParser);
    }
  //}}}

  int64_t getAudPts() { return mAudPts; }
  int64_t getVidPts() { return mVidPts; }
  int64_t getAvDiff() { return mAudPts - mVidPts; }

  //{{{
  int64_t selectService (int index) {
  // select service by index, return basePts of pts when switched

    int64_t basePts = cTransportStream::selectService (index);

    for (int i= 0; i < maxVidFrames; i++)
      mYuvFrames[i].invalidate();

    return basePts;
    }
  //}}}
  //{{{
  cYuvFrame* findNearestVidFrame (int64_t pts) {
  // find nearestVidFrame to pts, nullPtr if no candidate

    cYuvFrame* yuvFrame = nullptr;

    int64_t nearest = 0;
    for (int i = 0; i < maxVidFrames; i++) {
      if (mYuvFrames[i].mPts) {
        if (!yuvFrame || (abs(mYuvFrames[i].mPts - pts) < nearest)) {
          yuvFrame = &mYuvFrames[i];
          nearest = abs(mYuvFrames[i].mPts - pts);
          }
        }
      }

    return yuvFrame;
    }
  //}}}

  bool hasLoadedAud (int playFrame) { return (mLoadAudFrame - playFrame) > 8; }
  //{{{
  void getAudPlay (int playFrame, int16_t*& samples, int& numSampleBytes, int64_t& pts) {

    if (playFrame < mLoadAudFrame) {
      samples = mAudFrames[playFrame % maxAudFrames].mSamples;
      numSampleBytes = mAudFrames[playFrame % maxAudFrames].mNumSampleBytes;
      pts = mAudFrames[playFrame % maxAudFrames].mPts;
      }
    else {
      samples = nullptr;
      numSampleBytes = 0;
      }
    }
  //}}}

  int mChannelSelector = -1;

protected:
  //{{{
  void decodeAudPes (cPidInfo* pidInfo) {

    if (!audParser) { // allocate decoder
      audParser = av_parser_init (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
      audCodec = avcodec_find_decoder (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
      audContext = avcodec_alloc_context3 (audCodec);
      avcodec_open2 (audContext, audCodec, NULL);
      }

    mAudPts = pidInfo->mPts;
    auto interpolatedPts = pidInfo->mPts;

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = pidInfo->mBuffer;
    avPacket.size = 0;

    auto pesLen = int (pidInfo->mBufPtr - pidInfo->mBuffer);
    pidInfo->mBufPtr = pidInfo->mBuffer;
    while (pesLen) {
      auto lenUsed = av_parser_parse2 (audParser, audContext, &avPacket.data, &avPacket.size, pidInfo->mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      pidInfo->mBufPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        auto got = 0;
        auto avFrame = av_frame_alloc();
        auto bytesUsed = avcodec_decode_audio4 (audContext, avFrame, &got, &avPacket);
        avPacket.data += bytesUsed;
        avPacket.size -= bytesUsed;

        if (got) {
          mAudFrames[mLoadAudFrame % maxAudFrames].set (interpolatedPts, 2, 48000, avFrame->nb_samples);
          //printf ("aud %d pts:%x samples:%d \n", mAudFrameLoaded, pts, mSamplesPerAacFrame);
          interpolatedPts += (avFrame->nb_samples * 90000) / 48000;

          auto samplePtr = (short*)mAudFrames[mLoadAudFrame % maxAudFrames].mSamples;
          if (audContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
            //{{{  16bit signed planar
            short* leftPtr = (short*)avFrame->data[0];
            short* rightPtr = (short*)avFrame->data[1];
            for (int i = 0; i < avFrame->nb_samples; i++) {
              *samplePtr++ = *leftPtr++;
              *samplePtr++ = *rightPtr++;
              }
            }
            //}}}
          else if (audContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
            //{{{  32bit float planar
            int leftChan = 0;
            int rightChan = 0;
            if ((mChannelSelector > -1) && (mChannelSelector < avFrame->channels)) {
              leftChan = mChannelSelector;
              rightChan = mChannelSelector;
              }

            float* leftPtr = (float*)avFrame->data[leftChan];
            float* rightPtr = (float*)avFrame->data[rightChan];
            for (int i = 0; i < avFrame->nb_samples; i++) {
              *samplePtr++ = (short)(*leftPtr++ * 0x8000);
              *samplePtr++ = (short)(*rightPtr++ * 0x8000);
              }
            }
            //}}}
          mLoadAudFrame++;
          }
        av_frame_free (&avFrame);
        }
      }
    }
  //}}}
  //{{{
  void decodeVidPes (cPidInfo* pidInfo) {

    if (!vidParser) { // allocate decoder
      vidParser = av_parser_init (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
      vidCodec = avcodec_find_decoder (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
      vidContext = avcodec_alloc_context3 (vidCodec);
      avcodec_open2 (vidContext, vidCodec, NULL);
      }

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = pidInfo->mBuffer;
    avPacket.size = 0;

    auto pesLen = int (pidInfo->mBufPtr - pidInfo->mBuffer);
    pidInfo->mBufPtr = pidInfo->mBuffer;
    while (pesLen) {
      auto lenUsed = av_parser_parse2 (vidParser, vidContext, &avPacket.data, &avPacket.size, pidInfo->mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      pidInfo->mBufPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        auto avFrame = av_frame_alloc();
        auto got = 0;
        auto bytesUsed = avcodec_decode_video2 (vidContext, avFrame, &got, &avPacket);
        avPacket.data += bytesUsed;
        avPacket.size -= bytesUsed;
        if (got) {
          //printf ("vid pts:%x dts:%x\n", pidInfo->mPts, pidInfo->mDts);
          if (((pidInfo->mStreamType == 27) && !pidInfo->mDts) ||
              (((pidInfo->mStreamType == 2) && pidInfo->mDts))) // use actual pts
            mVidPts = pidInfo->mPts;
          else // fake pts
            mVidPts += 90000/25;
          bestLoadVidFrame()->set (mVidPts, avFrame->data, avFrame->linesize, vidContext->width, vidContext->height);
          }
        av_frame_free (&avFrame);
        }
      }
    }
  //}}}

private:
  //{{{
  cYuvFrame* bestLoadVidFrame() {
  // find first unused or oldest yuvFrame

    cYuvFrame* yuvFrame = nullptr;
    for (int i = 0; i < maxVidFrames; i++)
      if (!mYuvFrames[i].mPts)
        return &mYuvFrames[i];
      else if (!yuvFrame || (yuvFrame->mPts > mYuvFrames[i].mPts))
        yuvFrame = &mYuvFrames[i];

    return yuvFrame;
    }
  //}}}

  // vars
  int64_t mAudPts = 0;
  int64_t mVidPts = 0;

  int mLoadAudFrame = 0;

  AVCodecParserContext* audParser  = nullptr;
  AVCodec* audCodec = nullptr;
  AVCodecContext* audContext = nullptr;

  AVCodecParserContext* vidParser = nullptr;
  AVCodec* vidCodec = nullptr;
  AVCodecContext* vidContext = nullptr;

  cAudFrame mAudFrames[maxAudFrames];
  cYuvFrame mYuvFrames[maxVidFrames];
  };
//}}}

class cTvWindow : public cD2dWindow {
public:
  //{{{
  cTvWindow() {
    mSilence = (int16_t*)malloc (2048*4);
    memset (mSilence, 0, 2048*4);
    }
  //}}}
  virtual ~cTvWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* arg) {

    initialise (title, width, height);

    if (arg) {
      // launch loaderThread
      thread ([=](){loader(arg);}).detach();

      // launch playerThread
      auto playerThread = std::thread ([=](){player();});
      SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
      playerThread.detach();

      // loop till quit
      messagePump();
      }
    }
  //}}}

protected:
//{{{
bool onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    case 0x20 : mPlaying = !mPlaying; break;
    case 0x21 : mFilePtr -= 5 * 0x4000*188; changed(); break;
    case 0x22 : mFilePtr += 5 * 0x4000*188; changed(); break;
    case 0x23 : break; // home
    case 0x24 : break; // end
    case 0x25 : mFilePtr -= keyInc() * 0x4000*188; changed(); break;
    case 0x27 : mFilePtr += keyInc() * 0x4000*188; changed(); break;
    case 0x26 : mPlaying = false; mPlayAudFrame -= 2; changed(); break;
    case 0x28 : mPlaying = false;  mPlayAudFrame += 2; changed(); break;

    case 0x2d : mTs.mChannelSelector++; break;
    case 0x2e : mTs.mChannelSelector--; break;

    case 0x30 :
    case 0x31 :
    case 0x32 :
    case 0x33 :
    case 0x34 :
    case 0x35 :
    case 0x36 :
    case 0x37 :
    case 0x38 :
    case 0x39 : selectService (key - '0'); break;

    default   : printf ("key %x\n", key);
    }
  return false;
  }
//}}}
//{{{
void onMouseProx (bool inClient, int x, int y) {

  auto showChannel = mShowChannel;
  auto transportStream = mShowTransportStream;

  mShowTransportStream = inClient && (x > getClientF().width - 100);
  mShowChannel = inClient && (x < 100);

  if ((transportStream != mShowTransportStream) || (showChannel != mShowChannel))
    changed();
  }
//}}}
//{{{
void onMouseDown (bool right, int x, int y) {

  if (y > getClientF().height - 20.0f) {
    mFilePtr = 188 * (int64_t)((x / getClientF().width) * (mFileSize / 188));
    changed();
    }

  else if (x < 80) {
    auto channel = (y / 20) - 1;
    if (channel >= 0)
      selectService (channel);
    }

  }
//}}}
//{{{
void onMouseUp (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved) {}
  }
//}}}
//{{{
void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  //mPlayTime -= xInc*3600;
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  makeBitmap (mTs.findNearestVidFrame (mAudPts));
  if (mBitmap)
    dc->DrawBitmap (mBitmap, RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
  else
    dc->Clear (ColorF(ColorF::Black));

  // draw title
  wchar_t wStr[200];
  auto textr = RectF(0, 0, getClientF().width, getClientF().height);
  swprintf (wStr, 200, L"%4.1f of %4.3fm - dis:%d - aud:%6d av:%6d chan:%d",
            (mAudPts - mBasePts)/90000.0f, mFileSize / 1000000.0f, mTs.getDiscontinuity(),
            (int)(mTs.getAudPts() - mAudPts), (int)mTs.getAvDiff(), mTs.mChannelSelector);
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), textr, getWhiteBrush());

  if (mShowChannel)
    mTs.drawServices (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  if (mShowTransportStream)
    mTs.drawPids (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());

  auto x = getClientF().width * (float)mFilePtr / (float)mFileSize;
  dc->FillRectangle (RectF(0, getClientF().height-10.0f, x, getClientF().height), getYellowBrush());
  }
//}}}

private:
  //{{{
  void selectService (int index) {

    auto basePts = mTs.selectService (index);
    if (basePts)
      mBasePts = basePts;
    }
  //}}}
  //{{{
  void makeBitmap (cYuvFrame* yuvFrame) {

    static const D2D1_BITMAP_PROPERTIES props = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 96.0f, 96.0f };

    if (yuvFrame) {
      if (yuvFrame->mPts != mBitmapPts) {
        mBitmapPts = yuvFrame->mPts;
        if (mBitmap)  {
          auto pixelSize = mBitmap->GetPixelSize();
          if ((pixelSize.width != yuvFrame->mWidth) || (pixelSize.height != yuvFrame->mHeight)) {
            mBitmap->Release();
            mBitmap = nullptr;
            }
          }
        if (!mBitmap) // create bitmap
          getDeviceContext()->CreateBitmap (SizeU(yuvFrame->mWidth, yuvFrame->mHeight), props, &mBitmap);

        // allocate 16 byte aligned bgraBuf
        auto bgraBufUnaligned = malloc ((yuvFrame->mWidth * 4 * 2) + 15);
        auto bgraBuf = (uint8_t*)(((size_t)(bgraBufUnaligned) + 15) & ~0xf);

        // convert yuv420 -> bitmap bgra
        auto yPtr = yuvFrame->mYbuf;
        auto uPtr = yuvFrame->mUbuf;
        auto vPtr = yuvFrame->mVbuf;
        for (auto i = 0; i < yuvFrame->mHeight; i += 2) {
          yuv420_rgba32_sse2 (yPtr, uPtr, vPtr, bgraBuf, yuvFrame->mWidth);
          yPtr += yuvFrame->mYStride;

          yuv420_rgba32_sse2 (yPtr, uPtr, vPtr, bgraBuf + (yuvFrame->mWidth * 4), yuvFrame->mWidth);
          yPtr += yuvFrame->mYStride;
          uPtr += yuvFrame->mUVStride;
          vPtr += yuvFrame->mUVStride;

          mBitmap->CopyFromMemory (&RectU(0, i, yuvFrame->mWidth, i + 2), bgraBuf, yuvFrame->mWidth * 4);
          }

        free (bgraBufUnaligned);
        }
      }
    else if (mBitmap) {
      mBitmap->Release();
      mBitmap = nullptr;
      }
    }
  //}}}

  //{{{
  void loader (wchar_t* wFileName) {

    av_register_all();

    uint8_t tsBuf[1024*188]; // 192k buffer, about a frame

    auto readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    mFilePtr = 0;
    int64_t lastFilePtr = 0;
    DWORD numberOfBytesRead = 0;
    while (true) {
      auto skip = mFilePtr != lastFilePtr;
      if (skip) {
        //{{{  skip file to new filePtr
        LARGE_INTEGER large;
        large.QuadPart = mFilePtr;
        SetFilePointerEx (readFile, large, NULL, FILE_BEGIN);
        }
        //}}}
      ReadFile (readFile, tsBuf, 1024 * 188, &numberOfBytesRead, NULL);
      if (numberOfBytesRead) {
        mTs.demux (tsBuf, tsBuf + numberOfBytesRead, skip);
        mFilePtr += numberOfBytesRead;
        lastFilePtr = mFilePtr;

        while (mTs.hasLoadedAud (mPlayAudFrame))
          Sleep (1);

        if (mTs.getSelectedAudPid() <= 0)
         selectService (0);
        }

      GetFileSizeEx (readFile, (PLARGE_INTEGER)(&mFileSize));
      }

    CloseHandle (readFile);
    }
  //}}}
  //{{{
  //void ffmpegFileLoader (wchar_t* wFileName) {

    //CoInitialize (NULL);
    //av_register_all();
    //avformat_network_init();

    //char filename[100];
    //wcstombs (filename, wFileName, 100);
    //AVCodecParserContext* vidParser = nullptr;
    //AVCodec* vidCodec = nullptr;
    //AVCodecContext* vidCodecContext = nullptr;

    //AVCodecParserContext* audParser  = nullptr;
    //AVCodec* audCodec = nullptr;
    //AVCodecContext* audCodecContext = nullptr;

    //AVCodecContext* subCodecContext = nullptr;
    //AVCodec* subCodec = nullptr;
    //{{{  av init
    //AVFormatContext* avFormatContext;
    //avformat_open_input (&avFormatContext, filename, NULL, NULL);
    //avformat_find_stream_info (avFormatContext, NULL);

    //int vidStream = -1;
    //int audStream = -1;
    //int subStream = -1;
    //for (unsigned int i = 0; i < avFormatContext->nb_streams; i++)
      //if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (vidStream == -1))
        //vidStream = i;
      //else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) && (audStream == -1))
        //audStream = i;
      //else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) && (subStream == -1))
        //subStream = i;

    //if (audStream >= 0) {
      //mTs.mChannels = avFormatContext->streams[audStream]->codec->channels;
      //mTs.mSampleRate = avFormatContext->streams[audStream]->codec->sample_rate;
      //}

    //printf ("filename:%s sampleRate:%d channels:%d streams:%d aud:%d vid:%d sub:%d\n",
            //filename, mTs.mSampleRate, mTs.mChannels, avFormatContext->nb_streams, audStream, vidStream, subStream);
    ////av_dump_format (avFormatContext, 0, filename, 0);

    //if (mTs.mChannels > 2)
      //mTs.mChannels = 2;
    //}}}
    //{{{  aud init
    //if (audStream >= 0) {
      //audCodecContext = avFormatContext->streams[audStream]->codec;
      //audCodec = avcodec_find_decoder (audCodecContext->codec_id);
      //avcodec_open2 (audCodecContext, audCodec, NULL);
      //}
    //}}}
    //{{{  vid init
    //if (vidStream >= 0) {
      //vidCodecContext = avFormatContext->streams[vidStream]->codec;
      //vidCodec = avcodec_find_decoder (vidCodecContext->codec_id);
      //avcodec_open2 (vidCodecContext, vidCodec, NULL);
      //}
    //}}}
    //{{{  sub init
    //if (subStream >= 0) {
      //subCodecContext = avFormatContext->streams[subStream]->codec;
      //subCodec = avcodec_find_decoder (subCodecContext->codec_id);
      //avcodec_open2 (subCodecContext, subCodec, NULL);
      //}
    //}}}

    //AVFrame* audFrame = av_frame_alloc();
    //AVFrame* vidFrame = av_frame_alloc();

    //AVSubtitle sub;
    //memset (&sub, 0, sizeof(AVSubtitle));

    //AVPacket avPacket;
    //while (true) {
      //while (av_read_frame (avFormatContext, &avPacket) >= 0) {
        //if (avPacket.stream_index == audStream) {
          //{{{  aud packet
          //int got = 0;
          //avcodec_decode_audio4 (audCodecContext, audFrame, &got, &avPacket);

          //if (got) {
            //mTs.mSamplesPerAacFrame = audFrame->nb_samples;
            //mTs.mAudFramesPerSec = (float)mTs.mSampleRate / mTs.mSamplesPerAacFrame;
            //mTs.mAudFrames[mTs.mAudFramesLoaded % maxAudFrames].set (0, 2, mTs.mSamplesPerAacFrame);

            //double valueL = 0;
            //double valueR = 0;
            //short* ptr = (short*)mTs.mAudFrames[mTs.mAudFramesLoaded % maxAudFrames].mSamples;
            //if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
              //{{{  16bit signed planar
              //short* lptr = (short*)audFrame->data[0];
              //short* rptr = (short*)audFrame->data[1];
              //for (int i = 0; i < mTs.mSamplesPerAacFrame; i++) {
                //*ptr = *lptr++;
                //valueL += pow(*ptr++, 2);
                //*ptr = *rptr++;
                //valueR += pow(*ptr++, 2);
                //}
              //}
              //}}}
            //else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
              //{{{  32bit float planar
              //float* lptr = (float*)audFrame->data[0];
              //float* rptr = (float*)audFrame->data[1];
              //for (int i = 0; i < mTs.mSamplesPerAacFrame; i++) {
                //*ptr = (short)(*lptr++ * 0x8000);
                //valueL += pow(*ptr++, 2);
                //*ptr = (short)(*rptr++ * 0x8000);
                //valueR += pow(*ptr++, 2);
                //}
              //}
              //}}}
            //mTs.mAudFrames[mTs.mAudFramesLoaded % maxAudFrames].mPowerLeft = (float)sqrt (valueL) / (mTs.mSamplesPerAacFrame * 2.0f);
            //mTs.mAudFrames[mTs.mAudFramesLoaded % maxAudFrames].mPowerRight = (float)sqrt (valueR) / (mTs.mSamplesPerAacFrame * 2.0f);

            //mTs.mAudFramesLoaded++;
            //}
          //}
          //}}}
        //else if (avPacket.stream_index == vidStream) {
          //{{{  vid packet
          //int got = 0;
          //avcodec_decode_video2 (vidCodecContext, vidFrame, &got, &avPacket);

          //if (got) {
            //mTs.mYuvFrames[mTs.mNextFreeVidFrame].set (0, vidFrame->data, vidFrame->linesize, vidCodecContext->width, vidCodecContext->height);
            //mTs.mNextFreeVidFrame = (mTs.mNextFreeVidFrame + 1) % maxVidFrames;
            //}
          //}
          //}}}
        //else if (avPacket.stream_index == subStream) {
          //{{{  sub packet
          //int got = 0;
          //avsubtitle_free (&sub);
          //avcodec_decode_subtitle2 (subCodecContext, &sub, &got, &avPacket);
          //}
          //}}}
        //av_free_packet (&avPacket);
        //}
      //}

    //av_free (vidFrame);
    //av_free (audFrame);

    //avcodec_close (vidCodecContext);
    //avcodec_close (audCodecContext);

    //avformat_close_input (&avFormatContext);
    //CoUninitialize();
    //}
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);  // for winAudio

    winAudioOpen (48000, 16, 2);

    mPlayAudFrame = 0;
    while (true) {
      int16_t* samples;
      int numSampleBytes;
      mTs.getAudPlay (mPlayAudFrame, samples, numSampleBytes, mAudPts);

      if (mPlaying && samples) {
        winAudioPlay (samples, numSampleBytes, 1.0f);
        mPlayAudFrame++;
        changed();
        }
      else
        winAudioPlay (mSilence, 4096, 1.0f);
      }

    CoUninitialize();
    }
  //}}}

  //{{{  vars
  bool mShowChannel = false;
  bool mShowTransportStream = false;

  // tsSection
  cDecodeTransportStream mTs;

  int16_t* mSilence;

  int64_t mBasePts = 0;
  int64_t mAudPts = 0;
  bool mPlaying = true;

  int mPlayAudFrame = 0;

  int64_t mFilePtr = 0;
  int64_t mFileSize = 0;

  int64_t mBitmapPts = 0;
  ID2D1Bitmap* mBitmap = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  #ifndef _DEBUG
    //FreeConsole();
  #endif

  cTvWindow tvWindow;
  tvWindow.run (L"tvWindow", 896, 504, argv[1]);
  }
//}}}
