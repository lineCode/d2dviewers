// cTvWindow.cpp
//{{{  includes
#include "pch.h"

#include "../../shared/utils/utils.h"
#include "../../shared/d2dwindow/cD2dWindow.h"

#include "../../shared/decoders/cTransportStream.h"

#include "../../shared/video/cYuvFrame.h"
#include "../common/cAudFrame.h"
#include "../../shared/audio/cWinAudio.h"

#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")

using namespace std;
//}}}
#define kMaxDecodes 10
#define kMaxVidFrames 40
#define kMaxAudFrames 32
#define kAudLoadAhead 6

//{{{
class cDecodeTransportStream : public cTransportStream {
public:
  cDecodeTransportStream() {}
  //{{{
  virtual ~cDecodeTransportStream() {

    if (mAudContext)
      avcodec_close (mAudContext);
    if (mAudParser)
      av_parser_close (mAudParser);

    if (mVidParser)
      av_parser_close (mVidParser);
    if (mVidContext)
      avcodec_close (mVidContext);
    }
  //}}}

  uint64_t getBasePts() { return mBasePts; }
  uint64_t getAudPts() { return mAudPts; }
  uint64_t getVidPts() { return mVidPts; }

  int getLoadAudFrame() { return mLoadAudFrame; }
  //{{{
  void getAudSamples (int playFrame, int16_t*& samples, int& numSampleBytes, uint64_t& pts) {

    if (playFrame < mLoadAudFrame) {
      auto audFrame = playFrame % kMaxAudFrames;
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
  cYuvFrame* getNearestVidFrame (uint64_t pts) {
  // find nearestVidFrame to pts
  // - can return nullPtr if no frame loaded yet

    cYuvFrame* yuvFrame = nullptr;

    uint64_t nearest = 0;
    for (int i = 0; i < kMaxVidFrames; i++) {
      if (mYuvFrames[i].mPts) {
        uint64_t absDiff = mYuvFrames[i].mPts < pts ? pts - mYuvFrames[i].mPts : mYuvFrames[i].mPts - pts;
        if (!yuvFrame || (absDiff < nearest)) {
          yuvFrame = &mYuvFrames[i];
          nearest = absDiff;
          }
        }
      }

    return yuvFrame;
    }
  //}}}
  //{{{
  cYuvFrame* getYuvFrame (int i) {
    return &mYuvFrames[i];
    }
  //}}}
  //{{{
  void invalidateFrames() {

    mLoadAudFrame = 0;
    for (auto i = 0; i < kMaxAudFrames; i++)
      mAudFrames[i].invalidate();

    mLoadVidFrame = 0;
    for (auto i = 0; i < kMaxVidFrames; i++)
      mYuvFrames[i].invalidate();
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
          for (auto i= 0; i < kMaxAudFrames; i++)
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

  // vars
  //{{{
  void drawServices (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                     ID2D1SolidColorBrush* whiteBrush, ID2D1SolidColorBrush* blueBrush,
                     ID2D1SolidColorBrush* blackBrush, ID2D1SolidColorBrush* greyBrush) {

    auto r = D2D1::RectF(0, 20.0f, client.width, client.height);
    for (auto service : mServiceMap) {
      string str = service.second.getName() + " " + service.second.getNow()->mTitle;
      dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), textFormat, r, whiteBrush);
      r.top += 20.0f;
      }
    }
  //}}}
  //{{{
  void drawPids (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                 ID2D1SolidColorBrush* whiteBrush, ID2D1SolidColorBrush* blueBrush,
                 ID2D1SolidColorBrush* blackBrush, ID2D1SolidColorBrush* greyBrush) {

    if (!mPidInfoMap.empty()) {
      float lineY = 20.0f;
      string str = mTimeStr + " services:" + to_string (mServiceMap.size());
      auto r = D2D1::RectF(0, 20.0f, client.width, client.height);
      dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), textFormat, r, whiteBrush);
      lineY+= 20.0f;

      for (auto pidInfo : mPidInfoMap) {
        float total = (float)pidInfo.second.mTotal;
        if (total > mLargestPid)
          mLargestPid = total;
        auto len = (total / mLargestPid) * (client.width - 50.0f);
        dc->FillRectangle (RectF(40.0f, lineY+6.0f, 40.0f + len, lineY+22.0f), blueBrush);

        r.top = lineY;
        str = to_string (pidInfo.first) +
              ' ' + to_string (pidInfo.second.mStreamType) +
              ' ' + pidInfo.second.mInfo +
              ' ' + to_string (pidInfo.second.mTotal) +
              ':' + to_string (pidInfo.second.mDisContinuity);
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                      textFormat, r, whiteBrush);
        lineY += 20.0f;
        }
      }
    }
  //}}}
  //{{{
  void drawDebug (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                  ID2D1SolidColorBrush* whiteBrush, ID2D1SolidColorBrush* blueBrush,
                  ID2D1SolidColorBrush* blackBrush, ID2D1SolidColorBrush* greyBrush, ID2D1SolidColorBrush* yellowBrush,
                  uint64_t playAudPts) {

    float y = 40.0f;
    float h = 13.0f;
    float u = 18.0f;
    float vidFrameWidthPts = 90000.0f / 25.0f;
    float audFrameWidthPts = 90000.0f * 1152.0f / 48000.0f;
    float pixPerPts = u / audFrameWidthPts;
    float g = 1.0f;

    auto rMid = RectF ((client.width/2)-1, 0, (client.width/2)+1, y+y+y);
    dc->FillRectangle (rMid, greyBrush);

    for (auto i = 0; i < kMaxAudFrames; i++) {
      if (mAudFrames[i].mNumSamples) {
        audFrameWidthPts = 90000.0f * mAudFrames[i].mNumSamples / 48000.0f;
        pixPerPts = u / audFrameWidthPts;
        }

      int channels = mAudFrames[i].mChannels;

      // make sure we get a signed diff from unsigned pts
      int64_t diff = mAudFrames[i].mPts - playAudPts;
      float x = (client.width/2.0f) + float(diff) * pixPerPts;
      float w = u;

      for (auto j = 0; j < channels; j++) {
        //{{{  draw audFrame graphic
        float v = mAudFrames[i].mPower[j] / 2.0f;
        dc->FillRectangle (RectF(x + j*(w/channels), y-g-v , x+(j+1)*(w/channels)-g, y-g), blueBrush);
        }
        //}}}
      dc->FillRectangle (RectF(x, y, x+w-g, y+h), blueBrush);
      wstring wstr (to_wstring (i));
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, RectF(x, y, x+w-g, y+h), blackBrush);
      }

    for (auto i = 0; i < kMaxVidFrames; i++) {
      //{{{  draw vidFrame graphic
      cYuvFrame* yuvFrame = getYuvFrame (i);

      // make sure we get a signed diff from unsigned pts
      int64_t diff = yuvFrame->mPts - playAudPts;
      float x = (client.width/2.0f) + float(diff) * pixPerPts;
      float w = u * vidFrameWidthPts / audFrameWidthPts;

      dc->FillRectangle (RectF(x, y+h+g, x+w-g, y+h+g+h), yellowBrush);
      wstring wstr (to_wstring (i));
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, RectF(x, y+h+g, x+w-g, y+h+g+h), blackBrush);

      dc->FillRectangle (RectF(x, y+h+g+h+g, x+w-g, y+h+g+h+g+h), whiteBrush);
      switch (yuvFrame->mPictType) {
        case 1: wstr = L"I"; break;
        case 2: wstr = L"P"; break;
        case 3: wstr = L"B"; break;
        default: wstr = to_wstring (yuvFrame->mPictType); break;
        }
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, RectF(x, y+h+g+h+g, x+w-g, y+h+g+h+g+h), blackBrush);

      float l = yuvFrame->mLen / 1000.0f;
      dc->FillRectangle (RectF(x, y+h+g+h+g+h+g, x+w-g, y+h+g+h+g+h+g+l), whiteBrush);
      }
      //}}}
    }
  //}}}

protected:
  //{{{
  void decodeAudPes (cPidInfo* pidInfo) {

    if ((pidInfo->mStreamType == 17) || (pidInfo->mStreamType == 3) || (pidInfo->mStreamType == 4)) {
      if (pidInfo->mPid == mSelectedAudPid) {
        if (!mAudParser) {
          //{{{  allocate decoder
          mAudParser = av_parser_init (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
          mAudCodec = avcodec_find_decoder (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
          mAudContext = avcodec_alloc_context3 (mAudCodec);
          avcodec_open2 (mAudContext, mAudCodec, NULL);
          }
          //}}}

        printf ("A %4.3f\n", pidInfo->mPts / 90000.0f);

        mAudPts = pidInfo->mPts;
        mInterpolatedAudPts = pidInfo->mPts;

        AVPacket avPacket;
        av_init_packet (&avPacket);
        avPacket.data = pidInfo->mBuffer;
        avPacket.size = 0;

        auto pesLen = int (pidInfo->mBufPtr - pidInfo->mBuffer);
        pidInfo->mBufPtr = pidInfo->mBuffer;
        while (pesLen) {
          auto lenUsed = av_parser_parse2 (mAudParser, mAudContext, &avPacket.data, &avPacket.size, pidInfo->mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
          pidInfo->mBufPtr += lenUsed;
          pesLen -= lenUsed;

          if (avPacket.data) {
            auto got = 0;
            auto avFrame = av_frame_alloc();
            auto bytesUsed = avcodec_decode_audio4 (mAudContext, avFrame, &got, &avPacket);
            avPacket.data += bytesUsed;
            avPacket.size -= bytesUsed;

            if (got) {
              mAudFrames[mLoadAudFrame % kMaxAudFrames].set (mInterpolatedAudPts, avFrame->channels, 48000, avFrame->nb_samples);
              mInterpolatedAudPts += (avFrame->nb_samples * 90000) / 48000;

              double values[6] = { 0,0,0,0,0,0 };
              auto samplePtr = (short*)mAudFrames[mLoadAudFrame % kMaxAudFrames].mSamples;
              if (mAudContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
                //{{{  calc 16bit signed planar power
                short* chanPtr[6];
                for (auto j = 0; j < avFrame->channels; j++)
                  chanPtr[j] = (short*)avFrame->data[j];

                for (auto i = 0; i < avFrame->nb_samples; i++) {
                  for (auto j = 0; j < avFrame->channels; j++) {
                    auto sample = *chanPtr[j]++;
                    values[j] += pow (sample, 2);
                    *samplePtr++ = sample;
                    }
                  }
                }
                //}}}
              else if (mAudContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
                //{{{  calc 32bit float planar power
                float* chanPtr[6];
                for (auto j = 0; j < avFrame->channels; j++)
                  chanPtr[j] = (float*)avFrame->data[j];

                for (auto i = 0; i < avFrame->nb_samples; i++) {
                  for (auto j = 0; j < avFrame->channels; j++) {
                    auto sample = (short)(*chanPtr[j]++ * 0x8000);
                    values[j] += pow (sample, 2);
                    if (avFrame->channels == 6) {
                      // 5.1 channels
                      if (j == 2) {
                       // select centre channel
                       *samplePtr++ = sample;
                       *samplePtr++ = sample;
                        }
                      }
                    else if (j < 2)
                      *samplePtr++ = sample;
                    }
                  }
                }
                //}}}
              for (auto j = 0; j < avFrame->channels; j++)
                mAudFrames[mLoadAudFrame % kMaxAudFrames].mPower[j] = (float)sqrt (values[j]) / (avFrame->nb_samples * 2.0f);
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

    if ((pidInfo->mStreamType == 2) || (pidInfo->mStreamType == 27)) {
      if (!mVidParser) {
        //{{{  allocate decoder
        //printf ("allocate %d pid:%d\n", contextIndex, pidInfo->mPid);
        // allocate decoder
        mVidParser = av_parser_init (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
        mVidCodec = avcodec_find_decoder (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
        mVidContext = avcodec_alloc_context3 (mVidCodec);
        avcodec_open2 (mVidContext, mVidCodec, NULL);
        }
        //}}}

      mVidPts = pidInfo->mPts;

      AVPacket avPacket;
      av_init_packet (&avPacket);
      avPacket.data = pidInfo->mBuffer;
      avPacket.size = 0;

      auto pesPtr = pidInfo->mBuffer;
      auto pesLen = int (pidInfo->mBufPtr - pidInfo->mBuffer);
      while (pesLen) {
        auto lenUsed = av_parser_parse2 (mVidParser, mVidContext,
                                         &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
        pesPtr += lenUsed;
        pesLen -= lenUsed;
        if (avPacket.data) {
          auto avFrame = av_frame_alloc();
          auto got = 0;
          auto bytesUsed = avcodec_decode_video2 (mVidContext, avFrame, &got, &avPacket);
          if (bytesUsed >= 0) {
            avPacket.data += bytesUsed;
            avPacket.size -= bytesUsed;
            if (got) {
              //{{{  got frame
              //printf ("pid:%d vid pts:%3.1f dts:%3.1f len:%d type:%d\n",
              //        pidInfo->mPid, (pidInfo->mPts-mBasePts)/3600.0f, (pidInfo->mDts-mBasePts)/3600.0f, pesLen, avFrame->pict_type);
              if (((pidInfo->mStreamType == 27) && !pidInfo->mDts) || ((pidInfo->mStreamType == 2) && pidInfo->mDts))
                // use actual pts
                mInterpolatedVidPts = pidInfo->mPts;
              else
                // use fake pts
                mInterpolatedVidPts += 90000/25;

              mYuvFrames [mLoadVidFrame % kMaxVidFrames].set (mInterpolatedVidPts,
                                                              avFrame->data, avFrame->linesize,
                                                              mVidContext->width, mVidContext->height,
                                                              pesLen, avFrame->pict_type);

              mLoadVidFrame++;

              printf ("V %4.3f %4.3f t:%d l:%d\n",
                      pidInfo->mPts/90000.0f, pidInfo->mDts/90000.0f, avFrame->pict_type, pesLen);
              }
              //}}}
            }
          av_frame_free (&avFrame);
          }
        }
      }
    }
  //}}}

private:
  int mSelectedAudPid = 0;
  int mSelectedVidPid = 0;
  float mLargestPid = 10000.0f;

  uint64_t mBasePts = 0;
  uint64_t mAudPts = 0;
  uint64_t mVidPts = 0;
  uint64_t mInterpolatedAudPts = 0;
  uint64_t mInterpolatedVidPts = 0;

  AVCodec* mAudCodec = nullptr;
  AVCodecContext* mAudContext = nullptr;
  AVCodecParserContext* mAudParser = nullptr;

  AVCodec* mVidCodec = nullptr;
  AVCodecContext* mVidContext = nullptr;
  AVCodecParserContext* mVidParser = nullptr;

  int mLoadAudFrame = 0;
  cAudFrame mAudFrames[kMaxAudFrames];
  int mLoadVidFrame = 0;
  cYuvFrame mYuvFrames[kMaxVidFrames];

  };
//}}}

class cTvWindow : public cD2dWindow, public cWinAudio {
public:
  cTvWindow() {}
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
      auto playerThread = thread ([=](){player();});
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
    case 0x23 : break; // home
    case 0x24 : break; // end
    case 0x25 : incPlayFrame (-keyInc() * 0x4000*188); break; // left arrow
    case 0x27 : incPlayFrame (keyInc() * 0x4000*188); break;  // right arrow
    case 0x26 : mPlaying = false; mPlayAudFrame -= 1; changed(); break;  // up arrow
    case 0x28 : mPlaying = false;  mPlayAudFrame += 1; changed(); break; // down arrow
    case 0x2d : mServiceSelector++; break; // insert
    case 0x2e : mServiceSelector--; break; // delete

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

  auto ratio = mControlKeyDown ? 1.5f : mShiftKeyDown ? 1.2f : 1.1f;
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
  }
//}}}
//{{{
void onMouseUp (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved && !mDownConsumed)
    mPlaying = !mPlaying;
  }
//}}}
//{{{
void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  //mPlayTime -= xInc*3600;
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  if (makeBitmap (mTs.getNearestVidFrame (mPlayAudPts), mBitmap, mBitmapPts))
    dc->DrawBitmap (mBitmap, RectF (0.0f, 0.0f, getClientF().width, getClientF().height));

  // yellow vol bar
  dc->FillRectangle (RectF (getClientF().width - 20,0, getClientF().width, getVolume() * 0.8f * getClientF().height),
                     getYellowBrush());

  // draw title
  wstringstream str;
  str << (mPlayAudPts - mTs.getBasePts()) / 90000.0f
      << L" " << mFileSize / 1000000.0f << L"m "
      << L" cont:" << mTs.getDiscontinuity()
      << L" a:" << (mTs.getAudPts() - mTs.getBasePts()) / 90000.0f
      << L" v:" << (mTs.getVidPts() - mTs.getBasePts()) / 90000.0f
      << L" service:" << mServiceSelector;
  dc->DrawText (str.str().data(), (uint32_t)str.str().size(), getTextFormat(),
                RectF (0, 0, getClientF().width, getClientF().height), getWhiteBrush());

  if (mShowDebug) // show frames debug
    mTs.drawDebug (dc, getClientF(), mSmallTextFormat,
                   getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush(), getYellowBrush(), mPlayAudPts);
  if (mShowChannel) // show services
    mTs.drawServices (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  if (mShowTransportStream) // show pids
    mTs.drawPids (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());

  // show file position
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

        while (mPlayAudFrame < mTs.getLoadAudFrame() - (kMaxAudFrames - kAudLoadAhead))
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
  void player() {

    CoInitialize (NULL);  // for winAudio

    audOpen (48000, 16, 2);

    mPlayAudFrame = 0;
    while (true) {
      if (mPlaying) {
        int16_t* samples;
        int numSampleBytes;
        mTs.getAudSamples (mPlayAudFrame, samples, numSampleBytes, mPlayAudPts);
        if (samples) {
          audPlay (samples, numSampleBytes, 1.0f);
          mPlayAudFrame++;
          changed();
          }
        }
      else
        audSilence();

      }

    CoUninitialize();
    }
  //}}}

  //{{{  vars
  int64_t mFilePtr = 0;
  int64_t mFileSize = 0;

  cDecodeTransportStream mTs;
  int mServiceSelector = 0;

  uint64_t mPlayAudPts = 0;
  int mPlayAudFrame = 0;
  bool mPlaying = true;

  bool mShowDebug = false;
  bool mShowChannel = false;
  bool mShowTransportStream = false;

  uint64_t mBitmapPts = 0;
  ID2D1Bitmap* mBitmap = nullptr;

  IDWriteTextFormat* mSmallTextFormat = nullptr;
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
