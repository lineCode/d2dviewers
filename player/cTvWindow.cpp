// tv.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "cTsSection.h"
#include "bda.h"

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

class cTvWindow : public cD2dWindow {
public:
  cTvWindow() {}
  virtual ~cTvWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFilename) {

    if (wFilename) {
      mWideFilename = wFilename;
      wcstombs (mFilename, mWideFilename, 100);
      }
    initialise (title, width, height);

    // av init
    av_register_all();
    avformat_network_init();

    if (wFilename)
      thread ([=]() { tsFileLoader(); } ).detach();
      //thread ([=]() { fileLoader(); } ).detach();
    else {
      // 650000 674000 706000
      mBda = createBDAGraph (674000);
      if (mBda)
        thread ([=]() { tsLiveLoader(); } ).detach();
      }

    // launch playerThread, higher priority
    auto playerThread = thread ([=]() { player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

    // loop in windows message pump till quit
    messagePump();

    mTsSection.printServices();
    mTsSection.printPids();
    mTsSection.printPrograms();
    }
  //}}}

protected:
//{{{
bool onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    case 0x20 : mPlaying = !mPlaying; break;

    case 0x21 : mPlayFrame -= 5.0f * mAudFramesPerSec; changed(); break;
    case 0x22 : mPlayFrame += 5.0f * mAudFramesPerSec; changed(); break;

    case 0x23 : mPlayFrame = mAudFramesLoaded - 1.0f; changed(); break;
    case 0x24 : mPlayFrame = 0; break;

    case 0x25 : mPlayFrame -= 1.0f * mAudFramesPerSec; changed(); break;
    case 0x27 : mPlayFrame += 1.0f * mAudFramesPerSec; changed(); break;

    //case 0x26 : mPlaying = false; mPlayFrame -=2; changed(); break;
    //case 0x28 : mPlaying = false; mPlayFrame +=2; changed(); break;
    case 0x26 : mVidOffset -=1; break;
    case 0x28 : mVidOffset +=1; break;

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
    case 0x39 : mService = key - '0'; break;

    case 0x46 : saveBDA();
    default   : printf ("key %x\n", key);
    }

  if (mPlayFrame < 0)
    mPlayFrame = 0;
  else if (mPlayFrame >= mAudFramesLoaded)
    mPlayFrame = mAudFramesLoaded - 1.0f;

  return false;
  }
//}}}
//{{{
void onMouseUp (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved)
    mPlayFrame += x - (getClientF().width/2.0f);

  if (mPlayFrame < 0)
    mPlayFrame = 0;
  else if (mPlayFrame >= mAudFramesLoaded)
    mPlayFrame = mAudFramesLoaded - 1.0f;

  changed();
  }
//}}}
//{{{
void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  mPlayFrame -= xInc;

  if (mPlayFrame < 0)
    mPlayFrame = 0;
  else if (mPlayFrame >= mAudFramesLoaded)
    mPlayFrame = mAudFramesLoaded - 1.0f;

  changed();
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  int vidFrame = int(mPlayFrame*25/mAudFramesPerSec) - mVidOffset;
  if (vidFrame >= 0)
    makeBitmap (&mYuvFrames[vidFrame % maxVidFrames]);
  if (mBitmap)
    dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
  else
    dc->Clear (ColorF(ColorF::Black));

  D2D1_RECT_F r = RectF ((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
  dc->FillRectangle (r, getGreyBrush());

  int audFrame = int(mPlayFrame) - (int)(getClientF().width/2);
  for (r.left = 0.0f; (r.left < getClientF().width) && (audFrame < mAudFramesLoaded); r.left++, audFrame++) {
    if (audFrame >= 0) {
      r.top = (getClientF().height/2.0f) - mAudFrames[audFrame % maxAudFrames].mPowerLeft;
      r.bottom = (getClientF().height/2.0f) + mAudFrames[audFrame % maxAudFrames].mPowerRight;
      r.right = r.left + 1.0f;
      dc->FillRectangle (r, getBlueBrush());
      }
    }

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%4.1f %2.0f:%d %d:%d d:%d %d a:%lld v:%lld %lld",
            mPlayFrame/mAudFramesPerSec, mPlayFrame, mAudFramesLoaded, vidFrame, mVidFramesLoaded,
            mDiscontinuity, mVidOffset,
            mAudFrames[int(mPlayFrame) % maxAudFrames].mPts, mYuvFrames[vidFrame % maxVidFrames].mPts,
            mAudFrames[int(mPlayFrame) % maxAudFrames].mPts - mYuvFrames[vidFrame % maxVidFrames].mPts);
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(0, 2.0f, getClientF().width, getClientF().height), getBlackBrush());
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(0, 0, getClientF().width, getClientF().height), getWhiteBrush());

  mTsSection.renderPidInfo  (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  }
//}}}

private:
  //{{{
  void makeBitmap (cYuvFrame* yuvFrame) {

    static const D2D1_BITMAP_PROPERTIES props = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 96.0f, 96.0f };

    if (yuvFrame) {
      if (yuvFrame->mId != mVidId) {
        mVidId = yuvFrame->mId;
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
  int64_t parseTimeStamp (bool isPts, uint8_t* tsPtr) {

    tsPtr += 9;
    if (isPts) {
      if (((*tsPtr) & 0x20) == 0)
        return 0;
      }
    else { // isDts
      if (((*tsPtr) & 0x10) == 0)
        return 0;
      tsPtr += 5;
      }

    return (((*tsPtr) & 0x0E)<<30) | (*(tsPtr+1)<<22) | ((*(tsPtr+2) & 0xFE)<<14) | (*(tsPtr+2)<<7) | (*(tsPtr+3)>>1);
    }
  //}}}
  //{{{
  void parseTimeStamps (uint8_t* tsPtr, int64_t& pts, int64_t& dts) {

    pts = ((*(tsPtr+8) >= 5) && (*(tsPtr+7) & 0x80)) ? parseTimeStamp (true, tsPtr) : 0;
    dts = ((*(tsPtr+8) >= 10) && (*(tsPtr+7) & 0x40)) ? parseTimeStamp (false, tsPtr) : 0;

    //printf ("%x len:%04d 6:%02x 7:%02x 8:%02x pts:%02x:%02x:%02x:%02x:%02x dts:%02x:%02x:%02x:%02x:%02x - pts:%x dts:%x\n",
    //        ((*tsPtr) << 24) | ((*(tsPtr+1)) << 16) | ((*(tsPtr+2)) << 8) | (*(tsPtr+3)),
    //        (*(tsPtr+4) << 8) | *(tsPtr+5),
    //        *(tsPtr+6), *(tsPtr+7), *(tsPtr+8),
    //        *(tsPtr+9), *(tsPtr+10), *(tsPtr+11), *(tsPtr+12), *(tsPtr+13),
    //        *(tsPtr+14), *(tsPtr+15), *(tsPtr+16), *(tsPtr+17), *(tsPtr+18),
    //        pts, dts);

    //printf ("%x pts:%x dts:%x\n", ((*tsPtr) << 24) | ((*(tsPtr+1)) << 16) | ((*(tsPtr+2)) << 8) | (*(tsPtr+3)), pts, dts);
    }
  //}}}
  //{{{
  void saveBDA() {

    FILE* file = fopen ("C:\\Users\\colin\\Desktop\\bdaDump.ts", "wb");
    fwrite (mBda, 1, getBDAend() - mBda, file);
    fclose (file);
    }
  //}}}

  //{{{
  void tsReader (uint8_t* tsBuf, int len) {

    size_t i = 0;
    uint8_t* tsPtr = tsBuf;
    while (tsPtr < tsBuf + len) {
      //{{{  choose service
      int j = 0;
      for (auto service : mTsSection.mServiceMap) {
        if (j == mService) {
          mServicePtr = &service.second;
          break;
          }
        else
          j++;
        }
      //}}}
      while ((tsPtr < tsBuf + len) && (*tsPtr++ == 0x47) && ((tsPtr+187 >= tsBuf + len) || (*(tsPtr+187) == 0x47))) {
        //{{{  parse packetStart
        auto payStart = *tsPtr & 0x40;
        auto pid = ((*tsPtr & 0x1F) << 8) | *(tsPtr+1);
        auto headerBytes = (*(tsPtr+2) & 0x20) ? (4 + *(tsPtr+3)) : 3;
        auto continuity = *(tsPtr+2) & 0x0F;

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
          // count all discontinuity
          mDiscontinuity++;
          if (isSection) // but only report section/program discontinuity
            printf ("continuity error pid:%d - %x:%x\n", pid,  continuity, pidInfoIt->second.mContinuity);

          // reset content buffers
          pidInfoIt->second.mBufBytes = 0;
          pidInfoIt->second.mPesPtr = nullptr;
          }

        pidInfoIt->second.mContinuity = continuity;
        pidInfoIt->second.mTotal++;
        //}}}
        if (isSection) {
          if (payStart) {
            //{{{  parse sectionStart
            int pointerField = tsBuf[i+4];
            if ((pointerField > 0) && (pidInfoIt->second.mBufBytes > 0)) {
              // payloadStart has end of lastSection
              memcpy (&pidInfoIt->second.mBuf[pidInfoIt->second.mBufBytes], &tsBuf[i+5], pointerField);

              pidInfoIt->second.mBufBytes += pointerField;
              if (pidInfoIt->second.mLength + 3 <= pidInfoIt->second.mBufBytes) {
                mTsSection.parseEit (pidInfoIt->second.mBuf,0);
                pidInfoIt->second.mLength = 0;
                pidInfoIt->second.mBufBytes = 0;
                }

              if (pidInfoIt->second.mBufBytes > 0) {
                #ifdef TSERROR
                printf ("parsePackets pid:%d unused buf:%d sectLen:%d\n",
                        pid, (int)pidInfoIt->second.mBufBytes, pidInfoIt->second.mLength);
                #endif
                }
              }

            size_t j = i + pointerField + 5;
            pidInfoIt->second.mLength = ((tsBuf[j+1] & 0x0f) << 8) | tsBuf[j+2];
            if (pidInfoIt->second.mLength + 3 <= TS_SIZE - 5 - pointerField) {
              // first section
              mTsSection.parseSection (pid, tsBuf+j, tsBuf+i+188);
              j += pidInfoIt->second.mLength + 3;
              pidInfoIt->second.mBufBytes = 0;

              while (tsBuf[j] != 0xFF) {
                // parse more sections
                pidInfoIt->second.mLength = ((tsBuf[j+1] & 0x0f) << 8) | tsBuf[j+2];
                if (j + pidInfoIt->second.mLength + 4 - i < TS_SIZE) {
                  mTsSection.parseSection (pid, tsBuf+j, tsBuf+i+188);
                  j += pidInfoIt->second.mLength + 3;
                  pidInfoIt->second.mBufBytes = 0;
                  }
                else {
                  memcpy (pidInfoIt->second.mBuf, &tsBuf[j], TS_SIZE - (j - i));
                  pidInfoIt->second.mBufBytes = TS_SIZE - (j - i);
                  break;
                  }
                }
              }

            else if (pointerField < TS_SIZE - 5) {
              memcpy (pidInfoIt->second.mBuf, &tsBuf[j], TS_SIZE - 5 - pointerField);
              pidInfoIt->second.mBufBytes = TS_SIZE - 5 - pointerField;
              }

            else
              printf ("parsePackets pid:%d pointerField overflow\n", pid);
            }
            //}}}
          else if (pidInfoIt->second.mBufBytes > 0) {
            //{{{  parse sectionContinuation
            memcpy (&pidInfoIt->second.mBuf[pidInfoIt->second.mBufBytes], &tsBuf[i+4], TS_SIZE - 4);
            pidInfoIt->second.mBufBytes += TS_SIZE - 4;

            if (pidInfoIt->second.mLength + 3 <= pidInfoIt->second.mBufBytes) {
              mTsSection.parseSection (pid, pidInfoIt->second.mBuf, 0);
              pidInfoIt->second.mBufBytes = 0;
              }
            }
            //}}}
          }

        else if (mServicePtr && (pid == mServicePtr->getVidPid())) {
          if (!pidInfoIt->second.mPesBuf) {
            //{{{  allocate vid resources
            pidInfoIt->second.mPesBufSize = 500000;
            pidInfoIt->second.mPesBuf = (uint8_t*)malloc (pidInfoIt->second.mPesBufSize);

            vidParser = av_parser_init (pidInfoIt->second.mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
            vidCodec = avcodec_find_decoder (pidInfoIt->second.mStreamType == 27 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
            vidCodecContext = avcodec_alloc_context3 (vidCodec);

            avcodec_open2 (vidCodecContext, vidCodec, NULL);
            }
            //}}}
          if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == 0xe0)) {
            if (pidInfoIt->second.mPesPtr) {
              //{{{  decode last vidPES
              AVPacket avPacket;
              av_init_packet (&avPacket);
              avPacket.data = pidInfoIt->second.mPesBuf;
              avPacket.size = 0;

              int pesLen = int (pidInfoIt->second.mPesPtr - pidInfoIt->second.mPesBuf);
              pidInfoIt->second.mPesPtr = pidInfoIt->second.mPesBuf;
              while (pesLen) {
                int lenUsed = av_parser_parse2 (vidParser, vidCodecContext, &avPacket.data, &avPacket.size, pidInfoIt->second.mPesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
                pidInfoIt->second.mPesPtr += lenUsed;
                pesLen -= lenUsed;

                if (avPacket.data) {
                  AVFrame* avFrame = av_frame_alloc();
                  int got = 0;
                  int bytesUsed = avcodec_decode_video2 (vidCodecContext, avFrame, &got, &avPacket);
                  if (got) {
                    //printf ("vid %d %d %d\n", mVidFramesLoaded, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
                    mYuvFrames[mVidFramesLoaded % maxVidFrames].set (pidInfoIt->second.mPts,
                      avFrame->data, avFrame->linesize, vidCodecContext->width, vidCodecContext->height);
                    mVidFramesLoaded++;
                    }
                  av_frame_free (&avFrame);

                  avPacket.data += bytesUsed;
                  avPacket.size -= bytesUsed;
                  }
                }
              }
              //}}}
            //{{{  start new vidPES
            pidInfoIt->second.mPesPtr = pidInfoIt->second.mPesBuf;
            parseTimeStamps (tsPtr, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
            int pesHeaderBytes = 9 + *(tsPtr+8);
            tsPtr += pesHeaderBytes;
            tsFrameBytesLeft -= pesHeaderBytes;
            //}}}
            }
          if (pidInfoIt->second.mPesPtr) {
            //{{{  copy rest of ts to pesBuf
            memcpy (pidInfoIt->second.mPesPtr, tsPtr, tsFrameBytesLeft);
            pidInfoIt->second.mPesPtr += tsFrameBytesLeft;

            if (pidInfoIt->second.mPesPtr > pidInfoIt->second.mPesBuf + pidInfoIt->second.mPesBufSize)
              printf ("audPES overflow *********************************************\n");
            }
            //}}}
          }

        else if (mServicePtr && (pid == mServicePtr->getAudPid())) {
          if (!pidInfoIt->second.mPesBuf) {
            //{{{  allocate aud resources
            pidInfoIt->second.mPesBufSize = 5000;
            pidInfoIt->second.mPesBuf = (uint8_t*)malloc (pidInfoIt->second.mPesBufSize);

            audParser = av_parser_init (pidInfoIt->second.mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
            audCodec = avcodec_find_decoder (pidInfoIt->second.mStreamType == 17 ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_MP3);
            audCodecContext = avcodec_alloc_context3 (audCodec);

            avcodec_open2 (audCodecContext, audCodec, NULL);
            }
            //}}}
          if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == 0xc0)) {
            if (pidInfoIt->second.mPesPtr) {
              //{{{  decode last audPES
              AVPacket avPacket;
              av_init_packet (&avPacket);
              avPacket.data = pidInfoIt->second.mPesBuf;
              avPacket.size = 0;

              int pesLen = int (pidInfoIt->second.mPesPtr - pidInfoIt->second.mPesBuf);
              pidInfoIt->second.mPesPtr = pidInfoIt->second.mPesBuf;
              while (pesLen) {
                int lenUsed = av_parser_parse2 (audParser, audCodecContext, &avPacket.data, &avPacket.size, pidInfoIt->second.mPesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
                pidInfoIt->second.mPesPtr += lenUsed;
                pesLen -= lenUsed;

                if (avPacket.data) {
                  int got = 0;
                  AVFrame* avFrame = av_frame_alloc();
                  int bytesUsed = avcodec_decode_audio4 (audCodecContext, avFrame, &got, &avPacket);
                  if (got) {
                    mSamplesPerAcFrame = avFrame->nb_samples;
                    mAudFramesPerSec = (float)sampleRate / mSamplesPerAcFrame;
                    mAudFrames[mAudFramesLoaded % maxAudFrames].set (pidInfoIt->second.mPts, 2, mSamplesPerAcFrame);
                    //printf ("aud %d %d %d %d\n", mAudFramesLoaded, pidInfoIt->second.mPts, pidInfoIt->second.mDts, mSamplesPerAcFrame);

                    double leftValue = 0;
                    double rightValue = 0;
                    short* samplePtr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;
                    if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
                      //{{{  16bit signed planar
                      short* leftPtr = (short*)avFrame->data[0];
                      short* rightPtr = (short*)avFrame->data[1];
                      for (int i = 0; i < mSamplesPerAcFrame; i++) {
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
                      for (int i = 0; i < mSamplesPerAcFrame; i++) {
                        *samplePtr = (short)(*leftPtr++ * 0x8000);
                        leftValue += pow(*samplePtr++, 2);
                        *samplePtr = (short)(*rightPtr++ * 0x8000);
                        rightValue += pow(*samplePtr++, 2);
                        }
                      }
                      //}}}
                    mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerLeft = (float)sqrt (leftValue) / (mSamplesPerAcFrame * 2.0f);
                    mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerRight = (float)sqrt (rightValue) / (mSamplesPerAcFrame * 2.0f);
                    mAudFramesLoaded++;
                    }
                  av_frame_free (&avFrame);

                  avPacket.data += bytesUsed;
                  avPacket.size -= bytesUsed;
                  }
                }
              }
              //}}}
            //{{{  start new audPES
            pidInfoIt->second.mPesPtr = pidInfoIt->second.mPesBuf;
            parseTimeStamps (tsPtr, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
            int pesHeaderBytes = 9 + *(tsPtr+8);
            tsPtr += pesHeaderBytes;
            tsFrameBytesLeft -= pesHeaderBytes;
            //}}}
            }
          if (pidInfoIt->second.mPesPtr) {
            //{{{  copy rest of ts to pesBuf
            memcpy (pidInfoIt->second.mPesPtr, tsPtr, tsFrameBytesLeft);
            pidInfoIt->second.mPesPtr += tsFrameBytesLeft;

            if (pidInfoIt->second.mPesPtr > pidInfoIt->second.mPesBuf + pidInfoIt->second.mPesBufSize)
              printf ("vidPES overflow *********************************\n");
            }
            //}}}
          }

        tsPtr += tsFrameBytesLeft;
        i += 188;

        while (mAudFramesLoaded > int(mPlayFrame) + maxAudFrames/2)
          Sleep (40);
        }
      }
    }
  //}}}

  //{{{
  void tsFileLoader() {

    uint8_t tsBuf[240*188];

    HANDLE readFile = CreateFile (mWideFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    while (true) {
      DWORD sampleLen = 0;
      ReadFile (readFile, tsBuf, 240*188, &sampleLen, NULL);
      if (!sampleLen)
        break;
      tsReader (tsBuf, sampleLen);
      }

    CloseHandle (readFile);

    avcodec_close (audCodecContext);
    avcodec_close (vidCodecContext);
    }
  //}}}
  //{{{
  void tsLiveLoader() {

    uint8_t* bdaPtr = mBda;

    while (true) {
      while (getBDAend() > bdaPtr + 240*188)
        Sleep (1);

      printf ("live read\n");

      uint8_t* bdaEndPtr = getBDAend();
      tsReader (bdaPtr, int(bdaEndPtr - bdaPtr));
      bdaPtr = bdaEndPtr;
      }

    avcodec_close (audCodecContext);
    avcodec_close (vidCodecContext);
    }
  //}}}
  //{{{
  void ffmpegFileLoader() {

    //{{{  av init
    AVFormatContext* avFormatContext;
    avformat_open_input (&avFormatContext, mFilename, NULL, NULL);
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
      channels = avFormatContext->streams[audStream]->codec->channels;
      sampleRate = avFormatContext->streams[audStream]->codec->sample_rate;
      }

    printf ("filename:%s sampleRate:%d channels:%d streams:%d aud:%d vid:%d sub:%d\n",
            mFilename, sampleRate, channels, avFormatContext->nb_streams, audStream, vidStream, subStream);
    //av_dump_format (avFormatContext, 0, mFilename, 0);

    if (channels > 2)
      channels = 2;
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

        while (mAudFramesLoaded > int(mPlayFrame) + maxAudFrames/2)
          Sleep (40);

        if (avPacket.stream_index == audStream) {
          //{{{  aud packet
          int gotAudio = 0;
          avcodec_decode_audio4 (audCodecContext, audFrame, &gotAudio, &avPacket);

          if (gotAudio) {
            mSamplesPerAcFrame = audFrame->nb_samples;
            mAudFramesPerSec = (float)sampleRate / mSamplesPerAcFrame;
            mAudFrames[mAudFramesLoaded % maxAudFrames].set (0, 2, mSamplesPerAcFrame);

            double valueL = 0;
            double valueR = 0;
            short* ptr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;
            if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
              //{{{  16bit signed planar
              short* lptr = (short*)audFrame->data[0];
              short* rptr = (short*)audFrame->data[1];
              for (int i = 0; i < mSamplesPerAcFrame; i++) {
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
              for (int i = 0; i < mSamplesPerAcFrame; i++) {
                *ptr = (short)(*lptr++ * 0x8000);
                valueL += pow(*ptr++, 2);
                *ptr = (short)(*rptr++ * 0x8000);
                valueR += pow(*ptr++, 2);
                }
              }
              //}}}
            mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerLeft = (float)sqrt (valueL) / (mSamplesPerAcFrame * 2.0f);
            mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerRight = (float)sqrt (valueR) / (mSamplesPerAcFrame * 2.0f);

            mAudFramesLoaded++;
            }
          }
          //}}}
        else if (avPacket.stream_index == vidStream) {
          //{{{  vid packet
          int gotPicture = 0;
          avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &avPacket);

          if (gotPicture) {
            mYuvFrames[mVidFramesLoaded % maxVidFrames].set (
              0, vidFrame->data, vidFrame->linesize, vidCodecContext->width, vidCodecContext->height);

            mVidFramesLoaded++;
            }
          }
          //}}}
        else if (avPacket.stream_index == subStream) {
          //{{{  sub packet
          int gotPicture = 0;
          avsubtitle_free (&sub);
          avcodec_decode_subtitle2 (subCodecContext, &sub, &gotPicture, &avPacket);
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
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);

    // wait for first aud frame to load for sampleRate, channels
    while (mAudFramesLoaded < 1)
      Sleep (40);

    winAudioOpen (sampleRate, 16, channels);

    while (true) {
      if (!mPlaying || (int(mPlayFrame)+1 >= mAudFramesLoaded))
        Sleep (40);
      else if (mAudFrames[int(mPlayFrame) % maxAudFrames].mSamples) {
        winAudioPlay (mAudFrames[int(mPlayFrame) % maxAudFrames].mSamples, channels * mSamplesPerAcFrame *2, 1.0f);
        if (!getMouseDown())
          if ((int(mPlayFrame)+1) < mAudFramesLoaded) {
            mPlayFrame += 1.0;
            changed();
            }
        }
      }

    winAudioClose();

    CoUninitialize();
    }
  //}}}

  //{{{  vars
  wchar_t* mWideFilename;
  char mFilename[100];

  // av stuff
  AVCodecParserContext* vidParser = nullptr;
  AVCodec* vidCodec = nullptr;
  AVCodecContext* vidCodecContext = nullptr;

  AVCodecParserContext* audParser  = nullptr;
  AVCodec* audCodec = nullptr;
  AVCodecContext* audCodecContext = nullptr;

  AVCodecContext* subCodecContext = nullptr;
  AVCodec* subCodec = nullptr;

  cTsSection mTsSection;
  int mDiscontinuity = 0;

  int mService = 1;
  cService* mServicePtr = nullptr;
  int mVidOffset = 25;

  int mVidPid = 1701;
  int mAudPid = 1702;

  int mSamplesPerAcFrame = 1024;
  float mAudFramesPerSec = 40;
  unsigned char channels = 2;
  unsigned long sampleRate = 48000;

  bool mPlaying = true;
  double mPlayFrame = 0;

  int mAudFramesLoaded = 0;
  cAudFrame mAudFrames [maxAudFrames];

  int mVidId = 0;
  int mVidFramesLoaded = 0;
  cYuvFrame mYuvFrames[maxVidFrames];

  ID2D1Bitmap* mBitmap = nullptr;

  uint8_t* mBda = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);
  //{{{
  #ifndef _DEBUG
  //FreeConsole();
  #endif
  //}}}

  printf ("Player %d\n", argc);

  // bda
  //int freq = 674000; // 650000 674000 706000
  //swprintf (fileName, 100, L"C://Users/nnn/Desktop/%d.ts", freq);
  //openTsFile (fileName);
  //createBDAGraph (freq);
  //bdaGraph = true;

  cTvWindow tvWindow;
  tvWindow.run (L"tvWindow", 896, 504, argv[1]);

  CoUninitialize();
  }
//}}}
