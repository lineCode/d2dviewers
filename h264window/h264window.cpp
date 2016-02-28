// h264window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"
#include "../common/cYuvFrame.h"
#include "../common/cMpeg2decoder.h"

#include "ldecod.h"

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#pragma comment(lib,"welsdec.lib")
//}}}

#define maxVidFrames 10000

class cAppWindow : public cD2dWindow, public cMpeg2decoder {
public:
  cAppWindow() {}
  ~cAppWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFilename, char* filename) {

    initialise (title, width, height);

    // launch playerThread, higher priority
    std::thread ([=]() { playerMpeg2 (wFilename, filename); }).detach();
    //std::thread ([=]() { playerReference264(); }).detach();
    //std::thread ([=]() { playerOpenH264(); }).detach();

    // loop in windows message pump till quit
    messagePump();
    };
  //}}}
  //{{{
  void writeFrame (unsigned char* src[], int frame, bool progressive, int width, int height, int chromaWidth) {

    int32_t linesize[2];
    linesize[0] = width;
    linesize[1] = chromaWidth;
    mYuvFrames [frame+1].set (0, src, linesize,  width, height, 0, 0);
    mCurVidFrame++;
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true;

      //case 0x20 : stopped = !stopped; break;

      //case 0x21 : playFrame -= 5.0f * audFramesPerSec; changed(); break;
      //case 0x22 : playFrame += 5.0f * audFramesPerSec; changed(); break;

      //case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
      //case 0x24 : playFrame = 0; break;

      //case 0x25 : playFrame -= 1.0f * audFramesPerSec; changed(); break;
      //case 0x27 : playFrame += 1.0f * audFramesPerSec; changed(); break;

      //case 0x26 : stopped = true; playFrame -=2; changed(); break;
      //case 0x28 : stopped = true; playFrame +=2; changed(); break;

      //case 0x2d : markFrame = playFrame - 2.0f; changed(); break;
      //case 0x2e : playFrame = markFrame; changed(); break;

      default   : printf ("key %x\n", key);
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

    makeBitmap (&mYuvFrames[mCurVidFrame]);
    if (mBitmap)
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
  void makeBitmap (cYuvFrame* yuvFrame) {

    static const D2D1_BITMAP_PROPERTIES props = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 96.0f, 96.0f };

    if (yuvFrame && yuvFrame->mWidth && yuvFrame->mHeight) {
      if (mBitmap)  {
        auto pixelSize = mBitmap->GetPixelSize();
        if ((pixelSize.width != yuvFrame->mWidth) || (pixelSize.height != yuvFrame->mHeight)) {
          mBitmap->Release();
          mBitmap = nullptr;
          }
        }
      if (!mBitmap) // create bitmap
        getDeviceContext()->CreateBitmap (SizeU(yuvFrame->mWidth, yuvFrame->mHeight), props, &mBitmap);

      auto bgraBuf = yuvFrame->bgra();
      mBitmap->CopyFromMemory (&RectU(0, 0, yuvFrame->mWidth, yuvFrame->mHeight), bgraBuf, yuvFrame->mWidth * 4);
      _mm_free (bgraBuf);
      }
    else if (mBitmap) {
      mBitmap->Release();
      mBitmap = nullptr;
      }
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
  void playerReference264 (char* filename) {

    OpenDecoder (filename);

    int frame = 0;
    int iRet;
    do  {
      iRet = DecodeOneFrame (&pDecPicList);
      int iWidth = pDecPicList->iWidth*((pDecPicList->iBitDepth+7)>>3);
      int iHeight = pDecPicList->iHeight;
      int iStride = pDecPicList->iYBufStride;
      if (iWidth && iHeight) {
        //makeVidFrame (frame++, pDecPicList->pY, pDecPicList->pU, pDecPicList->pV, iWidth, iHeight, iWidth, iWidth/2);
        changed();
        }
      } while (iRet == DEC_EOS || iRet == DEC_SUCCEED);

    FinitDecoder (&pDecPicList);
    CloseDecoder();
    }
  //}}}
  //{{{
  void playerOpenH264 (char* filename) {

    //{{{  init decoder
    ISVCDecoder* pDecoder = NULL;
    WelsCreateDecoder (&pDecoder);

    int iLevelSetting = (int)WELS_LOG_WARNING;
    pDecoder->SetOption (DECODER_OPTION_TRACE_LEVEL, &iLevelSetting);

    // init decoder params
    SDecodingParam sDecParam = {0};
    sDecParam.sVideoProperty.size = sizeof (sDecParam.sVideoProperty);
    sDecParam.uiTargetDqLayer = (uint8_t) - 1;
    sDecParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    pDecoder->Initialize (&sDecParam);
    //}}}
    //{{{  open file, find size, read file and append startCode
    FILE* pH264File = fopen (filename, "rb");
    fseek (pH264File, 0L, SEEK_END);
    int32_t iFileSize = (int32_t) ftell (pH264File);
    fseek (pH264File, 0L, SEEK_SET);

    // read file
    uint8_t* pBuf = new uint8_t[iFileSize + 4];
    fread (pBuf, 1, iFileSize, pH264File);
    fclose (pH264File);

    // append slice startCode
    uint8_t uiStartCode[4] = {0, 0, 0, 1};
    memcpy (pBuf + iFileSize, &uiStartCode[0], 4);
    //}}}
    //{{{  set conceal option
    int32_t iErrorConMethod = (int32_t) ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
    pDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);
    //}}}

    int32_t iWidth = 0;
    int32_t iHeight = 0;
    int frameIndex = 0;
    unsigned long long uiTimeStamp = 0;
    int32_t iBufPos = 0;
    while (true) {
      // process slice
      int32_t iEndOfStreamFlag = 0;
      if (iBufPos >= iFileSize) {
        //{{{  end of stream
        iEndOfStreamFlag = true;
        pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
        break;
        }
        //}}}

      // find sliceSize by looking for next slice startCode
      int32_t iSliceSize;
      for (iSliceSize = 1; iSliceSize < iFileSize; iSliceSize++)
        if ((!pBuf[iBufPos+iSliceSize] && !pBuf[iBufPos+iSliceSize+1])  &&
            ((!pBuf[iBufPos+iSliceSize+2] && pBuf[iBufPos+iSliceSize+3] == 1) || (pBuf[iBufPos+iSliceSize+2] == 1)))
          break;

      uint8_t* pData[3]; // yuv ptrs
      SBufferInfo sDstBufInfo;
      memset (&sDstBufInfo, 0, sizeof (SBufferInfo));

      sDstBufInfo.uiInBsTimeStamp = uiTimeStamp++;
      //pDecoder->DecodeFrame2 (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);
      pDecoder->DecodeFrameNoDelay (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);

      if (sDstBufInfo.iBufferStatus == 1) {
        //makeVidFrame (frameIndex++, pData[0], pData[1], pData[2], 640, 360,
        //              sDstBufInfo.UsrData.sSystemBuffer.iStride[0], sDstBufInfo.UsrData.sSystemBuffer.iStride[1]);
        iWidth = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
        iHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
        }
      iBufPos += iSliceSize;
      }

    delete[] pBuf;
    pDecoder->Uninitialize();
    WelsDestroyDecoder (pDecoder);
    }
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
    for (int i = 0; i < maxVidFrames; i++) {
      if (!(i % 100))
        printf ("frame %d  %4.3fs %4.3fs\n", i, getTimer() - time, (getTimer() - time) / i);
      decodePes (pesPtr, fileBuffer + mFileBytes, &mYuvFrames[i], pesPtr);
      mCurVidFrame = i;
      }

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}

  int mCurVidFrame = 0;
  cYuvFrame mYuvFrames[maxVidFrames];
  DecodedPicList* pDecPicList;
  ID2D1Bitmap* mBitmap = nullptr;
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  char filename[100];
  strcpy (filename, "C:\\Users\\colin\\Desktop\\d2dviewers\\test.264");
  if (argv[1])
    std::wcstombs (filename, argv[1], wcslen(argv[1])+1);

  printf ("Player %d %s/n", argc, filename);

  cAppWindow appWindow;
  appWindow.run (L"appWindow", 1280, 720, argv[1], filename);
  }
//}}}
