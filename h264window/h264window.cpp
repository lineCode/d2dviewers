// h264window.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "../common/cMpeg2dec.h"

#include "ldecod.h"

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#pragma comment(lib,"welsdec.lib")
//}}}

#define maxFrame 30000
static char filename[100];

class cAppWindow : public cD2dWindow, public cMpeg2dec {
public:
  //{{{
  //{{{
  cAppWindow() :  mD2D1Bitmap(nullptr), mCurVidFrame(0) {
    for (auto i = 0; i < maxFrame; i++)
      vidFrames[i] = nullptr;
    }
  //}}}
  ~cAppWindow() {}
  //}}}
  //{{{
  void run (wchar_t* title, int width, int height) {

    // init window
    initialise (title, width, height);

    // launch playerThread, higher priority
    std::thread ([=]() { playerMpeg2(); }).detach();
    //std::thread ([=]() { playerReference264(); }).detach();
    //std::thread ([=]() { playerOpenH264(); }).detach();

    // loop in windows message pump till quit
    messagePump();
    };
  //}}}

  //{{{
  void writeFrame (unsigned char* src[], int frame, bool progressive, int width, int height, int chromaWidth) {

    Sleep (40);
    makeVidFrame (frame, src[0], src[1], src[2], width, height, width, chromaWidth);
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

    auto vidFrame = vidFrames[mCurVidFrame];
    if (vidFrame) {
      // convert to mD2D1Bitmap 32bit BGRA
      IWICFormatConverter* wicFormatConverter;
      getWicImagingFactory()->CreateFormatConverter (&wicFormatConverter);
      wicFormatConverter->Initialize (vidFrame, GUID_WICPixelFormat32bppPBGRA,
                                      WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
      if (mD2D1Bitmap)
        mD2D1Bitmap->Release();

      if (getDeviceContext())
        getDeviceContext()->CreateBitmapFromWicBitmap (wicFormatConverter, NULL, &mD2D1Bitmap);

      dc->DrawBitmap (mD2D1Bitmap, RectF (0.0f, 0.0f, getClientF().width, getClientF().height));
      }
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
  uint8_t limit (double v) {

    if (v <= 0.0)
      return 0;

    if (v >= 255.0)
      return 255;

    return (uint8_t)v;
    }
  //}}}
  //{{{
  void makeVidFrame (int frameIndex, BYTE* ys, BYTE* us, BYTE* vs, int width, int height, int stridey, int strideuv) {

    // create vidFrame wicBitmap 24bit BGR
    int pitch = width;
    //if (width % 32)
    //  pitch = ((width + 31) / 32) * 32;

    getWicImagingFactory()->CreateBitmap (pitch, height, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand,
                                          &vidFrames[frameIndex]);

    // lock vidFrame wicBitmap
    WICRect wicRect = { 0, 0, pitch, height };
    IWICBitmapLock* wicBitmapLock = NULL;
    vidFrames[frameIndex]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);

    // get vidFrame wicBitmap buffer
    UINT bufferLen = 0;
    BYTE* buffer = NULL;
    wicBitmapLock->GetDataPointer (&bufferLen, &buffer);

    // yuv
    for (auto y = 0; y < height; y++) {
      BYTE* yptr = ys + (y*stridey);
      BYTE* uptr = us + ((y/2)*strideuv);
      BYTE* vptr = vs + ((y/2)*strideuv);

      for (auto x = 0; x < width/2; x++) {
        int y1 = *yptr++;
        int y2 = *yptr++;
        int u = (*uptr++) - 128;
        int v = (*vptr++) - 128;

        *buffer++ = limit (y1 + (1.8556 * u));
        *buffer++ = limit (y1 - (0.1873 * u) - (0.4681 * v));
        *buffer++ = limit (y1 + (1.5748 * v));

        *buffer++ = limit (y2 + (1.8556 * u));
        *buffer++ = limit (y2 - (0.1873 * u) - (0.4681 * v));
        *buffer++ = limit (y2 + (1.5748 * v));
        }
      }
    // release vidFrame wicBitmap buffer
    wicBitmapLock->Release();
    mCurVidFrame = frameIndex;
    }
  //}}}
  //{{{
  void playerReference264() {

    OpenDecoder (filename);

    int frame = 0;
    int iRet;
    do  {
      iRet = DecodeOneFrame (&pDecPicList);
      int iWidth = pDecPicList->iWidth*((pDecPicList->iBitDepth+7)>>3);
      int iHeight = pDecPicList->iHeight;
      int iStride = pDecPicList->iYBufStride;
      if (iWidth && iHeight) {
        makeVidFrame (frame++, pDecPicList->pY, pDecPicList->pU, pDecPicList->pV, iWidth, iHeight, iWidth, iWidth/2);
        changed();
        }
      } while (iRet == DEC_EOS || iRet == DEC_SUCCEED);

    FinitDecoder (&pDecPicList);
    CloseDecoder();
    }
  //}}}
  //{{{
  void playerOpenH264() {

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
        makeVidFrame (frameIndex++, pData[0], pData[1], pData[2], 640, 360,
                      sDstBufInfo.UsrData.sSystemBuffer.iStride[0], sDstBufInfo.UsrData.sSystemBuffer.iStride[1]);
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
  void playerMpeg2() {

    initBitstream (filename);
    decodeBitstream();
    }
  //}}}

  int mCurVidFrame;
  ID2D1Bitmap* mD2D1Bitmap;
  IWICBitmap* vidFrames[maxFrame];
  DecodedPicList* pDecPicList;
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  strcpy (filename, "C:\\Users\\colin\\Desktop\\d2dviewers\\test.264");
  if (argv[1])
    std::wcstombs (filename, argv[1], wcslen(argv[1])+1);

  printf ("Player %d %s /n", argc, filename);

  cAppWindow appWindow;
  appWindow.run (L"appWindow", 1280, 720);
  }
//}}}
