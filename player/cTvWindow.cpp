// tv.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "cTsSection.h"
//#include "bda.h"

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

    mWideFilename = wFilename;
    wcstombs (mFilename, mWideFilename, 100);
    initialise (title, width, height);

    thread ([=]() { tsLoader(); } ).detach();
    //thread ([=]() { fileLoader(); } ).detach();

    // launch playerThread, higher priority
    auto playerThread = thread ([=]() { player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

    // loop in windows message pump till quit
    messagePump();

    //printServices();
    //printPids();
    //printPrograms();
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
  if (vidFrame >= 0) {
    makeBitmap (&mYuvFrames[vidFrame % maxVidFrames]);
    //printf ("playing %d %d %x %x\n",
    //        int(mPlayFrame), vidFrame, mAudFrames[int(mPlayFrame) % maxAudFrames].mPts, mYuvFrames[vidFrame % maxVidFrames].mPts);
    }

  if (mBitmap)
    dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
  else
    dc->Clear (ColorF(ColorF::Black));

  D2D1_RECT_F r = RectF ((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
  dc->FillRectangle (r, getGreyBrush());

  int audFrame = int(mPlayFrame) - (int)(getClientF().width/2);
  for (r.left = 0.0f; (r.left < getClientF().width) && (audFrame < mAudFramesLoaded); r.left++, audFrame++) {
    if (audFrame >= 0) {
      r.top = (getClientF().height/2.0f) - mAudFrames[audFrame % maxAudFrames].mPowerL;
      r.bottom = (getClientF().height/2.0f) + mAudFrames[audFrame % maxAudFrames].mPowerR;
      r.right = r.left + 1.0f;
      dc->FillRectangle (r, getBlueBrush());
      }
    }

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%4.1f %2.0f:%d %d:%d d:%d %d",
            mPlayFrame/mAudFramesPerSec, mPlayFrame, mAudFramesLoaded, vidFrame, mVidFramesLoaded, mDiscontinuity, mVidOffset);
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(getClientF().width-300-2, 2.0f, getClientF().width, getClientF().height), getBlackBrush());
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(getClientF().width-300, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());

  //mTsSection.renderPidInfo  (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
  }
//}}}

private:
  //{{{
  void tsLoader() {

    uint8_t tsBuf[240*188];
    uint8_t* audPes = (uint8_t*)malloc (5000);
    uint8_t* vidPes = (uint8_t*)malloc (500000);
    uint8_t* audPtr = nullptr;
    uint8_t* vidPtr = nullptr;
    //{{{  init av
    av_register_all();

    AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_MPEG2VIDEO);
    AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_MPEG2VIDEO);
    AVCodecContext* vidCodecContext = avcodec_alloc_context3 (vidCodec);
    avcodec_open2 (vidCodecContext, vidCodec, NULL);

    AVCodecParserContext* audParser = av_parser_init (AV_CODEC_ID_MP3);
    AVCodec* audCodec = avcodec_find_decoder (AV_CODEC_ID_MP3);
    AVCodecContext* audCodecContext = avcodec_alloc_context3 (audCodec);
    avcodec_open2 (audCodecContext, audCodec, NULL);
    //}}}

    HANDLE readFile = CreateFile (mWideFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    while (true) {
      //{{{  readFile
      DWORD sampleLen = 0;
      ReadFile (readFile, tsBuf, 240*188, &sampleLen, NULL);
      if (!sampleLen)
        break;
      //}}}
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

      while (mAudFramesLoaded > int(mPlayFrame) + maxAudFrames/2)
        Sleep (40);

      size_t i = 0;
      uint8_t* tsPtr = tsBuf;
      while ((tsPtr < tsBuf + sampleLen) && (*tsPtr++ == 0x47) && ((tsPtr+187 >= tsBuf + sampleLen) || (*(tsPtr+187) == 0x47))) {
        //{{{  parse packetStart
        auto payStart = *tsPtr & 0x40;
        auto pid = ((*tsPtr & 0x1F) << 8) | *(tsPtr+1);
        auto headerBytes = (*(tsPtr+2) & 0x20) ? (4 + *(tsPtr+3)) : 3;
        auto continuity = *(tsPtr+2) & 0x0F;
        tsPtr += headerBytes;
        auto tsFrameBytesLeft = 187 - headerBytes;

        bool discontinuity = false;
        bool isSection = (pid == PID_PAT) || (pid == PID_SDT) || (pid == PID_EIT) || (pid == PID_TDT) ||
                         (mTsSection.mProgramMap.find (pid) != mTsSection.mProgramMap.end());

        tPidInfoMap::iterator pidInfoIt = mTsSection.getPidInfoMap()->find (pid);
        if (pidInfoIt == mTsSection.getPidInfoMap()->end()) {
          // new pid, insert new cPidInfo, get pidInfoIt iterator
          pair<tPidInfoMap::iterator, bool> insPair = mTsSection.getPidInfoMap()->insert (tPidInfoMap::value_type (pid, cPidInfo(pid, isSection)));
          pidInfoIt = insPair.first;
          }
        else if (continuity != ((pidInfoIt->second.mContinuity+1) & 0x0f)) {
          // discontinuity, count all errors
          discontinuity = true;
          mDiscontinuity++;
          if (isSection) // only report section, program continuity error
            printf ("continuity error pid:%d - %x:%x\n", pid,  continuity, pidInfoIt->second.mContinuity);
          pidInfoIt->second.mBufBytes = 0;
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
              mTsSection.parseSection(pid, &tsBuf[j], &tsBuf[sampleLen]);
              j += pidInfoIt->second.mLength + 3;
              pidInfoIt->second.mBufBytes = 0;

              while (tsBuf[j] != 0xFF) {
                // parse more sections
                pidInfoIt->second.mLength = ((tsBuf[j+1] & 0x0f) << 8) | tsBuf[j+2];
                if (j + pidInfoIt->second.mLength + 4 - i < TS_SIZE) {
                  mTsSection.parseSection (pid, &tsBuf[j], &tsBuf[sampleLen]);
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
          //{{{  parse vidPid
          if (discontinuity)
            vidPtr = nullptr;

          if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == 0xe0)) {
            // start next vidPES
            if (vidPtr) {
              // decode last vidPES
              AVPacket vidPacket;
              av_init_packet (&vidPacket);
              vidPacket.data = vidPes;
              vidPacket.size = 0;

              AVFrame* vidFrame = av_frame_alloc();

              int vidLen = int (vidPtr - vidPes);
              vidPtr = vidPes;
              while (vidLen) {
                int len = av_parser_parse2 (vidParser, vidCodecContext, &vidPacket.data, &vidPacket.size, vidPtr, vidLen, 0, 0, AV_NOPTS_VALUE);
                vidPtr += len;
                vidLen -= len;
                if (vidPacket.data) {
                  int gotPicture = 0;
                  int bytesUsed = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
                  if (gotPicture) {
                    printf ("vid pic %d %x %x\n", mVidFramesLoaded, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
                    mYuvFrames[mVidFramesLoaded % maxVidFrames].set (pidInfoIt->second.mPts,
                      vidFrame->data, vidFrame->linesize, vidCodecContext->width, vidCodecContext->height);
                    mVidFramesLoaded++;
                    }
                  vidPacket.data += bytesUsed;
                  vidPacket.size -= bytesUsed;
                  }
                }
              av_frame_free (&vidFrame);
              }

            getTimeStamps (tsPtr, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
            int pesHeaderBytes = 9 + *(tsPtr+8);
            tsPtr += pesHeaderBytes;
            tsFrameBytesLeft -= pesHeaderBytes;
            vidPtr = vidPes;
            }

          if (vidPtr) {
            memcpy (vidPtr, tsPtr, tsFrameBytesLeft);
            vidPtr += tsFrameBytesLeft;
            }
          else
            printf ("discard vid %d\n", pid);
          }
          //}}}
        else if (mServicePtr && (pid == mServicePtr->getAudPid())) {
          //{{{  parse audPid
          if (discontinuity)
            audPtr = nullptr;

          if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == 0xc0)) {
            // start new audPES
            if (audPtr) {
              // decode last audPES
              AVPacket audPacket;
              av_init_packet (&audPacket);
              audPacket.data = audPes;
              audPacket.size = 0;

              AVFrame* audFrame = av_frame_alloc();

              int audLen = int (audPtr - audPes);
              audPtr = audPes;
              while (audLen) {
                int len = av_parser_parse2 (audParser, audCodecContext, &audPacket.data, &audPacket.size, audPtr, audLen, 0, 0, AV_NOPTS_VALUE);
                audPtr += len;
                audLen -= len;
                if (audPacket.data) {
                  int gotPicture = 0;
                  int bytesUsed = avcodec_decode_audio4 (audCodecContext, audFrame, &gotPicture, &audPacket);
                  samples = audFrame->nb_samples;
                  mAudFramesPerSec = (float)sampleRate / samples;

                  mAudFrames[mAudFramesLoaded % maxAudFrames].set (pidInfoIt->second.mPts, 2, samples);

                  if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
                    //{{{  16bit signed planar
                    short* lptr = (short*)audFrame->data[0];
                    short* rptr = (short*)audFrame->data[1];
                    short* ptr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;

                    double valueL = 0;
                    double valueR = 0;
                    for (int i = 0; i < audFrame->nb_samples; i++) {
                      *ptr = *lptr++;
                      valueL += pow(*ptr++, 2);
                      *ptr = *rptr++;
                      valueR += pow(*ptr++, 2);
                      }

                    mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerL = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
                    mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerR = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
                    }
                    //}}}
                  else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
                    //{{{  32bit float planar
                    float* lptr = (float*)audFrame->data[0];
                    float* rptr = (float*)audFrame->data[1];
                    short* ptr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;

                    double valueL = 0;
                    double valueR = 0;
                    for (int i = 0; i < audFrame->nb_samples; i++) {
                      *ptr = (short)(*lptr++ * 0x8000);
                      valueL += pow(*ptr++, 2);
                      *ptr = (short)(*rptr++ * 0x8000);
                      valueR += pow(*ptr++, 2);
                      }

                    mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerR = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
                    mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerR = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
                    }
                    //}}}
                  else
                    printf ("new sample_fmt:%d\n", audCodecContext->sample_fmt);
                  printf ("aud samples %d %d %x %x\n", samples, mAudFramesLoaded, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
                  mAudFramesLoaded++;

                  audPacket.data += bytesUsed;
                  audPacket.size -= bytesUsed;
                  }
                }
              av_frame_free (&audFrame);
              }

            getTimeStamps (tsPtr, pidInfoIt->second.mPts, pidInfoIt->second.mDts);
            int pesHeaderBytes = 9 + *(tsPtr+8);
            tsPtr += pesHeaderBytes;
            tsFrameBytesLeft -= pesHeaderBytes;

            // reset pes pointer
            audPtr = audPes;
            }

          if (audPtr) {
            memcpy (audPtr, tsPtr, tsFrameBytesLeft);
            audPtr += tsFrameBytesLeft;
            }
          else
            printf ("discard aud %d\n", pid);
          }
          //}}}

        tsPtr += tsFrameBytesLeft;
        i += 188;
        }
      }

    CloseHandle (readFile);
    avcodec_close (audCodecContext);
    avcodec_close (vidCodecContext);
    }
  //}}}
  //{{{
  void fileLoader() {

    //{{{  av init
    av_register_all();
    avformat_network_init();

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

    if (vidStream >= 0) {
      vidCodecContext = avFormatContext->streams[vidStream]->codec;
      vidCodec = avcodec_find_decoder (vidCodecContext->codec_id);
      avcodec_open2 (vidCodecContext, vidCodec, NULL);
      vidFrame = av_frame_alloc();
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

    AVPacket avPacket;
    while (true) {
      while (mAudFramesLoaded > int(mPlayFrame) + maxAudFrames/2)
        Sleep (40);

      while (av_read_frame (avFormatContext, &avPacket) >= 0) {
        if (avPacket.stream_index == audStream) {
          //{{{  aud packet
          int gotAudio = 0;
          avcodec_decode_audio4 (audCodecContext, audFrame, &gotAudio, &avPacket);
          if (gotAudio && (mAudFramesLoaded < maxAudFrames)) {
            samples = audFrame->nb_samples;
            mAudFramesPerSec = (float)sampleRate / samples;

            mAudFrames[mAudFramesLoaded % maxAudFrames].set (0, 2, samples);

            if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
              //{{{  16bit signed planar
              short* lptr = (short*)audFrame->data[0];
              short* rptr = (short*)audFrame->data[1];
              short* ptr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;

              double valueL = 0;
              double valueR = 0;
              for (int i = 0; i < audFrame->nb_samples; i++) {
                *ptr = *lptr++;
                valueL += pow(*ptr++, 2);
                *ptr = *rptr++;
                valueR += pow(*ptr++, 2);
                }

              mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerL = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
              mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerR = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
              }
              //}}}
            else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
              //{{{  32bit float planar
              float* lptr = (float*)audFrame->data[0];
              float* rptr = (float*)audFrame->data[1];
              short* ptr = (short*)mAudFrames[mAudFramesLoaded % maxAudFrames].mSamples;

              double valueL = 0;
              double valueR = 0;
              for (int i = 0; i < audFrame->nb_samples; i++) {
                *ptr = (short)(*lptr++ * 0x8000);
                valueL += pow(*ptr++, 2);
                *ptr = (short)(*rptr++ * 0x8000);
                valueR += pow(*ptr++, 2);
                }

              mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerL = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
              mAudFrames[mAudFramesLoaded % maxAudFrames].mPowerR = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
              }
              //}}}
            else
              printf ("new sample_fmt:%d\n",audCodecContext->sample_fmt);
            mAudFramesLoaded++;
            }
          }
          //}}}
        else if (avPacket.stream_index == vidStream) {
          //{{{  vid packet
          if (mVidFramesLoaded < 2000) {
            int gotPicture = 0;
            avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &avPacket);
            if (gotPicture) {
              mYuvFrames[mVidFramesLoaded % maxVidFrames].set (
                0, vidFrame->data, vidFrame->linesize, vidCodecContext->width, vidCodecContext->height);
              mVidFramesLoaded++;
              }
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
        winAudioPlay (mAudFrames[int(mPlayFrame) % maxAudFrames].mSamples, channels * samples*2, 1.0f);
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
  int64_t getTimeStamp (bool isPts, uint8_t* tsPtr) {

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
  void getTimeStamps (uint8_t* tsPtr, int64_t& pts, int64_t& dts) {

    pts = ((*(tsPtr+8) >= 5) && (*(tsPtr+7) & 0x80)) ? getTimeStamp (true, tsPtr) : 0;
    dts = ((*(tsPtr+8) >= 10) && (*(tsPtr+7) & 0x40)) ? getTimeStamp (false, tsPtr) : 0;

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

  //{{{  vars
  wchar_t* mWideFilename;
  char mFilename[100];

  cTsSection mTsSection;
  int mDiscontinuity = 0;

  int mService = 6;
  cService* mServicePtr = nullptr;
  int mVidOffset = 25;

  int mVidPid = 1701;
  int mAudPid = 1702;

  int samples = 1024;
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
