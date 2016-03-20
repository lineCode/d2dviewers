// h264window.cpp
//{{{  includes
#include "pch.h"

#include "../common/cAudFrame.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"
#include "../common/cYuvFrame.h"
#include "../common/cMpeg2decoder.h"

#include "ldecod.h"

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#pragma comment(lib,"welsdec.lib")

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
//}}}

#define maxAudFrames 1000
#define kMaxVidFrames 10000
cAudFrame* mAudFrames[maxAudFrames];

class cAppWindow : public cD2dWindow, public cMpeg2decoder {
public:
  cAppWindow() {}
  ~cAppWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFilename, char* filename) {

    initialise (title, width, height);

    // launch playerThread, higher priority
    thread ([=]() { playerffMpeg2 (wFilename, filename); }).detach();
    //thread ([=]() { playerMpeg2 (wFilename, filename); }).detach();
    //thread ([=]() { playerReference264(); }).detach();
    //thread ([=]() { playerOpenH264(); }).detach();

    //thread ([=]() { playerffMp3 (wFilename, filename); }).detach();
    //thread ([=]() { audioPlayer(); }).detach();

    // loop in windows message pump till quit
    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x10: // shift
      case 0x11: // control
      case 0x00: return false;
      case 0x1B: return true;

      //case 0x20: stopped = !stopped; break;

      //case 0x21: playFrame -= 5.0f * audFramesPerSec; changed(); break;
      //case 0x22: playFrame += 5.0f * audFramesPerSec; changed(); break;

      //case 0x23: playFrame = audFramesLoaded - 1.0f; changed(); break;
      //case 0x24: playFrame = 0; break;

      //case 0x25: playFrame -= 1.0f * audFramesPerSec; changed(); break;
      //case 0x27: playFrame += 1.0f * audFramesPerSec; changed(); break;

      //case 0x26: stopped = true; playFrame -=2; changed(); break;
      //case 0x28: stopped = true; playFrame +=2; changed(); break;

      //case 0x2d: markFrame = playFrame - 2.0f; changed(); break;
      //case 0x2e: playFrame = markFrame; changed(); break;

      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    //playFrame -= xInc;

    changed();
    }
  //}}}
  //{{{
  void onMouseUp  (bool right, bool mouseMoved, int x, int y) {

    //if (!mouseMoved)
      //playFrame += x - (getClientF().width/2.0f);
    changed();
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (makeBitmap (&mYuvFrames[mLoadVidFrame], mBitmap, mBitmapPts))
    //if (makeBitmap (getYuvFrame (mCurVidFrame), mBitmap, mBitmapPts))
      dc->DrawBitmap (mBitmap, RectF (0.0f, 0.0f, getClientF().width, getClientF().height));
    else
      dc->Clear (ColorF(ColorF::Black));

    dc->FillRectangle (RectF ((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height), getGreyBrush());

    wchar_t wStr[200];
    swprintf (wStr, 200, L"helloColin");
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                  RectF(0.0f, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());
    }
  //}}}

private:
  //{{{
  void playerffMpeg2 (wchar_t* wFilename,char* filename) {

    auto fileHandle = CreateFile (wFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
      return;

    int mFileBytes = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    av_register_all();

    AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_MPEG2VIDEO);
    AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_MPEG2VIDEO);
    AVCodecContext* vidContext = avcodec_alloc_context3 (vidCodec);
    avcodec_open2 (vidContext, vidCodec, NULL);

    //  hd ffmpeg decode
    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = fileBuffer;
    avPacket.size = 0;

    auto time = startTimer();

    int pesLen = mFileBytes;
    uint8_t* mBufPtr = fileBuffer;
    while (pesLen) {
      auto lenUsed = av_parser_parse2 (vidParser, vidContext, &avPacket.data, &avPacket.size, mBufPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      mBufPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        auto avFrame = av_frame_alloc();
        auto got = 0;
        auto bytesUsed = avcodec_decode_video2 (vidContext, avFrame, &got, &avPacket);
        if (bytesUsed >= 0) {
          avPacket.data += bytesUsed;
          avPacket.size -= bytesUsed;
          }
        if (got) {
          mYuvFrames [mCurVidFrame % kMaxVidFrames].set (mCurVidFrame,
            avFrame->data, avFrame->linesize, vidContext->width, vidContext->height, pesLen, avFrame->pict_type);
          mLoadVidFrame = mCurVidFrame;
          mCurVidFrame++;
          if (mLoadVidFrame && !(mLoadVidFrame % 100))
            printf ("frame %d  %4.3fs %4.3fms\n", mLoadVidFrame, getTimer() - time, 1000.0 * (getTimer() - time) / mLoadVidFrame);
          }
        av_frame_free (&avFrame);
        }
      }

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}

  //{{{
  uint8_t limit (double v) {

    if (v <= 0.0)
      return 0;

    if (v >= 255.0)
      return 255;

    return (uint8_t)v;
    }
  //}}}
  //{{{
  //void playerReference264 (char* filename) {

    //OpenDecoder (filename);

    //int frame = 0;
    //int iRet;
    //do  {
      //iRet = DecodeOneFrame (&pDecPicList);
      //int iWidth = pDecPicList->iWidth*((pDecPicList->iBitDepth+7)>>3);
      //int iHeight = pDecPicList->iHeight;
      //int iStride = pDecPicList->iYBufStride;
      //if (iWidth && iHeight) {
        ////makeVidFrame (frame++, pDecPicList->pY, pDecPicList->pU, pDecPicList->pV, iWidth, iHeight, iWidth, iWidth/2);
        //changed();
        //}
      //} while (iRet == DEC_EOS || iRet == DEC_SUCCEED);

    //FinitDecoder (&pDecPicList);
    //CloseDecoder();
    //}
  //}}}
  //{{{
  //void playerOpenH264 (char* filename) {

    //{{{  init decoder
    //ISVCDecoder* pDecoder = NULL;
    //WelsCreateDecoder (&pDecoder);

    //int iLevelSetting = (int)WELS_LOG_WARNING;
    //pDecoder->SetOption (DECODER_OPTION_TRACE_LEVEL, &iLevelSetting);

    //// init decoder params
    //SDecodingParam sDecParam = {0};
    //sDecParam.sVideoProperty.size = sizeof (sDecParam.sVideoProperty);
    //sDecParam.uiTargetDqLayer = (uint8_t) - 1;
    //sDecParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    //sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    //pDecoder->Initialize (&sDecParam);
    //}}}
    //{{{  open file, find size, read file and append startCode
    //FILE* pH264File = fopen (filename, "rb");
    //fseek (pH264File, 0L, SEEK_END);
    //int32_t iFileSize = (int32_t) ftell (pH264File);
    //fseek (pH264File, 0L, SEEK_SET);

    //// read file
    //uint8_t* pBuf = new uint8_t[iFileSize + 4];
    //fread (pBuf, 1, iFileSize, pH264File);
    //fclose (pH264File);

    //// append slice startCode
    //uint8_t uiStartCode[4] = {0, 0, 0, 1};
    //memcpy (pBuf + iFileSize, &uiStartCode[0], 4);
    //}}}
    //{{{  set conceal option
    //int32_t iErrorConMethod = (int32_t) ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
    //pDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);
    //}}}

    //int32_t iWidth = 0;
    //int32_t iHeight = 0;
    //int frameIndex = 0;
    //unsigned long long uiTimeStamp = 0;
    //int32_t iBufPos = 0;
    //while (true) {
      //// process slice
      //int32_t iEndOfStreamFlag = 0;
      //if (iBufPos >= iFileSize) {
        //{{{  end of stream
        //iEndOfStreamFlag = true;
        //pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
        //break;
        //}
        //}}}

      //// find sliceSize by looking for next slice startCode
      //int32_t iSliceSize;
      //for (iSliceSize = 1; iSliceSize < iFileSize; iSliceSize++)
        //if ((!pBuf[iBufPos+iSliceSize] && !pBuf[iBufPos+iSliceSize+1])  &&
            //((!pBuf[iBufPos+iSliceSize+2] && pBuf[iBufPos+iSliceSize+3] == 1) || (pBuf[iBufPos+iSliceSize+2] == 1)))
          //break;

      //uint8_t* pData[3]; // yuv ptrs
      //SBufferInfo sDstBufInfo;
      //memset (&sDstBufInfo, 0, sizeof (SBufferInfo));

      //sDstBufInfo.uiInBsTimeStamp = uiTimeStamp++;
      ////pDecoder->DecodeFrame2 (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);
      //pDecoder->DecodeFrameNoDelay (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);

      //if (sDstBufInfo.iBufferStatus == 1) {
        ////makeVidFrame (frameIndex++, pData[0], pData[1], pData[2], 640, 360,
        ////              sDstBufInfo.UsrData.sSystemBuffer.iStride[0], sDstBufInfo.UsrData.sSystemBuffer.iStride[1]);
        //iWidth = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
        //iHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
        //}
      //iBufPos += iSliceSize;
      //}

    //delete[] pBuf;
    //pDecoder->Uninitialize();
    //WelsDestroyDecoder (pDecoder);
    //}
  //}}}
  //{{{
  void playerMpeg2 (wchar_t* wFilename,char* filename) {

    auto fileHandle = CreateFile (wFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
      return;

    int mFileBytes = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    auto time = startTimer();
    uint8_t* pesPtr = fileBuffer;
    for (int i = 0; i < 10000; i++) {
      if (!(i % 100))
        printf ("frame %d  %4.3fs %4.3fms\n", i, getTimer() - time, 1000.0 * (getTimer() - time) / i);
      decodePes (pesPtr, fileBuffer + mFileBytes, i, pesPtr);
      mCurVidFrame = i % 40;
      }

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}
  //{{{
  void playerffMp3 (wchar_t* wFilename,char* filename) {

    auto fileHandle = CreateFile (wFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
      return;

    int mFileBytes = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    av_register_all();
    AVCodecParserContext* audParser = av_parser_init (AV_CODEC_ID_MP3);
    AVCodec* audCodec = avcodec_find_decoder (AV_CODEC_ID_MP3);
    AVCodecContext*audContext = avcodec_alloc_context3 (audCodec);
    avcodec_open2 (audContext, audCodec, NULL);

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = fileBuffer;
    avPacket.size = 0;

    auto time = startTimer();

    int pesLen = mFileBytes;
    uint8_t* pesPtr = fileBuffer;
    while (pesLen) {
      auto lenUsed = av_parser_parse2 (audParser, audContext, &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      pesPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        auto got = 0;
        auto avFrame = av_frame_alloc();
        auto bytesUsed = avcodec_decode_audio4 (audContext, avFrame, &got, &avPacket);
        if (bytesUsed >= 0) {
          avPacket.data += bytesUsed;
          avPacket.size -= bytesUsed;

          if (got) {
            mSampleRate = avFrame->sample_rate;
            mAudFrames[mLoadAudFrame] = new cAudFrame();
            mAudFrames[mLoadAudFrame]->set (0, avFrame->channels, 48000, avFrame->nb_samples);
            auto samplePtr = (short*)mAudFrames[mLoadAudFrame]->mSamples;
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
            mAudFrames[mLoadAudFrame]->mPower[0] = (float)sqrt (valueL) / (avFrame->nb_samples * 2.0f);
            mAudFrames[mLoadAudFrame]->mPower[1] = (float)sqrt (valueR) / (avFrame->nb_samples * 2.0f);
            mLoadAudFrame++;
            changed();
            }
          av_frame_free (&avFrame);
          }
        }
      }

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}

  int mCurVidFrame = 0;
  int mLoadVidFrame = 0;
  int64_t mBitmapPts = -1;
  ID2D1Bitmap* mBitmap = nullptr;
  cYuvFrame mYuvFrames[kMaxVidFrames];
  DecodedPicList* pDecPicList;

  int mSampleRate = 0;
  int mPlayAudFrame = 0;
  int mLoadAudFrame = 0;
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  char filename[100];
  strcpy (filename, "C:\\Users\\colin\\Desktop\\d2dviewers\\test.264");
  if (argv[1])
    wcstombs (filename, argv[1], wcslen(argv[1])+1);

  printf ("Player %d %s/n", argc, filename);

  cAppWindow appWindow;
  appWindow.run (L"appWindow", 1280, 720, argv[1], filename);
  }
//}}}
