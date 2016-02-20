// tv.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"
#include "../common/timer.h"

#include "../common/cTsSection.h"

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
#define maxVidFrames 128
#define maxAudFrames 256

class cMyTsSection : public cTsSection {
  cMyTsSection() {}
  ~cMyTsSection() {}

  //{{{
  void allocateVidResources (cPidInfo* pidInfo) {

   vidParser = av_parser_init (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
   vidCodec = avcodec_find_decoder (pidInfo->mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
   vidCodecContext = avcodec_alloc_context3 (vidCodec);
   avcodec_open2 (vidCodecContext, vidCodec, NULL);
    }
  //}}}
  //{{{
  void decodeVid (cPidInfo* pidInfo) {

    int64_t pts = pidInfo->mPts;

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

        avPacket.data += bytesUsed;
        avPacket.size -= bytesUsed;
        }
      }
    }
  //}}}
  //{{{
  void allocateAudResources (cPidInfo* pidInfo) {

    audParser = av_parser_init (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
    audCodec = avcodec_find_decoder (pidInfo->mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
    audCodecContext = avcodec_alloc_context3 (audCodec);
    avcodec_open2 (audCodecContext, audCodec, NULL);
    }
  //}}}
  //{{{
  void decodeAud (cPidInfo* pidInfo) {

    int64_t pts = pidInfo->mPts;

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = pidInfo->mBuf;
    avPacket.size = 0;

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
        if (got) {
          mSamplesPerAacFrame = avFrame->nb_samples;
          mAudFramesPerSec = (float)mSampleRate / mSamplesPerAacFrame;
          mAudFrames[mAudFramesLoaded % maxAudFrames].set (pts, 2, mSamplesPerAacFrame);
          //printf ("aud %d pts:%x samples:%d \n", mAudFramesLoaded, pts, mSamplesPerAacFrame);
          pts += (mSamplesPerAacFrame * 90) / 48; // really 90000/48000

          double leftValue = 0;
          double rightValue = 0;
          short* samplePtr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;
          if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
            //{{{  16bit signed planar
            short* leftPtr = (short*)avFrame->data[0];
            short* rightPtr = (short*)avFrame->data[1];
            for (int i = 0; i < mSamplesPerAacFrame; i++) {
              *samplePtr = *leftPtr++;
              leftValue += pow(*samplePtr++, 2);
              *samplePtr = *rightPtr++;
              rightValue += pow(*samplePtr++, 2);
              }
            }
            //}}}
          else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
            //{{{  32bit float planar
            float* leftPtr = (float*)avFrame->data[0];
            float* rightPtr = (float*)avFrame->data[1];
            for (int i = 0; i < mSamplesPerAacFrame; i++) {
              *samplePtr = (short)(*leftPtr++ * 0x8000);
              leftValue += pow(*samplePtr++, 2);
              *samplePtr = (short)(*rightPtr++ * 0x8000);
              rightValue += pow(*samplePtr++, 2);
              }
            }
            //}}}
          mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerLeft = (float)sqrt (leftValue) / (mSamplesPerAacFrame * 2.0f);
          mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerRight = (float)sqrt (rightValue) / (mSamplesPerAacFrame * 2.0f);
          mAudFramesLoaded++;
          }
        av_frame_free (&avFrame);

        avPacket.data += bytesUsed;
        avPacket.size -= bytesUsed;
        }
      }
    }
  //}}}

  int64_t mVidPts = 0;
  int mSamplesPerAacFrame = 0;
  float mAudFramesPerSec = 40;
  unsigned char mChannels = 2;
  unsigned long mSampleRate = 48000;

  // av stuff
  AVCodecParserContext* vidParser = nullptr;
  AVCodec* vidCodec = nullptr;
  AVCodecContext* vidCodecContext = nullptr;

  AVCodecParserContext* audParser  = nullptr;
  AVCodec* audCodec = nullptr;
  AVCodecContext* audCodecContext = nullptr;

  AVCodecContext* subCodecContext = nullptr;
  AVCodec* subCodec = nullptr;

  int mAudFramesLoaded = 0;
  cAudFrame mAudFrames [maxAudFrames];

  int mNextFreeVidFrame = 0;
  cYuvFrame mYuvFrames[maxVidFrames];
  };

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

    av_register_all();

    initialise (title, width, height);

    if (arg) {
      thread ([=]() { isTsFile (arg) ? tsFileLoader (arg) : ffmpegFileLoader (arg); } ).detach();

      // launch playerThread, higher priority
      auto playerThread = thread ([=]() { player(); });
      SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
      playerThread.detach();

      // loop in windows message pump till quit
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

    case 0x21 : mPlayTime -= 5 * 90000; changed(); break;
    case 0x22 : mPlayTime += 5 * 90000; changed(); break;

    case 0x23 : break; // home
    case 0x24 : break; // end

    case 0x25 : mPlayTime -= 90000; changed(); break;
    case 0x27 : mPlayTime += 90000; changed(); break;

    case 0x26 : mPlaying = false; mPlayTime -= 3600; changed(); break;
    case 0x28 : mPlaying = false;  mPlayTime += 3600; changed(); break;

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

    case 0x44 : debug();
    default   : printf ("key %x\n", key);
    }
  return false;
  }
//}}}
//{{{
void onMouseProx (bool inClient, int x, int y) {

  bool showChannel = mShowChannel;
  mShowChannel = inClient && (x < 80);
  if (showChannel != mShowChannel)
    changed();
  }
//}}}
//{{{
void onMouseUp (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved) {}
  changed();
  }
//}}}
//{{{
void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  mPlayTime -= xInc*3600;
  changed();
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  int vidFrame = findNearestVidFrame (mPlayTime);
  if (vidFrame >= 0)
    makeBitmap (&mYuvFrames[vidFrame % maxVidFrames]);
  if (mBitmap)
    dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
  else
    dc->Clear (ColorF(ColorF::Black));

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%4.1f %4.3fm dis:%d pre:%d",
                       (mPlayTime-mBaseTime)/90000.0, mFilePtr/1000000.0, mDiscontinuity, (int)mPreLoaded);
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(0, 0, getClientF().width, getClientF().height), getWhiteBrush());

  if (mShowChannel) {
    D2D1_RECT_F r = RectF ((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
    dc->FillRectangle (r, getGreyBrush());

    // ***** fix ********
    int audFrame = findAudFrame (mPlayTime) - (int)(getClientF().width/2);
    for (r.left = 0.0f; r.left < getClientF().width; r.left++, audFrame++) {
      if (audFrame >= 0) {
        r.top = (getClientF().height/2.0f) - mAudFrames[audFrame % maxAudFrames].mPowerLeft;
        r.bottom = (getClientF().height/2.0f) + mAudFrames[audFrame % maxAudFrames].mPowerRight;
        r.right = r.left + 1.0f;
        dc->FillRectangle (r, getBlueBrush());
        }
      }

    mTsSection.renderPidInfo  (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
    }
  }
//}}}

private:
  //{{{
  int findAudFrame (int64_t pts) {
  // find aud frame matching pts

    for (int i = 0; i < maxAudFrames; i++)
      if (mAudFrames[i].mPts)
        if (abs(mAudFrames[i].mPts - pts) <= (mSamplesPerAacFrame * 90/2) / 48)
          return i;

    return -1;
    }
  //}}}
  //{{{
  int64_t getAudPreLoaded (int64_t pts) {
  // return largest loaded audFrame pts diff ahead of pts

    int64_t ahead = 0;
    for (int audFrame = 0; audFrame < maxAudFrames; audFrame++)
      if (mAudFrames[audFrame].mPts)
        if (!audFrame || (mAudFrames[audFrame].mPts - pts > ahead))
          ahead = mAudFrames[audFrame].mPts - pts;

    return ahead;
    }
  //}}}
  //{{{
  int findNearestVidFrame (int64_t pts) {

    int vidFrame = -1;
    int64_t nearest = 0;

    for (int i = 0; i < maxVidFrames; i++) {
      if (mYuvFrames[i].mPts) {
        if ((vidFrame == -1) || (abs(mYuvFrames[i].mPts - pts - mVidOffset*3600) < nearest)) {
          vidFrame = i;
          nearest = abs(mYuvFrames[i].mPts - pts - mVidOffset * 3600);
          }
        }
      }
    return vidFrame;
    }
  //}}}

  //{{{
  void selectService (int index) {

    int j = 0;
    for (auto service : mTsSection.mServiceMap) {
      if (j == index) {
        int mPcrPid = service.second.getPcrPid();
        tPidInfoMap::iterator pidInfoIt = mTsSection.getPidInfoMap()->find (mPcrPid);
        if (pidInfoIt != mTsSection.getPidInfoMap()->end()) {
          mVidPid = service.second.getVidPid();
          mAudPid = service.second.getAudPid();

          for (int i= 0; i < maxVidFrames; i++)
            mYuvFrames[i].invalidate();
          for (int i= 0; i < maxAudFrames; i++)
            mAudFrames[i].invalidate();

          mBaseTime = pidInfoIt->second.mPcr;
          mPlayTime = mBaseTime;

          printf ("selectService %d vid:%d aud:%d pcr:%d base:%lld\n", j, mVidPid, mAudPid, mPcrPid, mBaseTime);
          }
        break;
        }
      else
        j++;
      }
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
  int64_t parseTimeStamp (uint8_t* tsPtr) {
    return (((*tsPtr) & 0x0E) << 29) | (*(tsPtr+1) << 22) | ((*(tsPtr+2) & 0xFE) << 14) | (*(tsPtr+3) << 7)  | (*(tsPtr+4) >> 1);
    }
  //}}}
  //{{{
  void parseTimeStamps (uint8_t* tsPtr, int64_t& pts, int64_t& dts) {

    pts = ((*(tsPtr+8) >= 5) && (*(tsPtr+7) & 0x80)) ? parseTimeStamp (tsPtr+9) : 0;
    dts = ((*(tsPtr+8) >= 10) && (*(tsPtr+7) & 0x40)) ? parseTimeStamp (tsPtr+14) : 0;
    }
  //}}}
  //{{{
  bool isTsFile (wchar_t* wFileName) {

    HANDLE readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    uint8_t buf[512];
    DWORD numberOfBytesRead = 0;
    ReadFile (readFile, buf, 512, &numberOfBytesRead, NULL);
    CloseHandle (readFile);

    return buf[0] == 0x47;
    }
  //}}}
  //{{{
  void debug() {
    mTsSection.printServices();
    mTsSection.printPids();
    mTsSection.printPrograms();
    }
  //}}}

  //{{{
  void tsParser (uint8_t* tsPtr, uint8_t* tsEnd) {

    if (mAudPid <= 0)
      selectService (0);

    // iterate over tsFrames, start marked by syncCode until tsPtr reaches tsEnd
    while ((tsPtr+188 <= tsEnd) && (*tsPtr++ == 0x47) && ((tsPtr+187 == tsEnd) || (*(tsPtr+187) == 0x47))) {
      //{{{  parse tsFrame start
      //bool tei = *tsPtr & 0x80;
      bool payStart = ((*tsPtr) & 0x40) != 0;
      auto pid = ((*tsPtr & 0x1F) << 8) | *(tsPtr+1);
      bool adaption = (*(tsPtr+2) & 0x20) != 0;
      //bool payload = *(tsPtr+2) & 0x10;
      auto headerBytes = adaption ? (4 + *(tsPtr+3)) : 3;
      auto continuity = *(tsPtr+2) & 0x0F;
      //if ((adaption && (*(tsPtr+4) & 0x80)) discontinuity = true;
      bool hasPcr = adaption && (*(tsPtr+4) & 0x10);
      int64_t pcr = hasPcr ? ((*(tsPtr+5)<<25) | (*(tsPtr+6)<<17)  | (*(tsPtr+7)<<9) | (*(tsPtr+8)<<1) | (*(tsPtr+9)>>7)) : 0;

      bool copyPES = false;

      tsPtr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;

      bool isSection = (pid == PID_PAT) || (pid == PID_SDT) || (pid == PID_EIT) || (pid == PID_TDT) ||
                       (mTsSection.mProgramMap.find (pid) != mTsSection.mProgramMap.end());

      tPidInfoMap::iterator pidInfoIt = mTsSection.getPidInfoMap()->find (pid);
      if (pidInfoIt == mTsSection.getPidInfoMap()->end()) {
        // new pid, insert new cPidInfo, get pidInfoIt iterator
        pair<tPidInfoMap::iterator, bool> insertPair =
          mTsSection.getPidInfoMap()->insert (tPidInfoMap::value_type (pid, cPidInfo(pid, isSection)));
        pidInfoIt = insertPair.first;
        }

      else if (continuity != ((pidInfoIt->second.mContinuity+1) & 0x0f)) {
        mDiscontinuity++;
        if (isSection) //  only report section/program discontinuity
          printf ("continuity error pid:%d - %x:%x\n", pid, continuity, pidInfoIt->second.mContinuity);

        // reset content buffers
        pidInfoIt->second.mBufBytes = 0;
        pidInfoIt->second.mBufPtr = nullptr;
        }

      if (hasPcr)
        pidInfoIt->second.mPcr = pcr;
      pidInfoIt->second.mContinuity = continuity;
      pidInfoIt->second.mTotal++;
      //}}}
      if (isSection) {
        //{{{  parse sections
        if (payStart) {
          // parse sectionStart
          int pointerField = *tsPtr;
          if (pointerField && (pidInfoIt->second.mBufBytes > 0)) {
            //{{{  packet has end of last section
            //printf ("PayStart- buffering end of last section len:%d %d\n", pointerField, (int)pidInfoIt->second.mBufBytes);
            memcpy (pidInfoIt->second.mBuf + pidInfoIt->second.mBufBytes, tsPtr+1, pointerField);
            pidInfoIt->second.mBufBytes += pointerField;
            if (pidInfoIt->second.mBufBytes >= pidInfoIt->second.mSectionLength+3) {
              //printf ("  - parsed buf len:%d\n", (int)pidInfoIt->second.mBufBytes);
              mTsSection.parseSection (pid, pidInfoIt->second.mBuf);
              }

            else if (pidInfoIt->second.mBufBytes) {
              //printf ("  - end of last section mismatch buf %d unused:%d sectionLen:%d *****************\n",
              //        pid, (int)pidInfoIt->second.mBufBytes, pidInfoIt->second.mSectionLength);
              }

            // consumed buffer
            pidInfoIt->second.mBufBytes = 0;
            }
            //}}}

          pidInfoIt->second.mSectionLength = ((*(tsPtr+pointerField+2) & 0x0F) << 8) | *(tsPtr+pointerField+3);
          //printf ("PayStart:%d pointerField:%d, sectionLen:%d\n", pid, pointerField, pidInfoIt->second.mSectionLength);

          if (pointerField > 183 - (pidInfoIt->second.mSectionLength+3)) { // 1st section straddles packets, start buffering
            if (!pidInfoIt->second.mBuf) {
              pidInfoIt->second.mBufSize = 4096;
              pidInfoIt->second.mBuf = (uint8_t*)malloc (pidInfoIt->second.mBufSize);
              }
            memcpy (pidInfoIt->second.mBuf, tsPtr+pointerField+1, 183 - pointerField);
            pidInfoIt->second.mBufBytes = 183 - pointerField;
            //printf ("  - buffered 1st section - off:%d len:%d\n", pointerField + 1, 183 - pointerField);
            }
          else { // 1st section in packet, parse it
            //printf ("- parse 1st section in packet - off:%d\n", pointerField+1);
            mTsSection.parseSection (pid, tsPtr+pointerField+1);
            pointerField += pidInfoIt->second.mSectionLength+3;

            while (*(tsPtr+pointerField+1) != 0xFF) { // more sections
              pidInfoIt->second.mSectionLength = ((*(tsPtr+pointerField+2) & 0x0F) << 8) | *(tsPtr+pointerField+3);
              //printf ("- next section in packet sectionLen:%d\n", pidInfoIt->second.mSectionLength);
              if (pointerField < 183 - (pidInfoIt->second.mSectionLength+3)) { // parse next section in packet
                //printf ("  - parse next section in packet - off:%d\n", pointerField+1);
                mTsSection.parseSection (pid, tsPtr + pointerField+1);
                pointerField += pidInfoIt->second.mSectionLength+3;
                }
              else { // next section straddles packets, start buffering
                if (!pidInfoIt->second.mBuf) {
                  pidInfoIt->second.mBufSize = 4096;
                  pidInfoIt->second.mBuf = (uint8_t*)malloc (pidInfoIt->second.mBufSize);
                  }
                memcpy (pidInfoIt->second.mBuf, tsPtr + pointerField + 1, 183 - pointerField);
                pidInfoIt->second.mBufBytes = 183 - pointerField;
                //printf ("  - buffered next section - off:%d len:%d\n", pointerField + 1, 183 - pointerField);
                break;
                }
              }
            }
          }
        else if (pidInfoIt->second.mBufBytes > 0) { // add to buffered section
          memcpy (pidInfoIt->second.mBuf + pidInfoIt->second.mBufBytes, tsPtr, tsFrameBytesLeft);
          pidInfoIt->second.mBufBytes += tsFrameBytesLeft;
          if (pidInfoIt->second.mBufBytes > pidInfoIt->second.mBufSize)
            printf ("sectionBuf overflow %d\n", (int)pidInfoIt->second.mBufBytes);
          //printf ("- buffered section continuation packet %d - buf:%d sectionLen:%d\n",
          //        pid, (int)pidInfoIt->second.mBufBytes, pidInfoIt->second.mSectionLength);

          if (pidInfoIt->second.mBufBytes >= pidInfoIt->second.mSectionLength + 3) { // enough bytes to parse a section
            //printf ("    -  parse buffered section\n");
            mTsSection.parseSection (pid, pidInfoIt->second.mBuf);
            // consumed buffer
            pidInfoIt->second.mBufBytes = 0;
            }
          }
        else
          printf ("------- buffering section continuation packet discarded, no buffer started %d\n", pid);
        }
        //}}}

      else if (pid == mVidPid) {
        if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == 0xe0)) {
          if (!pidInfoIt->second.mBuf) {
            //{{{  first vidPES, allocate resources
            pidInfoIt->second.mBufSize = 500000;
            pidInfoIt->second.mBuf = (uint8_t*)malloc (pidInfoIt->second.mBufSize);

            vidParser = av_parser_init (pidInfoIt->second.mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
            vidCodec = avcodec_find_decoder (pidInfoIt->second.mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
            vidCodecContext = avcodec_alloc_context3 (vidCodec);

            avcodec_open2 (vidCodecContext, vidCodec, NULL);
            }
            //}}}
          else if (pidInfoIt->second.mBufPtr) {
            //{{{  decode prev vidPES
            int64_t pts = pidInfoIt->second.mPts;

            AVPacket avPacket;
            av_init_packet (&avPacket);
            avPacket.data = pidInfoIt->second.mBuf;
            avPacket.size = 0;

            int pesLen = int (pidInfoIt->second.mBufPtr - pidInfoIt->second.mBuf);
            pidInfoIt->second.mBufPtr = pidInfoIt->second.mBuf;
            while (pesLen) {
              int lenUsed = av_parser_parse2 (vidParser, vidCodecContext, &avPacket.data, &avPacket.size, pidInfoIt->second.mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
              pidInfoIt->second.mBufPtr += lenUsed;
              pesLen -= lenUsed;

              if (avPacket.data) {
                AVFrame* avFrame = av_frame_alloc();
                int got = 0;
                int bytesUsed = avcodec_decode_video2 (vidCodecContext, avFrame, &got, &avPacket);
                if (got) {
                  //printf ("vid pts:%x dts:%x\n", pidInfoIt->second.mPts, pidInfoIt->second.mDts);
                  if (((pidInfoIt->second.mStreamType == 27) && !pidInfoIt->second.mDts) ||
                      (((pidInfoIt->second.mStreamType == 2) && pidInfoIt->second.mDts))) // use pts
                    mVidPts = pidInfoIt->second.mPts;
                  else // fake pts
                    mVidPts += 3600;
                  mYuvFrames[mNextFreeVidFrame].set (
                    mVidPts, avFrame->data, avFrame->linesize, vidCodecContext->width, vidCodecContext->height);
                  mNextFreeVidFrame = (mNextFreeVidFrame + 1) % maxVidFrames;
                  }
                av_frame_free (&avFrame);

                avPacket.data += bytesUsed;
                avPacket.size -= bytesUsed;
                }
              }
            }
            //}}}
          //{{{  start next vidPES
          pidInfoIt->second.mBufPtr = pidInfoIt->second.mBuf;
          parseTimeStamps (tsPtr, pidInfoIt->second.mPts, pidInfoIt->second.mDts);

          int pesHeaderBytes = 9 + *(tsPtr+8);
          tsPtr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          //}}}
          }
        copyPES = true;
        }

      else if (pid == mAudPid) {
        if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == 0xc0)) {
          if (!pidInfoIt->second.mBuf) {
            //{{{  first audPES, allocate resources
            pidInfoIt->second.mBufSize = 5000;
            pidInfoIt->second.mBuf = (uint8_t*)malloc (pidInfoIt->second.mBufSize);

            audParser = av_parser_init (pidInfoIt->second.mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
            audCodec = avcodec_find_decoder (pidInfoIt->second.mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
            audCodecContext = avcodec_alloc_context3 (audCodec);

            avcodec_open2 (audCodecContext, audCodec, NULL);
            }
            //}}}
          else if (pidInfoIt->second.mBufPtr) {
            //{{{  decode prev audPES
            int64_t pts = pidInfoIt->second.mPts;

            AVPacket avPacket;
            av_init_packet (&avPacket);
            avPacket.data = pidInfoIt->second.mBuf;
            avPacket.size = 0;

            int pesLen = int (pidInfoIt->second.mBufPtr - pidInfoIt->second.mBuf);
            pidInfoIt->second.mBufPtr = pidInfoIt->second.mBuf;
            while (pesLen) {
              int lenUsed = av_parser_parse2 (audParser, audCodecContext, &avPacket.data, &avPacket.size, pidInfoIt->second.mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
              pidInfoIt->second.mBufPtr += lenUsed;
              pesLen -= lenUsed;

              if (avPacket.data) {
                int got = 0;
                AVFrame* avFrame = av_frame_alloc();
                int bytesUsed = avcodec_decode_audio4 (audCodecContext, avFrame, &got, &avPacket);
                if (got) {
                  mSamplesPerAacFrame = avFrame->nb_samples;
                  mAudFramesPerSec = (float)mSampleRate / mSamplesPerAacFrame;
                  mAudFrames[mAudFramesLoaded % maxAudFrames].set (pts, 2, mSamplesPerAacFrame);
                  //printf ("aud %d pts:%x samples:%d \n", mAudFramesLoaded, pts, mSamplesPerAacFrame);
                  pts += (mSamplesPerAacFrame * 90) / 48; // really 90000/48000

                  double leftValue = 0;
                  double rightValue = 0;
                  short* samplePtr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;
                  if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
                    //{{{  16bit signed planar
                    short* leftPtr = (short*)avFrame->data[0];
                    short* rightPtr = (short*)avFrame->data[1];
                    for (int i = 0; i < mSamplesPerAacFrame; i++) {
                      *samplePtr = *leftPtr++;
                      leftValue += pow(*samplePtr++, 2);
                      *samplePtr = *rightPtr++;
                      rightValue += pow(*samplePtr++, 2);
                      }
                    }
                    //}}}
                  else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
                    //{{{  32bit float planar
                    float* leftPtr = (float*)avFrame->data[0];
                    float* rightPtr = (float*)avFrame->data[1];
                    for (int i = 0; i < mSamplesPerAacFrame; i++) {
                      *samplePtr = (short)(*leftPtr++ * 0x8000);
                      leftValue += pow(*samplePtr++, 2);
                      *samplePtr = (short)(*rightPtr++ * 0x8000);
                      rightValue += pow(*samplePtr++, 2);
                      }
                    }
                    //}}}
                  mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerLeft = (float)sqrt (leftValue) / (mSamplesPerAacFrame * 2.0f);
                  mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerRight = (float)sqrt (rightValue) / (mSamplesPerAacFrame * 2.0f);
                  mAudFramesLoaded++;
                  }
                av_frame_free (&avFrame);

                avPacket.data += bytesUsed;
                avPacket.size -= bytesUsed;
                }
              }
            }
            //}}}
          //{{{  start next audPES
          pidInfoIt->second.mBufPtr = pidInfoIt->second.mBuf;
          parseTimeStamps (tsPtr, pidInfoIt->second.mPts, pidInfoIt->second.mDts);

          int pesHeaderBytes = 9 + *(tsPtr+8);
          tsPtr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          //}}}
          }
        copyPES = true;
        }

      if (copyPES && pidInfoIt->second.mBufPtr) {
        //{{{  copy rest of tsFrame to pesBuf
        memcpy (pidInfoIt->second.mBufPtr, tsPtr, tsFrameBytesLeft);
        pidInfoIt->second.mBufPtr += tsFrameBytesLeft;

        if (pidInfoIt->second.mBufPtr > pidInfoIt->second.mBuf + pidInfoIt->second.mBufSize)
          printf ("************* PES overflow - %d %d\n",
                 int(pidInfoIt->second.mBufPtr - pidInfoIt->second.mBuf), pidInfoIt->second.mBufSize);
        }
        //}}}
      tsPtr += tsFrameBytesLeft;
      }
    }
  //}}}

  //{{{
  void tsFileLoader (wchar_t* wFileName) {

    CoInitialize (NULL);
    av_register_all();

    mFilePtr = 0;
    uint8_t tsBuf[256*188];
    HANDLE readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD numberOfBytesRead = 0;
    while (ReadFile (readFile, tsBuf, 256*188, &numberOfBytesRead, NULL)) {
      if (numberOfBytesRead) {
        mFilePtr += numberOfBytesRead;
        tsParser (tsBuf, tsBuf + numberOfBytesRead);

        mPreLoaded = getAudPreLoaded (mPlayTime);
        if (mBaseTime && (mPreLoaded > 90000))
          Sleep (20);
        }
      }

    CloseHandle (readFile);

    avcodec_close (audCodecContext);
    avcodec_close (vidCodecContext);
    CoUninitialize();
    }
  //}}}
  //{{{
  void ffmpegFileLoader (wchar_t* wFileName) {

    CoInitialize (NULL);
    av_register_all();
    //avformat_network_init();

    char filename[100];
    wcstombs (filename, wFileName, 100);
    //{{{  av init
    AVFormatContext* avFormatContext;
    avformat_open_input (&avFormatContext, filename, NULL, NULL);
    avformat_find_stream_info (avFormatContext, NULL);

    int vidStream = -1;
    int audStream = -1;
    int subStream = -1;
    for (unsigned int i = 0; i < avFormatContext->nb_streams; i++)
      if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (vidStream == -1))
        vidStream = i;
      else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) && (audStream == -1))
        audStream = i;
      else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) && (subStream == -1))
        subStream = i;

    if (audStream >= 0) {
      mChannels = avFormatContext->streams[audStream]->codec->channels;
      mSampleRate = avFormatContext->streams[audStream]->codec->sample_rate;
      }

    printf ("filename:%s sampleRate:%d channels:%d streams:%d aud:%d vid:%d sub:%d\n",
            filename, mSampleRate, mChannels, avFormatContext->nb_streams, audStream, vidStream, subStream);
    //av_dump_format (avFormatContext, 0, filename, 0);

    if (mChannels > 2)
      mChannels = 2;
    //}}}
    //{{{  aud init
    if (audStream >= 0) {
      audCodecContext = avFormatContext->streams[audStream]->codec;
      audCodec = avcodec_find_decoder (audCodecContext->codec_id);
      avcodec_open2 (audCodecContext, audCodec, NULL);
      }
    //}}}
    //{{{  vid init
    if (vidStream >= 0) {
      vidCodecContext = avFormatContext->streams[vidStream]->codec;
      vidCodec = avcodec_find_decoder (vidCodecContext->codec_id);
      avcodec_open2 (vidCodecContext, vidCodec, NULL);
      }
    //}}}
    //{{{  sub init
    if (subStream >= 0) {
      subCodecContext = avFormatContext->streams[subStream]->codec;
      subCodec = avcodec_find_decoder (subCodecContext->codec_id);
      avcodec_open2 (subCodecContext, subCodec, NULL);
      }
    //}}}

    AVFrame* audFrame = av_frame_alloc();
    AVFrame* vidFrame = av_frame_alloc();

    AVSubtitle sub;
    memset (&sub, 0, sizeof(AVSubtitle));

    AVPacket avPacket;
    while (true) {
      while (av_read_frame (avFormatContext, &avPacket) >= 0) {

        //while (mAudFramesLoaded > int(mPlayTime) + maxAudFrames/2)
        //  Sleep (40);

        if (avPacket.stream_index == audStream) {
          //{{{  aud packet
          int got = 0;
          avcodec_decode_audio4 (audCodecContext, audFrame, &got, &avPacket);

          if (got) {
            mSamplesPerAacFrame = audFrame->nb_samples;
            mAudFramesPerSec = (float)mSampleRate / mSamplesPerAacFrame;
            mAudFrames[mAudFramesLoaded % maxAudFrames].set (0, 2, mSamplesPerAacFrame);

            double valueL = 0;
            double valueR = 0;
            short* ptr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;
            if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
              //{{{  16bit signed planar
              short* lptr = (short*)audFrame->data[0];
              short* rptr = (short*)audFrame->data[1];
              for (int i = 0; i < mSamplesPerAacFrame; i++) {
                *ptr = *lptr++;
                valueL += pow(*ptr++, 2);
                *ptr = *rptr++;
                valueR += pow(*ptr++, 2);
                }
              }
              //}}}
            else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
              //{{{  32bit float planar
              float* lptr = (float*)audFrame->data[0];
              float* rptr = (float*)audFrame->data[1];
              for (int i = 0; i < mSamplesPerAacFrame; i++) {
                *ptr = (short)(*lptr++ * 0x8000);
                valueL += pow(*ptr++, 2);
                *ptr = (short)(*rptr++ * 0x8000);
                valueR += pow(*ptr++, 2);
                }
              }
              //}}}
            mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerLeft = (float)sqrt (valueL) / (mSamplesPerAacFrame * 2.0f);
            mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerRight = (float)sqrt (valueR) / (mSamplesPerAacFrame * 2.0f);

            mAudFramesLoaded++;
            }
          }
          //}}}
        else if (avPacket.stream_index == vidStream) {
          //{{{  vid packet
          int got = 0;
          avcodec_decode_video2 (vidCodecContext, vidFrame, &got, &avPacket);

          if (got) {
            mYuvFrames[mNextFreeVidFrame].set (0, vidFrame->data, vidFrame->linesize, vidCodecContext->width, vidCodecContext->height);
            mNextFreeVidFrame = (mNextFreeVidFrame + 1) % maxVidFrames;
            }
          }
          //}}}
        else if (avPacket.stream_index == subStream) {
          //{{{  sub packet
          int got = 0;
          avsubtitle_free (&sub);
          avcodec_decode_subtitle2 (subCodecContext, &sub, &got, &avPacket);
          }
          //}}}
        av_free_packet (&avPacket);
        }
      }

    av_free (vidFrame);
    av_free (audFrame);

    avcodec_close (vidCodecContext);
    avcodec_close (audCodecContext);

    avformat_close_input (&avFormatContext);
    CoUninitialize();
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);

    // wait for first aud frame to load for mSampleRate, mChannels
    while (mAudFramesLoaded < 1)
      Sleep (40);

    winAudioOpen (mSampleRate, 16, mChannels);

    while (true) {
      int audFrame = findAudFrame (mPlayTime);
      winAudioPlay ((mPlaying && (audFrame >= 0)) ? mAudFrames[audFrame].mSamples : mSilence, mChannels*mSamplesPerAacFrame*2, 1.0f);
      if (mPlaying) {
        mPlayTime += (mSamplesPerAacFrame * 90) / 48;
        changed();
        }
      }

    winAudioClose();

    CoUninitialize();
    }
  //}}}

  //{{{  vars
  bool mShowChannel = false;

  // tsSection
  cTsSection mTsSection;
  int mDiscontinuity = 0;

  int mFilePtr = 0;

  // service
  int mVidPid = 0;
  int mAudPid = 0;

  int mVidOffset = 0;
  int64_t mVidPts = 0;

  // av stuff
  AVCodecParserContext* vidParser = nullptr;
  AVCodec* vidCodec = nullptr;
  AVCodecContext* vidCodecContext = nullptr;

  AVCodecParserContext* audParser  = nullptr;
  AVCodec* audCodec = nullptr;
  AVCodecContext* audCodecContext = nullptr;

  AVCodecContext* subCodecContext = nullptr;
  AVCodec* subCodec = nullptr;

  int16_t* mSilence;

  int mSamplesPerAacFrame = 0;
  float mAudFramesPerSec = 40;
  unsigned char mChannels = 2;
  unsigned long mSampleRate = 48000;

  bool mPlaying = true;
  int64_t mPlayTime = 0;
  int64_t mBaseTime = 0;
  int64_t mPreLoaded = 0;

  int mAudFramesLoaded = 0;
  cAudFrame mAudFrames [maxAudFrames];

  int mNextFreeVidFrame = 0;
  cYuvFrame mYuvFrames[maxVidFrames];

  int64_t mBitmapPts = 0;
  ID2D1Bitmap* mBitmap = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  #ifndef _DEBUG
    //FreeConsole();
  #endif

  cTvWindow tvWindow;
  tvWindow.run (L"tvWindow", 896, 504, argv[1]);

  CoUninitialize();
  }
//}}}
