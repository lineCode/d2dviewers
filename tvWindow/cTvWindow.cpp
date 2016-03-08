// cTvWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cTransportStream.h"

#include "../common/cYuvFrame.h"
//#include "../common/yuvrgb_sse2.h"
#include "../common/cMpeg2decoder.h"

#include "../common/cAudFrame.h"
#include "../common/cAudio.h"
#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")
//}}}
#define maxAudFrames 32
#define maxVidFrames 40
#define maxVidDecodes 12

//{{{
class cDecodeContext {
public:
  cDecodeContext (int pid) : mPid(pid) {}
  //{{{
  ~cDecodeContext() {
    if (mVidParser)
      av_parser_close (mVidParser);
    if (mVidContext)
      avcodec_close (mVidContext);
    }
  //}}}

  //{{{
  cYuvFrame* getYuvFrame (int i) {
    return &mYuvFrames[i];
    }
  //}}}
  //{{{
  cYuvFrame* getNearestVidFrame (int64_t pts) {
  // find nearestVidFrame to pts
  // - can return nullPtr if no frame loaded yet

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
  //{{{
  void invalidateFrames() {

    for (auto i = 0; i < maxVidFrames; i++)
      mYuvFrames[i].invalidate();

    mLoadVidFrame = 0;
    }
  //}}}

  // vars
  int mPid = 0;

  AVCodecParserContext* mVidParser = nullptr;
  AVCodec* mVidCodec = nullptr;
  AVCodecContext* mVidContext = nullptr;

  //cMpeg2decoder* mMpeg2decoder = nullptr;

  cYuvFrame mYuvFrames[maxVidFrames];
  int mLoadVidFrame = 0;

  int64_t mFakePts = 0;
  };
//}}}
//{{{
class cDecodeTransportStream : public cTransportStream {
public:
  cDecodeTransportStream() {}
  //{{{
  virtual ~cDecodeTransportStream() {

    if (audContext)
      avcodec_close (audContext);
    if (audParser)
      av_parser_close (audParser);
    }
  //}}}

  int getLoadAudFrame() { return mLoadAudFrame; }
  int64_t getBasePts() { return mBasePts; }
  //{{{
  void getAudSamples (int playFrame, int16_t*& samples, int& numSampleBytes, int64_t& pts) {

    if (playFrame < mLoadAudFrame) {
      auto audFrame = playFrame % maxAudFrames;
      samples = mAudFrames[audFrame].mSamples;
      if (mAudFrames[audFrame].mChannels > 2)
        numSampleBytes = mAudFrames[audFrame].mNumSampleBytes / (mAudFrames[audFrame].mChannels/2) ;
      else
        numSampleBytes = mAudFrames[audFrame].mNumSampleBytes;
      pts = mAudFrames[audFrame].mPts;
      }
    else {
      samples = nullptr;
      numSampleBytes = 0;
      }
    }
  //}}}
  //{{{
  cYuvFrame* getSelectedVid (int64_t pts) {

    if (mDecodeContexts[0])
      return mDecodeContexts[0]->getNearestVidFrame (pts);
    else
      return nullptr;
    }
  //}}}

  //{{{
  bool isServiceSelected() {
    return mSelectedAudPid > 0;
    }
  //}}}
  //{{{
  void selectService (int index) {

    auto j = 0;
    for (auto service : mServiceMap) {
      if (j == index) {
        int pcrPid = service.second.getPcrPid();
        tPidInfoMap::iterator pidInfoIt = mPidInfoMap.find (pcrPid);
        if (pidInfoIt != mPidInfoMap.end()) {
          mSelectedVidPid = service.second.getVidPid();
          mSelectedAudPid = service.second.getAudPid();
          printf ("selectService %d vid:%d aud:%d pcr:%d base:%lld\n",
                  j, mSelectedVidPid, mSelectedAudPid, pcrPid, pidInfoIt->second.mPcr);
          mBasePts = pidInfoIt->second.mPcr;

          //invalidateFrames();
    for (auto i= 0; i < maxAudFrames; i++)
      mAudFrames[i].invalidate();
    mLoadAudFrame = 0;

          return;
          }
        }
      else
        j++;
      }
    }
  //}}}
  //{{{
  void invalidateFrames() {

    if (mDecodeContexts[0])
      mDecodeContexts[0]->invalidateFrames();

    for (auto i= 0; i < maxAudFrames; i++)
      mAudFrames[i].invalidate();
    mLoadAudFrame = 0;
    }
  //}}}

  //{{{
  void drawDebug (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                  ID2D1SolidColorBrush* whiteBrush, ID2D1SolidColorBrush* blueBrush,
                  ID2D1SolidColorBrush* blackBrush, ID2D1SolidColorBrush* greyBrush, ID2D1SolidColorBrush* yellowBrush,
                  int64_t playAudPts) {

    float y = 40.0f;
    float h = 13.0f;
    float u = 18.0f;
    float vidFrameWidthPts = 90000.0f / 25.0f;
    float audFrameWidthPts = 90000.0f * 1152.0f / 48000.0f;
    float pixPerPts = u / audFrameWidthPts;
    float g = 1.0f;

    wchar_t wStr[10];
    for (auto i = 0; i < maxAudFrames; i++) {
      if (mAudFrames[i].mNumSamples) {
        audFrameWidthPts = 90000.0f * mAudFrames[i].mNumSamples / 48000.0f;
        pixPerPts = u / audFrameWidthPts;
        }

      int channels = mAudFrames[i].mChannels;
      float x = (client.width/2.0f) + float(mAudFrames[i].mPts - playAudPts) * pixPerPts;
      float w = u;

      for (auto j = 0; j < channels; j++) {
        //{{{  draw audFrame graphic
        float v = mAudFrames[i].mPower[j] / 2.0f;
        dc->FillRectangle (RectF(x + j*(w/channels), y-g-v , x+(j+1)*(w/channels)-g, y-g), blueBrush);
        }
        //}}}
      dc->FillRectangle (RectF(x, y, x+w-g, y+h), blueBrush);
      swprintf (wStr, 10, L"%d", i);
      dc->DrawText (wStr, (UINT32)wcslen(wStr), textFormat, RectF(x, y, x+w-g, y+h), blackBrush);
      }

    cDecodeContext* decodeContext = mDecodeContexts[0];
    if (decodeContext)
      for (auto i = 0; i < maxVidFrames; i++) {
        //{{{  draw vidFrame graphic
        cYuvFrame* yuvFrame = decodeContext->getYuvFrame (i);
        float x = (client.width/2.0f) + float(yuvFrame->mPts - playAudPts) * pixPerPts;
        float w = u * vidFrameWidthPts / audFrameWidthPts;

        dc->FillRectangle (RectF(x, y+h+g, x+w-g, y+h+g+h), yellowBrush);
        swprintf (wStr, 10, L"%d", i);
        dc->DrawText (wStr, (UINT32)wcslen(wStr), textFormat, RectF(x, y+h+g, x+w-g, y+h+g+h), blackBrush);

        dc->FillRectangle (RectF(x, y+h+g+h+g, x+w-g, y+h+g+h+g+h), whiteBrush);
        switch (yuvFrame->mPictType) {
          case 1: swprintf (wStr, 10, L"I"); break;
          case 2: swprintf (wStr, 10, L"P"); break;
          case 3: swprintf (wStr, 10, L"B"); break;
          default: swprintf (wStr, 10, L"%d", yuvFrame->mPictType); break;
          }
        dc->DrawText (wStr, (UINT32)wcslen(wStr), textFormat, RectF(x, y+h+g+h+g, x+w-g, y+h+g+h+g+h), blackBrush);

        float l = yuvFrame->mLen / 1000.0f;
        dc->FillRectangle (RectF(x, y+h+g+h+g+h+g, x+w-g, y+h+g+h+g+h+g+l), whiteBrush);
        }
        //}}}
    }
  //}}}

  // vars
  cDecodeContext* mDecodeContexts[maxVidDecodes];
  int mSelectedVidPid = 0;

protected:
  //{{{
  void decodeAudPes (cPidInfo* pidInfo) {

    if ((pidInfo->mStreamType == 17) || (pidInfo->mStreamType == 3) || (pidInfo->mStreamType == 4)) {
      if (pidInfo->mPid == mSelectedAudPid) {
        //printf ("decodeAudPes %d %d\n", pidInfo->mPid, pidInfo->mStreamType);
        if (!audParser) {
          //{{{  allocate decoder
          audParser = av_parser_init (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
          audCodec = avcodec_find_decoder (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
          audContext = avcodec_alloc_context3 (audCodec);
          avcodec_open2 (audContext, audCodec, NULL);
          }
          //}}}

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
              mAudFrames[mLoadAudFrame % maxAudFrames].set (interpolatedPts, avFrame->channels, 48000, avFrame->nb_samples);
              //printf ("aud %d pts:%x samples:%d \n", mAudFrameLoaded, pts, mSamplesPerAacFrame);
              interpolatedPts += (avFrame->nb_samples * 90000) / 48000;

              auto samplePtr = (short*)mAudFrames[mLoadAudFrame % maxAudFrames].mSamples;
              if (audContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
                //{{{  16bit signed planar
                double valueL = 0;
                double valueR = 0;

                short* leftPtr = (short*)avFrame->data[0];
                short* rightPtr = (short*)avFrame->data[1];
                for (int i = 0; i < avFrame->nb_samples; i++) {
                  *samplePtr = *leftPtr++;
                  valueL += pow (*samplePtr++, 2);
                  *samplePtr = *rightPtr++;
                  valueR += pow (*samplePtr++, 2);
                  }

                mAudFrames[mLoadAudFrame % maxAudFrames].mPower[0] = (float)sqrt (valueL) / (avFrame->nb_samples * 2.0f);
                mAudFrames[mLoadAudFrame % maxAudFrames].mPower[1] = (float)sqrt (valueR) / (avFrame->nb_samples * 2.0f);
                }
                //}}}
              else if (audContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
                //{{{  32bit float planar
                for (auto i = 0; i < avFrame->channels; i += 2) {
                  double valueL = 0;
                  double valueR = 0;

                  float* leftPtr = (float*)avFrame->data[i];
                  float* rightPtr = (float*)avFrame->data[i+1];
                  for (int i = 0; i < avFrame->nb_samples; i++) {
                    *samplePtr = (short)(*leftPtr++ * 0x8000);
                    valueL += pow (*samplePtr++, 2);
                    *samplePtr = (short)(*rightPtr++ * 0x8000);
                    valueR += pow (*samplePtr++, 2);
                    }

                  mAudFrames[mLoadAudFrame % maxAudFrames].mPower[i] = (float)sqrt (valueL) / (avFrame->nb_samples * 2.0f);
                  mAudFrames[mLoadAudFrame % maxAudFrames].mPower[i+1] = (float)sqrt (valueR) / (avFrame->nb_samples * 2.0f);
                  }
                }
                //}}}
              mLoadAudFrame++;
              }
            av_frame_free (&avFrame);
            }
          }
        }
      }
    //else
    //  printf ("**** unrecognised aud streamtype pid:%d type:%d\n", pidInfo->mPid, pidInfo->mStreamType);
    }
  //}}}
  //{{{
  void decodeVidPes (cPidInfo* pidInfo) {

    switch (pidInfo->mStreamType) {
      case 27:
        if (pidInfo->mPid != mSelectedVidPid)
          break;
      case 2: {
        // find decodeContext for pid
        int contextIndex = 0;
        cDecodeContext* decodeContext = nullptr;
        while ((contextIndex < maxVidDecodes) && mDecodeContexts[contextIndex]) {
          if (pidInfo->mPid == mDecodeContexts[contextIndex]->mPid) {
            decodeContext = mDecodeContexts[contextIndex];
            break;
            }
          contextIndex++;
          }
        if (!decodeContext) {
          //{{{  not found, create it with decoder
          //printf ("allocate %d pid:%d\n", contextIndex, pidInfo->mPid);
          decodeContext = new cDecodeContext (pidInfo->mPid);
          mDecodeContexts[contextIndex] = decodeContext;

          // allocate decoder
          decodeContext->mVidParser = av_parser_init (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
          decodeContext->mVidCodec = avcodec_find_decoder (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
          decodeContext->mVidContext = avcodec_alloc_context3 (decodeContext->mVidCodec);
          avcodec_open2 (decodeContext->mVidContext, decodeContext->mVidCodec, NULL);
          }
          //}}}

        AVPacket avPacket;
        av_init_packet (&avPacket);
        avPacket.data = pidInfo->mBuffer;
        avPacket.size = 0;

        auto pesPtr = pidInfo->mBuffer;
        auto pesLen = int (pidInfo->mBufPtr - pidInfo->mBuffer);
        //printf ("vidPes pid:%d i:%d - len:%d\n", pidInfo->mPid, contextIndex, pesLen);
        while (pesLen) {
          auto lenUsed = av_parser_parse2 (decodeContext->mVidParser, decodeContext->mVidContext,
                                           &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
          pesPtr += lenUsed;
          pesLen -= lenUsed;
          if (avPacket.data) {
            auto avFrame = av_frame_alloc();
            auto got = 0;
            auto bytesUsed = avcodec_decode_video2 (decodeContext->mVidContext, avFrame, &got, &avPacket);
            if (bytesUsed >= 0) {
              avPacket.data += bytesUsed;
              avPacket.size -= bytesUsed;
              if (got) {
                //{{{  got frame
                //printf ("pid:%d vid pts:%3.1f dts:%3.1f len:%d type:%d\n",
                //        pidInfo->mPid, (pidInfo->mPts-mBasePts)/3600.0f, (pidInfo->mDts-mBasePts)/3600.0f, pesLen, avFrame->pict_type);

                if (((pidInfo->mStreamType == 27) && !pidInfo->mDts) || ((pidInfo->mStreamType == 2) && pidInfo->mDts))
                  // use actual pts
                  decodeContext->mFakePts = pidInfo->mPts;
                else
                  // use fake pts
                  decodeContext->mFakePts += 90000/25;

                decodeContext->mYuvFrames [decodeContext->mLoadVidFrame % maxVidFrames].set (
                  decodeContext->mFakePts,
                  avFrame->data, avFrame->linesize, decodeContext->mVidContext->width, decodeContext->mVidContext->height,
                  pesLen, avFrame->pict_type);

                decodeContext->mLoadVidFrame++;
                }
                //}}}
              }
            av_frame_free (&avFrame);
            }
          }
        break;
        }
      //{{{  mpeg2decoder
      //decodeContext->mMpeg2decoder = new cMpeg2decoder();
      //uint8_t* pesPtr;
      //pidInfo->mDecoder->decodePes (pidInfo->mBuffer, pidInfo->mBufPtr, decodeContext->mFakePts, pesPtr);
      //}}}
      default:
        //printf ("**** unrecognised vid stream type pid:%d type:%d\n", pidInfo->mPid, pidInfo->mStreamType);
        break;
      }
    }
  //}}}

private:

  // vars
  int64_t mAudPts = 0;
  int64_t mVidPts = 0;
  int64_t mBasePts = 0;

  int mSelectedAudPid = 0;

  AVCodecParserContext* audParser  = nullptr;
  AVCodec* audCodec = nullptr;
  AVCodecContext* audContext = nullptr;

  int mLoadAudFrame = 0;
  cAudFrame mAudFrames[maxAudFrames];
  };
//}}}

class cTvWindow : public cD2dWindow, public cAudio {
public:
  //{{{
  cTvWindow() {
    for (auto i = 0; i < 10; i++) {
      mBitmaps[i] = nullptr;
      mBitmapPts[i] = 0;
      }
    }
  //}}}
  virtual ~cTvWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* arg) {

    initialise (title, width, height);

    getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
                                          DWRITE_FONT_WEIGHT_REGULAR,
                                          DWRITE_FONT_STYLE_NORMAL,
                                          DWRITE_FONT_STRETCH_NORMAL,
                                          12.0f, L"en-us", &mSmallTextFormat);

    mSmallTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);

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
    case 0x20 : mPlaying = !mPlaying; break;  // space

    case 0x21 : incPlayFrame (-5 * 0x4000*188); break; // page up
    case 0x22 : incPlayFrame (5 * 0x4000*188); break;  // page down
    case 0x25 : incPlayFrame (-keyInc() * 0x4000*188); break; // left arrow
    case 0x27 : incPlayFrame (keyInc() * 0x4000*188); break;  // right arrow
    case 0x26 : mPlaying = false; mPlayAudFrame -= 1; changed(); break;  // up arrow
    case 0x28 : mPlaying = false;  mPlayAudFrame += 1; changed(); break; // down arrow

    case 0x23 : break; // home
    case 0x24 : break; // end
    case 0x2d : mChannelSelector++; break; // insert
    case 0x2e : mChannelSelector--; break; // delete

    case 0x30 :
    case 0x31 :
    case 0x32 :
    case 0x33 :
    case 0x34 :
    case 0x35 :
    case 0x36 :
    case 0x37 :
    case 0x38 :
    case 0x39 : mTs.selectService (key - '0'); mPlayAudFrame = 0; break;

    default   : printf ("key %x\n", key);
    }
  return false;
  }
//}}}
//{{{
void onMouseWheel (int delta) {

  auto ratio = controlKeyDown ? 1.5f : shiftKeyDown ? 1.2f : 1.1f;
  if (delta > 0)
    ratio = 1.0f/ratio;

  setVolume (getVolume() * ratio);

  changed();
  }
//}}}
//{{{
void onMouseProx (bool inClient, int x, int y) {

  auto showChannel = mShowChannel;
  mShowChannel = inClient && (x < 100);

  auto transportStream = mShowTransportStream;
  mShowTransportStream = inClient && (x > getClientF().width - 100);

  auto showDebug = mShowDebug;
  mShowDebug = inClient;

  if ((showChannel != mShowChannel) || (showDebug != mShowDebug) || (transportStream != mShowTransportStream))
    changed();
  }
//}}}
//{{{
void onMouseDown (bool right, int x, int y) {

  if (y > getClientF().height - 20.0f) {
    mFilePtr = 188 * (int64_t)((x / getClientF().width) * (mFileSize / 188));
    mTs.invalidateFrames();
    mPlayAudFrame = 0;
    changed();
    mDownConsumed = true;
    }

  else if (x < 80) {
    auto channel = (y / 20) - 1;
    if (channel >= 0) {
      mTs.selectService (channel);
      mPlayAudFrame = 0;
      }
    mDownConsumed = true;
    }
  else
    mDownConsumed = false;
  }
//}}}
//{{{
void onMouseUp (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved && !mDownConsumed)
    mPlaying = !mPlaying;

  mDownConsumed = true;
  }
//}}}
//{{{
void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  //mPlayTime -= xInc*3600;
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  //dc->Clear (ColorF (ColorF::Black));

  int i = 0;
  int pip = 0;
  while (mTs.mDecodeContexts[i]) {
    if (makeBitmap (mTs.mDecodeContexts[i]->getNearestVidFrame (mAudPts), mBitmaps[i], mBitmapPts[i])) {
      if (mTs.mDecodeContexts[i]->mPid == mTs.mSelectedVidPid)
        dc->DrawBitmap (mBitmaps[i], RectF (0.0f, 0.0f, getClientF().width, getClientF().height));
      else {
        dc->DrawBitmap (mBitmaps[i], RectF ((pip%3) * getClientF().width/6, (pip/3) * getClientF().height/6,
                                            (((pip%3)+1) * getClientF().width/6)-4, (((pip/3)+1) * getClientF().height/6)-4));
        pip++;
        }
      }
    i++;
    }

  auto rMid = RectF ((getClientF().width/2)-1, 0, (getClientF().width/2)+1, getClientF().height);
  dc->FillRectangle (rMid, getGreyBrush());

  // yellow vol bar
  auto rVol= RectF (getClientF().width - 20,0, getClientF().width, getVolume() * 0.8f * getClientF().height);
  dc->FillRectangle (rVol, getYellowBrush());

  // draw title
  wchar_t wStr[200];
  swprintf (wStr, 200, L"%4.1f of %4.3fm - dis:%d chan:%d",
            (mAudPts - mTs.getBasePts()) / 90000.0f, mFileSize / 1000000.0f, mTs.getDiscontinuity(), mChannelSelector);
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF (0, 0, getClientF().width, getClientF().height), getWhiteBrush());

  if (mShowDebug)
    mTs.drawDebug (dc, getClientF(), mSmallTextFormat,
                   getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush(), getYellowBrush(), mAudPts);

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
  void incPlayFrame (int64_t inc) {

    mFilePtr += inc;
    mPlayAudFrame = 0;
    mTs.invalidateFrames();
    }
  //}}}

  //{{{
  void loader (wchar_t* wFileName) {

    av_register_all();

    uint8_t tsBuf[128*188];

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
      ReadFile (readFile, tsBuf, 128 * 188, &numberOfBytesRead, NULL);
      if (numberOfBytesRead) {
        mTs.demux (tsBuf, tsBuf + numberOfBytesRead, skip);
        mFilePtr += numberOfBytesRead;
        lastFilePtr = mFilePtr;

        while (mPlayAudFrame < mTs.getLoadAudFrame() - (maxAudFrames-6))
          Sleep (1);

        if (!mTs.isServiceSelected()) {
          mTs.selectService (0);
          mPlayAudFrame = 0;
          }
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

    audOpen (48000, 16, 2);

    mPlayAudFrame = 0;
    while (true) {
      int16_t* samples;
      int numSampleBytes;
      mTs.getAudSamples (mPlayAudFrame, samples, numSampleBytes, mAudPts);
      // could mChannelSelector samples

      if (mPlaying && samples) {
        audPlay (samples, numSampleBytes, 1.0f);
        mPlayAudFrame++;
        changed();
        }
      else
        audSilence();
      }

    CoUninitialize();
    }
  //}}}

  //{{{  vars
  cDecodeTransportStream mTs;

  int mChannelSelector = 0;

  int64_t mAudPts = 0;
  bool mPlaying = true;

  int mPlayAudFrame = 0;

  int64_t mFilePtr = 0;
  int64_t mFileSize = 0;

  bool mDownConsumed = false;
  bool mShowDebug = false;
  bool mShowChannel = false;
  bool mShowTransportStream = false;

  IDWriteTextFormat* mSmallTextFormat = nullptr;

  int64_t mBitmapPts[maxVidDecodes];
  ID2D1Bitmap* mBitmaps[maxVidDecodes];
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
