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
#define maxAudFrames 16
#define maxVidFrames 32

//{{{
class cDecodeTransportStream : public cTransportStream {
public:
  cDecodeTransportStream() {}
  //{{{
  virtual ~cDecodeTransportStream() {

    avcodec_close (audCodecContext);
    av_parser_close (audParser);

    avcodec_close (vidCodecContext);
    av_parser_close (vidParser);
    }
  //}}}

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

  unsigned char mChannels = 2;
  unsigned long mSampleRate = 48000;
  int mSamplesPerAacFrame = 0;
  float mAudFramesPerSec = 40;

  int mLoadAudFrame = 0;
  cAudFrame mAudFrames[maxAudFrames];

  int mNextFreeVidFrame = 0;
  cYuvFrame mYuvFrames[maxVidFrames];

protected:
  //{{{
  void decodeAudPes (cPidInfo* pidInfo) {

    if (!audParser) { // allocate decoder
      audParser = av_parser_init (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
      audCodec = avcodec_find_decoder (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
      audCodecContext = avcodec_alloc_context3 (audCodec);
      avcodec_open2 (audCodecContext, audCodec, NULL);
      }

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = pidInfo->mBuf;
    avPacket.size = 0;

    int64_t pts = pidInfo->mPts;
    int pesLen = int (pidInfo->mBufPtr - pidInfo->mBuf);
    pidInfo->mBufPtr = pidInfo->mBuf;
    while (pesLen) {
      int lenUsed = av_parser_parse2 (audParser, audCodecContext, &avPacket.data, &avPacket.size, pidInfo->mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      pidInfo->mBufPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        int got = 0;
        AVFrame* avFrame = av_frame_alloc();
        int bytesUsed = avcodec_decode_audio4 (audCodecContext, avFrame, &got, &avPacket);
        avPacket.data += bytesUsed;
        avPacket.size -= bytesUsed;

        if (got) {
          mSamplesPerAacFrame = avFrame->nb_samples;
          mAudFramesPerSec = (float)mSampleRate / mSamplesPerAacFrame;
          mAudFrames[mLoadAudFrame % maxAudFrames].set (pts, 2, mSamplesPerAacFrame);
          //printf ("aud %d pts:%x samples:%d \n", mAudFrameLoaded, pts, mSamplesPerAacFrame);
          pts += (mSamplesPerAacFrame * 90) / 48; // really 90000/48000
          short* samplePtr = (short*)mAudFrames[mLoadAudFrame % maxAudFrames].mSamples;
          if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
            //{{{  16bit signed planar
            short* leftPtr = (short*)avFrame->data[0];
            short* rightPtr = (short*)avFrame->data[1];
            for (int i = 0; i < mSamplesPerAacFrame; i++) {
              *samplePtr++ = *leftPtr++;
              *samplePtr++ = *rightPtr++;
              }
            }
            //}}}
          else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
            //{{{  32bit float planar
            float* leftPtr = (float*)avFrame->data[0];
            float* rightPtr = (float*)avFrame->data[1];
            for (int i = 0; i < mSamplesPerAacFrame; i++) {
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
      vidCodecContext = avcodec_alloc_context3 (vidCodec);
      avcodec_open2 (vidCodecContext, vidCodec, NULL);
      }

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = pidInfo->mBuf;
    avPacket.size = 0;

    int pesLen = int (pidInfo->mBufPtr - pidInfo->mBuf);
    pidInfo->mBufPtr = pidInfo->mBuf;
    while (pesLen) {
      int lenUsed = av_parser_parse2 (vidParser, vidCodecContext, &avPacket.data, &avPacket.size, pidInfo->mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      pidInfo->mBufPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        AVFrame* avFrame = av_frame_alloc();
        int got = 0;
        int bytesUsed = avcodec_decode_video2 (vidCodecContext, avFrame, &got, &avPacket);
        avPacket.data += bytesUsed;
        avPacket.size -= bytesUsed;
        if (got) {
          //printf ("vid pts:%x dts:%x\n", pidInfo->mPts, pidInfo->mDts);
          if (((pidInfo->mStreamType == 27) && !pidInfo->mDts) ||
              (((pidInfo->mStreamType == 2) && pidInfo->mDts))) // use pts
            mVidPts = pidInfo->mPts;
          else // fake pts
            mVidPts += 3600;
          mYuvFrames[mNextFreeVidFrame].set (
            mVidPts, avFrame->data, avFrame->linesize, vidCodecContext->width, vidCodecContext->height);
          mNextFreeVidFrame = (mNextFreeVidFrame + 1) % maxVidFrames;
          }
        av_frame_free (&avFrame);
        }
      }
    }
  //}}}

private:
  int64_t mVidPts = 0;

  AVCodecParserContext* vidParser = nullptr;
  AVCodec* vidCodec = nullptr;
  AVCodecContext* vidCodecContext = nullptr;

  AVCodecParserContext* audParser  = nullptr;
  AVCodec* audCodec = nullptr;
  AVCodecContext* audCodecContext = nullptr;
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
      thread ([=](){tsFileLoader(arg);}).detach();
      thread ([=](){player();}).detach();

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

    case 0x21 : mFilePtr -= 5 * 64*256*188; break;
    case 0x22 : mFilePtr += 5 * 64*256*188; break;

    case 0x23 : break; // home
    case 0x24 : break; // end

    case 0x25 : mFilePtr -= keyInc() * 64*256*188; break;
    case 0x27 : mFilePtr += keyInc() * 64*256*188; break;

    case 0x26 : mPlaying = false; mPlayAudFrame -= 2; changed(); break;
    case 0x28 : mPlaying = false;  mPlayAudFrame += 2; changed(); break;

    case 0x2d : break;
    case 0x2e : break;

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

  bool showChannel = mShowChannel;
  bool transportStream = mShowTransportStream;

  mShowTransportStream = inClient && (x > getClientF().width - 100);
  mShowChannel = inClient && (x < 100);

  if ((transportStream != mShowTransportStream) || (showChannel != mShowChannel))
    changed();
  }
//}}}
//{{{
void onMouseDown (bool right, int x, int y) {

  if (x < 80) {
    int channel = (y / 20) - 1;
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
    dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
  else
    dc->Clear (ColorF(ColorF::Black));

  // draw title
  wchar_t wStr[200];
  D2D1_RECT_F textr = D2D1::RectF(0, 0, getClientF().width, getClientF().height);
  swprintf (wStr, 200, L"%4.1f filePtr:%4.3fm dis:%d", (mAudPts- mBasePts)/90000.0f, mFilePtr/1000000.0f, mTs.getDiscontinuity());
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), textr, getWhiteBrush());

  if (mShowChannel)
    mTs.drawServices (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  if (mShowTransportStream)
    mTs.drawPids (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  }
//}}}

private:
  //{{{
  void selectService (int index) {

    int64_t basePts = mTs.selectService (index);
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
  void tsFileLoader (wchar_t* wFileName) {

    CoInitialize (NULL);

    av_register_all();

    uint8_t tsBuf[256*188];

    HANDLE readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    mFilePtr = 0;
    DWORD numberOfBytesRead = 0;
    while (ReadFile (readFile, tsBuf, 256*188, &numberOfBytesRead, NULL)) {
      if (numberOfBytesRead) {
        mTs.tsParser (tsBuf, tsBuf + numberOfBytesRead);
        while (mTs.mLoadAudFrame - mPlayAudFrame > 8)
          Sleep (1);

        if (mTs.getSelectedAudPid() <= 0)
         selectService (0);

        mFilePtr += numberOfBytesRead;
        SetFilePointer (readFile, mFilePtr, NULL, FILE_BEGIN);
        }
      }

    CloseHandle (readFile);

    CoUninitialize();
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);

    while (mTs.mLoadAudFrame < 1)
      Sleep (40);

    winAudioOpen (mTs.mSampleRate, 16, mTs.mChannels);

    mPlayAudFrame = 0;
    while (true) {
      winAudioPlay ((mPlaying && (mPlayAudFrame < mTs.mLoadAudFrame)) ?
        mTs.mAudFrames[mPlayAudFrame % maxAudFrames].mSamples : mSilence, mTs.mChannels*mTs.mSamplesPerAacFrame*2, 1.0f);

      if (mPlayAudFrame < mTs.mLoadAudFrame) {
        mAudPts = mTs.mAudFrames[mPlayAudFrame % maxAudFrames].mPts;
        if (mPlaying) {
          mPlayAudFrame++;
          changed();
          }
        }
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
  int64_t mAudPts = 0;

  int mPlayAudFrame = 0;
  bool mPlaying = true;
  int64_t mBasePts = 0;

  long mFilePtr = 0;

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
