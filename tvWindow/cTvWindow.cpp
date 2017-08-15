// cTvWindow.cpp
//{{{  includes
#include "pch.h"

#include "../../shared/utils/utils.h"
#include "../../shared/d2dwindow/cD2dWindow.h"

#include "../../shared/decoders/cTransportStream.h"

#include "../../shared/video/cYuvFrame.h"
#include "../../shared/audio/cAudFrame.h"
#include "../../shared/audio/cWinAudio.h"

#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")

using namespace std;
//}}}
const bool kDebugPes = false;
const int kMaxVidFrames = 64;
const int kMaxAudFrames = 64;
//{{{
class cDecodeTransportStream : public cTransportStream {
public:
  //{{{
  cDecodeTransportStream() {
    for (auto i = 0; i < kMaxAudFrames; i++)
      mAudFrames.push_back (new cAudFrame());
    for (auto i = 0; i < kMaxVidFrames; i++)
      mVidFrames.push_back (new cYuvFrame());
    }
  //}}}
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

  uint64_t getLastAudPts() { return mLastAudPts ? mLastAudPts - mBasePts : 0; }
  uint64_t getLastVidPts() { return mLastVidPts ? mLastVidPts - mBasePts : 0; }
  float getPixPerPts() { return mPixPerPts; }

  //{{{
  cAudFrame* getAudByPts (uint64_t pts) {
  // find audFrame containing pts
  // - returns nullPtr if no frame loaded yet

    pts += mBasePts;
    for (auto frame : mAudFrames) {
      if (frame->mPts) {
        auto ptsWidth = uint64_t ((90000 * frame->mNumSamples) / 48000);
        if ((pts >= frame->mPts) && (pts < frame->mPts + ptsWidth))
          return frame;
        }
      }

    return nullptr;
    }
  //}}}
  //{{{
  cYuvFrame* getVidByPts (uint64_t pts) {
  // find vidFrame containing pts, else nearestVidFrame to pts
  // - returns nullPtr if no frame loaded yet

    cYuvFrame* nearestFrame = nullptr;

    pts += mBasePts;

    uint64_t nearest = 0;
    for (auto frame : mVidFrames) {
      if (frame->mPts) {
        auto ptsWidth = uint64_t (90000 / 25);
        if ((pts >= frame->mPts) && (pts < frame->mPts + ptsWidth))
          return frame;
        uint64_t absDiff = pts > frame->mPts ? pts - frame->mPts : frame->mPts - pts;
        if (!nearestFrame || (absDiff < nearest)) {
          nearestFrame = frame;
          nearest = absDiff;
          }
        }
      }

    return nearestFrame;
    }
  //}}}

  //{{{
  void invalidateFrames() {

    invalidateAudFrames();
    invalidateVidFrames();
    }
  //}}}

  //{{{
  bool isServiceSelected() {
    return mSelectedAudPid > 0;
    }
  //}}}
  //{{{
  void selectService (int index) {

    auto serviceIndex = 0;
    for (auto service : mServiceMap) {
      if (serviceIndex == index) {
        int pcrPid = service.second.getPcrPid();
        tPidInfoMap::iterator pidInfoIt = mPidInfoMap.find (pcrPid);
        if (pidInfoIt != mPidInfoMap.end()) {
          mSelectedVidPid = service.second.getVidPid();
          mSelectedAudPid = service.second.getAudPid();
          printf ("selectService %d vid:%d aud:%d pcr:%d base:%4.3f\n",
            serviceIndex, mSelectedVidPid, mSelectedAudPid, pcrPid, pidInfoIt->second.mPcr/90000.0f);
          mBasePts = pidInfoIt->second.mPcr;

          invalidateAudFrames();

          return;
          }
        }
      else
        serviceIndex++;
      }
    }
  //}}}

  // vars
  //{{{
  void drawServices (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                     ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                     ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* grey) {

    auto r = D2D1::RectF(0, 20.0f, client.width, client.height);
    for (auto service : mServiceMap) {
      string str = service.second.getName() + " " + service.second.getNow()->mTitle;
      dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), textFormat, r, white);
      r.top += 20.0f;
      }
    }
  //}}}
  //{{{
  void drawPids (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                 ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                 ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* grey) {

    if (!mPidInfoMap.empty()) {
      float lineY = 20.0f;
      string str = mTimeStr + " services:" + to_string (mServiceMap.size());
      auto r = D2D1::RectF(0, 20.0f, client.width, client.height);
      dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), textFormat, r, white);
      lineY += 20.0f;

      float mLargestPid = 10000.0f;
      for (auto pidInfo : mPidInfoMap) {
        float total = (float)pidInfo.second.mTotal;
        if (total > mLargestPid)
          mLargestPid = total;
        auto len = (total / mLargestPid) * (client.width - 50.0f);
        dc->FillRectangle (RectF(40.0f, lineY+6.0f, 40.0f + len, lineY+22.0f), blue);

        r.top = lineY;
        str = to_string (pidInfo.first) +
              ' ' + to_string (pidInfo.second.mStreamType) +
              ' ' + pidInfo.second.mInfo +
              ' ' + to_string (pidInfo.second.mTotal) +
              ':' + to_string (pidInfo.second.mDisContinuity);
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                      textFormat, r, white);
        lineY += 20.0f;
        }
      }
    }
  //}}}
  //{{{
  void drawDebug (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                  ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                  ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* grey, ID2D1SolidColorBrush* yellow,
                  uint64_t playPts) {

    auto y = 40.0f;
    auto h = 13.0f;
    auto u = 18.0f;
    auto vidFrameWidthPts = 90000.0f / 25.0f;
    auto audFrameWidthPts = 90000.0f * 1152.0f / 48000.0f;
    mPixPerPts = u / audFrameWidthPts;
    auto g = 1.0f;

    auto rMid = RectF ((client.width/2)-1, 0, (client.width/2)+1, y+y+y);
    dc->FillRectangle (rMid, grey);

    auto index = 1;
    for (auto audFrame : mAudFrames) {
      //{{{  draw audFrame graphic
      if (audFrame->mNumSamples) {
        audFrameWidthPts = 90000.0f * audFrame->mNumSamples / 48000.0f;
        mPixPerPts = u / audFrameWidthPts;
        }

      // make sure we get a signed diff from unsigned pts
      int64_t diff = audFrame->mPts - mBasePts - playPts;
      float x = (client.width/2.0f) + float(diff) * mPixPerPts;
      float w = u;

      int channels = audFrame->mChannels;
      for (auto channel = 0; channel < channels; channel++) {
        // draw channel
        float v = audFrame->mPower[channel] / 2.0f;
        dc->FillRectangle (RectF(x + channel*(w/channels), y-g-v , x+(channel+1)*(w/channels)-g, y-g), blue);
        }

      dc->FillRectangle (RectF(x, y, x+w-g, y+h), blue);
      wstring wstr (to_wstring (index++));
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, RectF(x, y, x+w-g, y+h), black);
      }
      //}}}

    index = 1;
    for (auto vidFrame : mVidFrames) {
      //{{{  draw vidFrame graphic
      // make sure we get a signed diff from unsigned pts
      int64_t diff = vidFrame->mPts - mBasePts - playPts;
      float x = (client.width/2.0f) + float(diff) * mPixPerPts;
      float w = u * vidFrameWidthPts / audFrameWidthPts;

      dc->FillRectangle (RectF(x, y+h+g, x+w-g, y+h+g+h), yellow);
      wstring wstr (to_wstring (index++));
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, RectF(x, y+h+g, x+w-g, y+h+g+h), black);

      dc->FillRectangle (RectF(x, y+h+g+h+g, x+w-g, y+h+g+h+g+h), white);
      switch (vidFrame->mPictType) {
        case 1: wstr = L"I"; break;
        case 2: wstr = L"P"; break;
        case 3: wstr = L"B"; break;
        default: wstr = to_wstring (vidFrame->mPictType); break;
        }
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, RectF(x, y+h+g+h+g, x+w-g, y+h+g+h+g+h), black);

      float l = vidFrame->mLen / 1000.0f;
      dc->FillRectangle (RectF(x, y+h+g+h+g+h+g, x+w-g, y+h+g+h+g+h+g+l), white);
      }
      //}}}
    }
  //}}}

protected:
  //{{{
  void decodeAudPes (cPidInfo* pidInfo) {

    if ((pidInfo->mPid == mSelectedAudPid) &&
        ((pidInfo->mStreamType == 17) || (pidInfo->mStreamType == 3) || (pidInfo->mStreamType == 4))) {
      if (!mAudParser) {
        //{{{  allocate decoder
        mAudParser = av_parser_init (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
        mAudCodec = avcodec_find_decoder (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
        mAudContext = avcodec_alloc_context3 (mAudCodec);
        avcodec_open2 (mAudContext, mAudCodec, NULL);
        }
        //}}}

      if (kDebugPes)
        printf ("A %4.3f %4.3f\n", pidInfo->mPts / 90000.0f, mBasePts / 90000.0f);

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
            auto audFrame = mAudFrames[mLoadAudFrame];
            audFrame->set (mInterpolatedAudPts, avFrame->channels, 48000, avFrame->nb_samples);
            mLastAudPts = mInterpolatedAudPts;
            mInterpolatedAudPts += (avFrame->nb_samples * 90000) / 48000;

            auto samplePtr = (short*)audFrame->mSamples;
            if (mAudContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
              //{{{  calc 16bit signed planar power
              short* chanPtr[2];
              for (auto channel = 0; channel < avFrame->channels; channel++)
                chanPtr[channel] = (short*)avFrame->data[channel];

              for (auto i = 0; i < avFrame->nb_samples; i++) {
                for (auto channel = 0; channel < avFrame->channels; channel++) {
                  auto sample = *chanPtr[channel]++;
                  audFrame->mPower[channel] += powf (sample, 2);
                  *samplePtr++ = sample;
                  }
                }
              }
              //}}}
            else if (mAudContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
              //{{{  calc 32bit float planar power
              float* chanPtr[6];
              for (auto channel = 0; channel < avFrame->channels; channel++)
                chanPtr[channel] = (float*)avFrame->data[channel];

              for (auto i = 0; i < avFrame->nb_samples; i++) {
                for (auto channel = 0; channel < avFrame->channels; channel++) {
                  auto sample = (short)(*chanPtr[channel]++ * 0x8000);
                  audFrame->mPower[channel] += powf (sample, 2);
                  if (avFrame->channels == 6) {
                    // 5.1 channels
                    if (channel == 2) { // use centre channel
                     *samplePtr++ = sample;
                     *samplePtr++ = sample;
                      }
                    }
                  else if (avFrame->channels == 2)
                    *samplePtr++ = sample;
                  }
                }
              }
              //}}}
            for (auto channel = 0; channel < avFrame->channels; channel++)
              audFrame->mPower[channel] = (float)sqrt (audFrame->mPower[channel]) / (avFrame->nb_samples * 2.0f);
            mLoadAudFrame = (mLoadAudFrame + 1) % mAudFrames.size();
            }
          av_frame_free (&avFrame);
          }
        }
      }
    }
  //}}}
  //{{{
  void decodeVidPes (cPidInfo* pidInfo) {

    if ((pidInfo->mPid == mSelectedVidPid) &&
        ((pidInfo->mStreamType == 2) || (pidInfo->mStreamType == 27))) {
      if (!mVidParser) {
        //{{{  allocate decoder
        mVidParser = av_parser_init (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
        mVidCodec = avcodec_find_decoder (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
        mVidContext = avcodec_alloc_context3 (mVidCodec);
        avcodec_open2 (mVidContext, mVidCodec, NULL);
        }
        //}}}

      mLastVidPts = pidInfo->mPts;

      AVPacket avPacket;
      av_init_packet (&avPacket);
      avPacket.data = pidInfo->mBuffer;
      avPacket.size = 0;

      auto pesPtr = pidInfo->mBuffer;
      auto pesLen = int (pidInfo->mBufPtr - pidInfo->mBuffer);
      while (pesLen) {
        auto lenUsed = av_parser_parse2 (
          mVidParser, mVidContext, &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
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
              if (((pidInfo->mStreamType == 27) && !pidInfo->mDts) || ((pidInfo->mStreamType == 2) && pidInfo->mDts)) // use pts
                mInterpolatedVidPts = pidInfo->mPts;
              else // interpolate pts
                mInterpolatedVidPts += 90000/25;
              mLastVidPts = mInterpolatedVidPts;
              mVidFrames[mLoadVidFrame]->set (mInterpolatedVidPts,
                                              avFrame->data, avFrame->linesize,
                                              mVidContext->width, mVidContext->height,
                                              pesLen, avFrame->pict_type);
              mLoadVidFrame = (mLoadVidFrame + 1) % mVidFrames.size();
              if (kDebugPes)
                printf ("V %4.3f %4.3f t:%d l:%d\n",
                        pidInfo->mPts/90000.0f, pidInfo->mDts/90000.0f, avFrame->pict_type, pesLen);
              }
            }
          av_frame_free (&avFrame);
          }
        }
      }
    }
  //}}}

private:
  //{{{
  void invalidateAudFrames() {

    mLastAudPts = 0;
    mInterpolatedAudPts = 0;
    mLoadAudFrame = 0;
    for (auto audFrame : mAudFrames)
      audFrame->invalidate();
    }
  //}}}
  //{{{
  void invalidateVidFrames() {

    mLastVidPts = 0;
    mInterpolatedVidPts = 0;
    mLoadVidFrame = 0;
    for (auto vidFrame : mVidFrames)
      vidFrame->invalidate();
    }
  //}}}

  int mSelectedAudPid = 0;
  int mSelectedVidPid = 0;

  uint64_t mBasePts = 0;
  uint64_t mLastAudPts = 0;
  uint64_t mLastVidPts = 0;
  uint64_t mInterpolatedAudPts = 0;
  uint64_t mInterpolatedVidPts = 0;

  AVCodec* mAudCodec = nullptr;
  AVCodecContext* mAudContext = nullptr;
  AVCodecParserContext* mAudParser = nullptr;

  AVCodec* mVidCodec = nullptr;
  AVCodecContext* mVidContext = nullptr;
  AVCodecParserContext* mVidParser = nullptr;

  int mLoadAudFrame = 0;
  int mLoadVidFrame = 0;
  vector<cAudFrame*> mAudFrames;
  vector<cYuvFrame*> mVidFrames;

  float mPixPerPts = 0.0f;
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

    case 0x23 : break; // home
    case 0x24 : break; // end
    case 0x21 : bigJump (-90000*5); break; // page up
    case 0x22 : bigJump (90000*5); break;  // page down
    case 0x26 : jump (-90000); break; // up arrow
    case 0x28 : jump (90000); break;  // down arrow
    case 0x25 : step (-90000/25); break; // left arrow
    case 0x27 : step (90000/25); break; // right arrow
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
    case 0x39 : mTs.selectService (key - '0'); mTs.invalidateFrames(); changed(); break;

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
    fracJump (x / getClientF().width);
    mDownConsumed = true;
    }

  else if (x < 80) {
    if (y > 20)
      mTs.selectService ((y / 20) - 1);
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

  mPlaying = false;
  mPlayPts -= (uint64_t)(xInc / mTs.getPixPerPts());
  changed();
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  if (makeBitmap (mTs.getVidByPts (mPlayPts), mBitmap, mBitmapPts))
    dc->DrawBitmap (mBitmap, RectF (0.0f, 0.0f, getClientF().width, getClientF().height));

  // draw title
  wstringstream str;
  str << L"service:" << mServiceSelector
      << L" " << mFileSize / 1000000.0f << L"m "
      << L" cont:" << mTs.getDiscontinuity()
      << L" p:" << mPlayPts / 90000.0f
      << L" a:" << mTs.getLastAudPts() / 90000.0f
      << L" v:" << mTs.getLastVidPts() / 90000.0f
      << L" r:" << mBytesPerPts
      << L" f:" << mFilePtr/188
      << L" " << (mBlocked ? L"B " : L"  ")
      ;
  dc->DrawText (str.str().data(), (uint32_t)str.str().size(), getTextFormat(),
                RectF (0, 0, getClientF().width, getClientF().height), getWhiteBrush());

  if (mShowDebug) // show frames debug
    mTs.drawDebug (dc, getClientF(), mSmallTextFormat,
                   getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush(), getYellowBrush(),
                   mPlayPts);
  if (mShowChannel) // show services
    mTs.drawServices (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  if (mShowTransportStream) // show pids
    mTs.drawPids (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());

  // file position yellow bar
  auto x = getClientF().width * (float)mFilePtr / (float)mFileSize;
  dc->FillRectangle (RectF(0, getClientF().height-10.0f, x, getClientF().height), getYellowBrush());

  // yellow volume bar
  auto v = getClientF().height * getVolume() * 0.8f;
  dc->FillRectangle (RectF(getClientF().width - 20.0f, 0, getClientF().width, v), getYellowBrush());
  }
//}}}

private:
  //{{{
  void fracJump (float frac) {
    mFilePtr = ((int64_t)(frac * mFileSize) / 188) * 188;
    mTs.invalidateFrames();
    changed();
    }
  //}}}
  //{{{
  void bigJump (int inc) {
    mPlayPts += inc;
    mTs.invalidateFrames();
    changed();
    }
  //}}}
  //{{{
  void jump (int inc) {
    mPlayPts += inc;
    changed();
    }
  //}}}
  //{{{
  void step (int inc) {
    mPlayPts += inc;
    mPlaying = false;
    changed();
    }
  //}}}

  //{{{
  void loader (wchar_t* wFileName) {

    av_register_all();

    auto readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    GetFileSizeEx (readFile, (PLARGE_INTEGER)(&mFileSize));

    mFilePtr = 0;
    int64_t lastFilePtr = 0;
    uint8_t tsBuf[256*188];
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
      ReadFile (readFile, tsBuf, 256 * 188, &numberOfBytesRead, NULL);
      if (numberOfBytesRead) {
        mTs.demux (tsBuf, tsBuf + numberOfBytesRead, skip);
        mFilePtr += numberOfBytesRead;
        lastFilePtr = mFilePtr;

        if (!mTs.isServiceSelected())
          mTs.selectService (0);

        if (mTs.getLastAudPts()) {
          // estimate file rate
          mBytesPerPts = (float)mFilePtr / (float)mTs.getLastAudPts();

          int64_t diff = mPlayPts - mTs.getLastAudPts();
          if (diff > 2 * 90000) {
            printf ("diff > 2s : %4.3f %4.3f\n", diff / 90000.0f, mBytesPerPts);
            mFilePtr += (int64_t (2*90000 * mBytesPerPts) / 188) * 188;
            mTs.invalidateFrames();
            }
          else if (diff < -2 * 90000) {
            printf ("diff < -2s : %4.3f %4.3f\n", diff/90000.0f, mBytesPerPts);
            mFilePtr -= (int64_t (2*90000 * mBytesPerPts) / 188) * 188;
            mTs.invalidateFrames();
            }
          else {
            while (mPlayPts + 90000 < mTs.getLastAudPts()) {
              mBlocked = true;
              Sleep (40);
              }
            mBlocked = false;
            }
          }
        }

      // fileSize can change dynamically
      GetFileSizeEx (readFile, (PLARGE_INTEGER)(&mFileSize));
      }

    CloseHandle (readFile);
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);  // for winAudio

    audOpen (48000, 16, 2);

    mPlayPts = 0;
    while (true) {
      auto audFrame = mTs.getAudByPts (mPlayPts);
      auto ptsWidth = uint64_t (90000 * (audFrame ? audFrame->mNumSamples : 1024) / 48000);
      if (audFrame && (mMouseDown || mPlaying))
        audPlay (audFrame->mSamples, audFrame->mNumSamples * 2 * 2, 1.0f);
      else
        audSilence();

      if (mPlaying)
        mPlayPts += ptsWidth;

      if (mMouseDown || mPlaying)
        changed();
      }

    CoUninitialize();
    }
  //}}}

  //{{{  vars
  int64_t mFilePtr = 0;
  int64_t mFileSize = 0;

  cDecodeTransportStream mTs;
  int mServiceSelector = 0;

  uint64_t mPlayPts = 0;
  bool mPlaying = true;

  bool mShowDebug = false;
  bool mShowChannel = false;
  bool mShowTransportStream = false;

  float mBytesPerPts = 0;
  bool mBlocked = false;

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
