// tv.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "ts.h"
//#include "bda.h"
#include "../common/yuvrgb_sse2.h"

#include "../common/winAudio.h"
#include "../common/cYuvFrame.h"
#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
//}}}
#define maxAudFrames 10000
#define maxVidFrames 200


class cTvWindow : public cD2dWindow {
public:
  cTvWindow() {}
  virtual ~cTvWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFilename) {

    mWideFilename = wFilename;
    wcstombs (mFilename, mWideFilename, 100);
    initialise (title, width, height);

    mAudPes = (uint8_t*)malloc (10000);
    mVidPes = (uint8_t*)malloc (1000000);

    thread ([=]() { tsLoader(); } ).detach();

    if (false) {
      //{{{  laucnh loader
      av_register_all();
      avformat_network_init();
      mAvFormatContext = NULL;
      avformat_open_input (&mAvFormatContext, mFilename, NULL, NULL);
      avformat_find_stream_info (mAvFormatContext, NULL);
      for (unsigned int i = 0; i < mAvFormatContext->nb_streams; i++)
        if ((mAvFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (vidStream == -1))
          vidStream = i;
        else if ((mAvFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) && (audStream == -1))
          audStream = i;
        else if ((mAvFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) && (subStream == -1))
          subStream = i;

      if (audStream >= 0) {
        channels = mAvFormatContext->streams[audStream]->codec->channels;
        sampleRate = mAvFormatContext->streams[audStream]->codec->sample_rate;
        }

      printf ("filename:%s sampleRate:%d channels:%d streams:%d aud:%d vid:%d sub:%d\n",
              mFilename, sampleRate, channels, mAvFormatContext->nb_streams, audStream, vidStream, subStream);
      //av_dump_format (mAvFormatContext, 0, mFilename, 0);

      if (channels > 2)
        channels = 2;

      // launch loaderThread
      thread ([=]() { loader(); } ).detach();
      }
      //}}}

    // launch playerThread, higher priority
    auto playerThread = thread ([=]() { player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

    // loop in windows message pump till quit
    messagePump();
    }
  //}}}

protected:
//{{{
bool onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    case 0x20 : playing = !playing; break;

    case 0x21 : playFrame -= 5.0f * audFramesPerSec; changed(); break;
    case 0x22 : playFrame += 5.0f * audFramesPerSec; changed(); break;

    case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
    case 0x24 : playFrame = 0; break;

    case 0x25 : playFrame -= 1.0f * audFramesPerSec; changed(); break;
    case 0x27 : playFrame += 1.0f * audFramesPerSec; changed(); break;

    case 0x26 : playing = false; playFrame -=2; changed(); break;
    case 0x28 : playing = false; playFrame +=2; changed(); break;

    case 0x2d : break;
    case 0x2e : break;

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
void onMouseUp  (bool right, bool mouseMoved, int x, int y) {

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
void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  playFrame -= xInc;

  if (playFrame < 0)
    playFrame = 0;
  else if (playFrame >= audFramesLoaded)
    playFrame = audFramesLoaded - 1.0f;

  changed();
  }
//}}}
//{{{
void onDraw (ID2D1DeviceContext* dc) {

  makeBitmap (mVidFrame);
  if (mBitmap)
    dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
  else
    dc->Clear (ColorF(ColorF::Black));

  D2D1_RECT_F r = RectF((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
  dc->FillRectangle (r, getGreyBrush());

  int j = int(playFrame) - (int)(getClientF().width/2);
  for (float i = 0.0f; (i < getClientF().width) && (j < audFramesLoaded); i++, j++) {
    r.left = i;
    r.right = i + 1.0f;
    if (j >= 0) {
      r.top = (getClientF().height/2.0f) - audFramesPowerL[j];
      r.bottom = (getClientF().height/2.0f) + audFramesPowerR[j];
      dc->FillRectangle (r, getBlueBrush());
      }
    }

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%4.1fs - %2.0f:%d", playFrame/audFramesPerSec, playFrame, audFramesLoaded);
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(getClientF().width-200-2, 2.0f, getClientF().width, getClientF().height), getBlackBrush());
  dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                RectF(getClientF().width-200, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());

  renderPidInfo  (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
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
  void tsLoader() {

    uint8_t tsBuf[240*188];
    uint8_t* audPtr = nullptr;
    uint8_t* vidPtr = nullptr;

    av_register_all();
    avformat_network_init();

    AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_MPEG2VIDEO);
    AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_MPEG2VIDEO);
    AVCodecContext* vidCodecContext = avcodec_alloc_context3 (vidCodec);
    avcodec_open2 (vidCodecContext, vidCodec, NULL);

    HANDLE readFile = CreateFile (mWideFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    while (true) {
      DWORD sampleLen = 0;
      ReadFile (readFile, tsBuf, 240*188, &sampleLen, NULL);
      if (!sampleLen)
        break;
      //printf ("readfile %d\n", sampleLen);

      uint8_t* ptr = tsBuf;

      size_t i = 0;
      while ((ptr < tsBuf + sampleLen) && (*ptr++ == 0x47) &&
             ((ptr+187 >= tsBuf + sampleLen) || (*(ptr+187) == 0x47))) {
        auto payStart = *ptr & 0x40;
        auto pid = ((*ptr & 0x1F) << 8) | *(ptr+1);
        auto headerBytes = (*(ptr+2) & 0x20) ? (4 + *(ptr+3)) : 3;
        auto continuity = *(ptr+2) & 0x0F;
        ptr += headerBytes;
        auto tsFrameBytesLeft = 187 - headerBytes;

        bool discontinuity = false;
        bool isSection = (pid == PID_PAT) || (pid == PID_SDT) || (pid == PID_EIT) || (pid == PID_TDT) ||
                         (mProgramMap.find (pid) != mProgramMap.end());

        tPidInfoMap::iterator pidInfoIt = mPidInfoMap.find (pid);
        if (pidInfoIt == mPidInfoMap.end()) {
          //{{{  new pid, new cPidInfo
          printf ("PID new cPidInfo %d\n", pid);

          // insert new cPidInfo, get pidInfoIt iterator
          pair<tPidInfoMap::iterator, bool> resultPair = mPidInfoMap.insert (tPidInfoMap::value_type (pid, cPidInfo(pid, isSection)));
          pidInfoIt = resultPair.first;
          }
          //}}}
        else if (continuity != ((pidInfoIt->second.mContinuity+1) & 0x0f)) {
          //{{{  discontinuity
          // count all continuity error
          mDiscontinuity++;
          discontinuity = true;

          if (isSection) // only report section, program continuity error
            printf ("continuity error %d pid:%d - %x:%x\n", mPackets, pid,  continuity, pidInfoIt->second.mContinuity);

          pidInfoIt->second.mBufBytes = 0;
          }
          //}}}
        pidInfoIt->second.mContinuity = continuity;
        pidInfoIt->second.mTotal++;

        if (isSection) {
          if (payStart) {
            //{{{  parse section start
            int pointerField = tsBuf[i+4];
            if ((pointerField > 0) && (pidInfoIt->second.mBufBytes > 0)) {
              // payloadStart has end of lastSection
              memcpy (&pidInfoIt->second.mBuf[pidInfoIt->second.mBufBytes], &tsBuf[i+5], pointerField);

              pidInfoIt->second.mBufBytes += pointerField;
              if (pidInfoIt->second.mLength + 3 <= pidInfoIt->second.mBufBytes) {
                parseEit (pidInfoIt->second.mBuf,0);
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
              parseSection(pid, &tsBuf[j], &tsBuf[sampleLen]);
              j += pidInfoIt->second.mLength + 3;
              pidInfoIt->second.mBufBytes = 0;

              while (tsBuf[j] != 0xFF) {
                // parse more sections
                pidInfoIt->second.mLength = ((tsBuf[j+1] & 0x0f) << 8) | tsBuf[j+2];
                if (j + pidInfoIt->second.mLength + 4 - i < TS_SIZE) {
                  parseSection (pid, &tsBuf[j], &tsBuf[sampleLen]);
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
            //{{{  parse section continuation
            memcpy (&pidInfoIt->second.mBuf[pidInfoIt->second.mBufBytes], &tsBuf[i+4], TS_SIZE - 4);
            pidInfoIt->second.mBufBytes += TS_SIZE - 4;

            if (pidInfoIt->second.mLength + 3 <= pidInfoIt->second.mBufBytes) {
              parseSection (pid, pidInfoIt->second.mBuf, 0);
              pidInfoIt->second.mBufBytes = 0;
              }
            }
            //}}}
          }
        else {
          tServiceMap::iterator serviceIt = mServiceMap.find (pidInfoIt->second.mSid);
          if (serviceIt != mServiceMap.end()) {
            if ((pid == mVidPid) && (pid == serviceIt->second.getVidPid())) {
              //{{{  vidPid
              if (discontinuity) {
                printf ("vid disc------------------------------------------\n");
                vidPtr = nullptr;
                }

              if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == 0xe0)) {
                if (vidPtr) {
                  //{{{  decode last vidPES
                  uint8_t* ptr = mVidPes;
                  int vidLen = int (vidPtr - mVidPes);
                  //printf ("vidPes %d %d - ", pid, vidLen); for (int j = 0; j < 16; j++) printf ("%02x ", mVidPes[j]); printf ("\n");

                  AVFrame* vidFrame = av_frame_alloc();

                  AVPacket vidPacket;
                  av_init_packet (&vidPacket);
                  vidPacket.data = mVidPes;
                  vidPacket.size = 0;

                  int bytesUsed = 0;
                  while (vidLen) {
                    int len = av_parser_parse2 (vidParser, vidCodecContext, &vidPacket.data, &vidPacket.size, ptr, vidLen, 0, 0, AV_NOPTS_VALUE);
                    ptr += len;
                    vidLen -= len;

                    if (vidPacket.data) {
                      int gotPicture = 0;
                      bytesUsed = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
                      if (gotPicture && (mVidFramesLoaded < maxVidFrames))
                        mYuvFrames[mVidFramesLoaded++].set (vidFrame->data, vidFrame->linesize,
                                                            vidCodecContext->width, vidCodecContext->height);
                      vidPacket.data += bytesUsed;
                      vidPacket.size -= bytesUsed;
                      //printf ("got:%d %d %d\n", gotPicture, bytesUsed, vidPacket.size);
                      }

                    //printf ("- vidPes %d %d\n", vidLen, bytesUsed);
                    }

                  av_frame_free (&vidFrame);
                  }
                  //}}}

                int pesHeaderBytes = 9 + *(ptr+8);
                ptr += pesHeaderBytes;
                tsFrameBytesLeft -= pesHeaderBytes;
                vidPtr = mVidPes;
                }

              if (vidPtr) {
                memcpy (vidPtr, ptr, tsFrameBytesLeft);
                vidPtr += tsFrameBytesLeft;
                }
              else
                printf ("discard vid\n");
              }
              //}}}
            else if ((pid == mAudPid) && (pid == serviceIt->second.getAudPid())) {
              //{{{  audPid
              if (discontinuity) {
                printf ("aud discontinuity --------------------\n");
                audPtr = nullptr;
                }

              if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == 0xc0)) {
                if (audPtr) {
                  //{{{  decode last audPES
                  int audLen = int (audPtr - mAudPes);
                  //printf ("audPes %d %d - ", pid, audLen);
                  //for (int j = 0; j < 16; j++) printf ("%02x ", mAudPes[j]);
                  //printf ("\n");

                  AVCodecParserContext* audParser = av_parser_init (AV_CODEC_ID_MP3);
                  AVCodec* audCodec = avcodec_find_decoder (AV_CODEC_ID_MP3);
                  AVCodecContext* audCodecContext = avcodec_alloc_context3 (audCodec);
                  avcodec_open2 (audCodecContext, audCodec, NULL);

                  AVFrame* audFrame = av_frame_alloc();

                  AVPacket audPacket;
                  av_init_packet (&audPacket);
                  audPacket.data = mAudPes;
                  audPacket.size = 0;

                  int bytesUsed = 0;
                  while (bytesUsed || audLen) {
                    uint8_t* data = NULL;
                    av_parser_parse2 (audParser, audCodecContext, &data, &audPacket.size, audPacket.data, audLen, 0, 0, AV_NOPTS_VALUE);

                    int gotPicture = 0;
                    bytesUsed = avcodec_decode_audio4 (audCodecContext, audFrame, &gotPicture, &audPacket);
                    if (gotPicture && (audFramesLoaded < maxAudFrames)) {
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

                    audPacket.data += bytesUsed;
                    audLen -= bytesUsed;
                    //printf ("audPes %d %d\n", audLen, bytesUsed);
                    }

                  av_frame_free (&audFrame);
                  avcodec_close (audCodecContext);
                  }
                  //}}}

                // start new audPES
                int pesHeaderBytes = 9 + *(ptr+8);
                ptr += pesHeaderBytes;
                tsFrameBytesLeft -= pesHeaderBytes;

                // reset pes pointer
                audPtr = mAudPes;
                }

              if (audPtr) {
                memcpy (audPtr, ptr, tsFrameBytesLeft);
                audPtr += tsFrameBytesLeft;
                }
              else
                printf ("discard aud\n");
              }
              //}}}
            }
          }

        ptr += tsFrameBytesLeft;
        i += 188;
        }

      int mVidLen = (int)(vidPtr - mVidPes);
      int mAudLen = (int)(audPtr - mAudPes);
      }

    CloseHandle (readFile);
    avcodec_close (vidCodecContext);
    //printServices();
    //printPids();
    //printPrograms();
    }
  //}}}
  //{{{
  void loader() {

    //{{{  aud init
    AVCodecContext* audCodecContext = NULL;
    AVCodec* audCodec = NULL;
    AVFrame* audFrame = NULL;

    if (audStream >= 0) {
      audCodecContext = mAvFormatContext->streams[audStream]->codec;
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
      vidCodecContext = mAvFormatContext->streams[vidStream]->codec;
      vidCodec = avcodec_find_decoder (vidCodecContext->codec_id);
      avcodec_open2 (vidCodecContext, vidCodec, NULL);
      vidFrame = av_frame_alloc();
      }
    //}}}
    //{{{  sub init
    AVCodecContext* subCodecContext = NULL;
    AVCodec* subCodec = NULL;

    if (subStream >= 0) {
      subCodecContext = mAvFormatContext->streams[subStream]->codec;
      subCodec = avcodec_find_decoder (subCodecContext->codec_id);
      avcodec_open2 (subCodecContext, subCodec, NULL);
      }
    //}}}
    AVSubtitle sub;
    memset (&sub, 0, sizeof(AVSubtitle));

    audFramesLoaded = 0;
    mVidFramesLoaded = 0;
    AVPacket avPacket;
    while (true) {
      while (av_read_frame (mAvFormatContext, &avPacket) >= 0) {
        if (avPacket.stream_index == audStream) {
          // aud packet
          int gotPicture = 0;
          avcodec_decode_audio4 (audCodecContext, audFrame, &gotPicture, &avPacket);
          if (gotPicture && (audFramesLoaded < maxAudFrames)) {
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
          if (mVidFramesLoaded < 2000) {
            int gotPicture = 0;
            avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &avPacket);
            if (gotPicture) {
              // create wicBitmap
              //printf ("got pictuure %d\n", vidFramesLoaded);
              mVidFramesLoaded++;
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
    avformat_close_input (&mAvFormatContext);
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);
    // wait for first aud frame to load for sampleRate, channels
    while (audFramesLoaded < 1)
      Sleep (40);

    winAudioOpen (sampleRate, 16, channels);

    while (true) {
      if (!playing || (int(playFrame)+1 >= audFramesLoaded))
        Sleep (40);
      else if (audFrames[int(playFrame)]) {
        winAudioPlay (audFrames[int(playFrame)], channels * samples*2, 1.0f);
        if (!getMouseDown())
          if ((int(playFrame)+1) < audFramesLoaded) {
            playFrame += 1.0;
            changed();
            }
        }
      if (int(playFrame) < maxVidFrames)
        mVidFrame = &mYuvFrames[int(playFrame)];
      }

    winAudioClose();
    CoUninitialize();
    }
  //}}}

  //{{{  vars
  wchar_t* mWideFilename;
  char mFilename[100];

  int vidStream = -1;
  int audStream = -1;
  int subStream = -1;
  AVFormatContext* mAvFormatContext;

  int mVidPid = 1401;
  int mAudPid = 1402;
  uint8_t* mAudPes = nullptr;
  uint8_t* mVidPes = nullptr;

  float audFramesPerSec = 0.0;
  unsigned char channels = 2;
  unsigned long sampleRate = 48000;
  int samples = 1024;

  bool playing = true;
  double playFrame = 0;

  int audFramesLoaded = 0;
  void* audFrames [maxAudFrames];
  float audFramesPowerL [maxAudFrames];
  float audFramesPowerR [maxAudFrames];

  int mVidId = 0;
  cYuvFrame mYuvFrames[maxVidFrames];
  int mVidFramesLoaded = 0;
  ID2D1Bitmap* mBitmap = nullptr;
  cYuvFrame* mVidFrame = nullptr;
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
  // CreateThread (NULL, 0, tsFileLoaderThread, argv[2], 0, NULL);
  //  bda
  //int freq = 674000; // 650000 674000 706000
  //wchar_t fileName [100];
  //swprintf (fileName, 100, L"C://Users/nnn/Desktop/%d.ts", freq);
  //openTsFile (fileName);
  //createBDAGraph (freq);
  //bdaGraph = true;

  cTvWindow tvWindow;
  tvWindow.run (L"tvWindow", 896, 504, argv[1]);

  CoUninitialize();
  }
//}}}
