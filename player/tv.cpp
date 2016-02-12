// tv.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "ts.h"
//#include "bda.h"

#include "../common/winAudio.h"
#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
//}}}
#define maxAudFrames 10000

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
    mVidPes = (uint8_t*)malloc (100000);

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

      // launch playerThread, higher priority
      auto playerThread = thread ([=]() { player(); });
      SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
      playerThread.detach();
      }
      //}}}
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
  void tsLoader() {

    av_register_all();
    avformat_network_init();

    HANDLE readFile = CreateFile (mWideFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    while (true) {
      uint8_t tsBuf[240*188];
      DWORD sampleLen = 0;
      ReadFile (readFile, tsBuf, 240*188, &sampleLen, NULL);
      if (!sampleLen)
        break;

      size_t i = 0;
      while (i < sampleLen) {
        if ((tsBuf[i] == 0x47) && (((i + 188) >= sampleLen) || (tsBuf[i+188] == 0x47))) {
          int pid =        ((tsBuf[i+1] & 0x1F) << 8) | tsBuf[i+2];
          bool payStart =   (tsBuf[i+1] & 0x40) != 0;
          int continuity =   tsBuf[i+3] & 0x0F;
          int headerBytes = (tsBuf[i+3] & 0x20) ? (5 + tsBuf[i+4]) : 4;

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
              if ((pid == mAudPid) && (pid == serviceIt->second.getAudPid())) {
                //{{{  aud PES
                // look for aud PES startCode
                if (payStart &&
                    (tsBuf[i+4] == 0) && (tsBuf[i+4+1] == 0) && (tsBuf[i+4+2] == 1) && (tsBuf[i+4+3] == 0xC0)) {

                  if (pidInfoIt->second.mBufBytes) {
                    printf ("audPes pid:%d rx:%05d - ", pid, (int)pidInfoIt->second.mBufBytes);
                    for (int i = 0; i < 16; i++) printf ("%02x ", mAudPes[i]);
                    printf ("\n");
                    }

                  //  start of aud PES
                  pidInfoIt->second.mLength = (tsBuf[i+4+4] << 8) | tsBuf[i+4+5];
                  int ph1 = tsBuf[i+4+6];
                  int ph2 = tsBuf[i+4+7];
                  int pesHeadLen = tsBuf[i+4+8];
                  int dataOffset = 4+9 + pesHeadLen;
                  pidInfoIt->second.mLength -= 3 + pesHeadLen;

                  memcpy (mAudPes, tsBuf + i + dataOffset, 188-dataOffset);
                  pidInfoIt->second.mBufBytes = 188-dataOffset;
                  }
                else if (pidInfoIt->second.mBufBytes > 0) {
                  memcpy (mAudPes + pidInfoIt->second.mBufBytes, tsBuf+i+4, 188-4);
                  pidInfoIt->second.mBufBytes += 188-4;
                  }
                }
                //}}}
              else if ((pid == mVidPid) && (pid == serviceIt->second.getVidPid())) {
                //{{{  vid PES
                // look for aud PES startCode
                if (payStart &&
                    (tsBuf[i+4] == 0) && (tsBuf[i+4+1] == 0) && (tsBuf[i+4+2] == 1) && (tsBuf[i+4+3] == 0xE0)) {

                  int vidLen = (int)(pidInfoIt->second.mBufBytes);
                  if (vidLen) {
                    //{{{  decode last vid PES
                    printf ("vidPes pid:%d rx:%05d - ", pid, (int)pidInfoIt->second.mBufBytes);
                    for (int i = 0; i < 16; i++) printf ("%02x ", mVidPes[i]);
                    printf ("\n");

                    AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_MPEG2VIDEO);
                    AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_MPEG2VIDEO);
                    AVCodecContext* vidCodecContext = avcodec_alloc_context3 (vidCodec);
                    avcodec_open2 (vidCodecContext, vidCodec, NULL);

                    AVFrame* vidFrame = av_frame_alloc();

                    AVPacket vidPacket;
                    av_init_packet (&vidPacket);
                    vidPacket.data = mVidPes;
                    vidPacket.size = 0;

                    int bytesUsed = 0;
                    while (bytesUsed || vidLen) {
                      uint8_t* data = NULL;
                      av_parser_parse2 (vidParser, vidCodecContext, &data, &vidPacket.size, vidPacket.data, vidLen, 0, 0, AV_NOPTS_VALUE);

                      int gotPicture = 0;
                      bytesUsed = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
                      if (gotPicture)
                        printf("gotpicture\n");
                        //mYuvFrames[mVidFramesLoaded++].set (vidFrame->data, vidFrame->linesize,
                        //                                    vidCodecContext->width, vidCodecContext->height);

                      vidPacket.data += bytesUsed;
                      vidLen -= bytesUsed;
                      }
                    av_frame_free (&vidFrame);
                    avcodec_close (vidCodecContext);
                    }
                    //}}}

                  //  start of new vid PES
                  pidInfoIt->second.mLength = (tsBuf[i+4+4] << 8) | tsBuf[i+4+5];
                  int ph1 = tsBuf[i+4+6];
                  int ph2 = tsBuf[i+4+7];
                  int pesHeadLen = tsBuf[i+4+8];
                  int dataOffset = 4+9 + pesHeadLen;
                  pidInfoIt->second.mLength -= 3 + pesHeadLen;

                  memcpy (mVidPes, tsBuf+i+dataOffset, 188-dataOffset);
                  pidInfoIt->second.mBufBytes = 188-dataOffset;
                  }

                else if (pidInfoIt->second.mBufBytes > 0) {
                  memcpy (mVidPes + pidInfoIt->second.mBufBytes, tsBuf+i+headerBytes, 188-headerBytes);
                  pidInfoIt->second.mBufBytes += 188-headerBytes;
                  }
                }
                //}}}
              else if (pid == serviceIt->second.getSubPid()) {
                //{{{  subtitle pes
                // look for subtitle PES startCode
                if (payStart &&
                    (tsBuf[i + 4] == 0) && (tsBuf[i + 4 + 1] == 0) && (tsBuf[i + 4 + 2] == 1) && (tsBuf[i + 4 + 3] == 0xBD)) {
                  //  start of subtitle PES
                  pidInfoIt->second.mLength = (tsBuf[i + 4 + 4] << 8) | tsBuf[i + 4 + 5];
                  int ph1 = tsBuf[i + 4 + 6];
                  int ph2 = tsBuf[i + 4 + 7];
                  int pesHeadLen = tsBuf[i + 4 + 8];
                  int dataOffset = 4 + 9 + pesHeadLen;

                  // adjust mLength for PTS header
                  pidInfoIt->second.mLength -= 3 + pesHeadLen;

                  memcpy (pidInfoIt->second.mBuf, tsBuf + i + dataOffset, 188 - dataOffset);
                  pidInfoIt->second.mBufBytes = 188 - dataOffset;
                  }

                else if (pidInfoIt->second.mBufBytes > 0) {
                  // started, no discontinuity, append next bit of PES
                  memcpy (pidInfoIt->second.mBuf + pidInfoIt->second.mBufBytes, tsBuf + i + 4, 188 - 4);
                  pidInfoIt->second.mBufBytes += 188 - 4;
                  }

                if (pidInfoIt->second.mBufBytes >= pidInfoIt->second.mLength) {
                  // enough bytes for PES, should be last packet
                  //dvbsubDecode (pidInfoIt->second.mBuf, pidInfoIt->second.mLength);
                  //printf ("- subtitle PES pid:%d - len:%d bytes:%d\n",
                  //        pid, pidInfoIt->second.mLength, pidInfoIt->second.mBufBytes);
                  }
                }
                //}}}
              }
            }
          //DWORD written; WriteFile (mFile, &tsBuf[i], 188, &written, NULL);
          mPackets++;
          i += 188;
          }
        else {
          //{{{  sync error
          i++;
          printf ("parsePackets - sync error\n");
          }
          //}}}
        }
      }

    CloseHandle (readFile);

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
    vidFramesLoaded = 0;
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
          if (vidFramesLoaded < 2000) {
            int gotPicture = 0;
            avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &avPacket);
            if (gotPicture) {
              // create wicBitmap
              //printf ("got pictuure %d\n", vidFramesLoaded);
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

  int mVidPid = 1701;
  int mAudPid = 1102;
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

  int vidFramesLoaded = 0;
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
