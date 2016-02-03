// main.cpp = 320000  using libfaad randomly broke
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "../common/winAudio.h"
#pragma comment(lib,"libfaad.lib")
//}}}
#include "decoder_test.h"

//  YUV400     =  0,     //!< Monochrome
//  YUV420     =  1,     //!< 4:2:0
//  YUV422     =  2,     //!< 4:2:2
//  YUV444     =  3      //!< 4:4:4

#define maxFrame 300
static char filename[100];

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() :  mD2D1Bitmap(nullptr), mCurVidFrame(0) {
    for (auto i = 0; i < maxFrame; i++)
      vidFrames[i] = nullptr;
    }
  ~cAppWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height) {

    // init window
    initialise (title, width, height);

    // launch playerThread, higher priority
    auto playerThread = std::thread ([=]() { player(); });
    //SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

    // loop in windows message pump till quit
    messagePump();
    };
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
  void onDraw(ID2D1DeviceContext* dc) {

  IWICBitmap* vidFrame = vidFrames[mCurVidFrame];
    if (vidFrame) {
      // convert to mD2D1Bitmap 32bit BGRA
      IWICFormatConverter* wicFormatConverter;
      getWicImagingFactory()->CreateFormatConverter(&wicFormatConverter);
      wicFormatConverter->Initialize(vidFrame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
      if (mD2D1Bitmap)
        mD2D1Bitmap->Release();

      if (getDeviceContext())
        getDeviceContext()->CreateBitmapFromWicBitmap(wicFormatConverter, NULL, &mD2D1Bitmap);

      dc->DrawBitmap(mD2D1Bitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
      }
    else
      dc->Clear (D2D1::ColorF(D2D1::ColorF::Black));

    D2D1_RECT_F rt = D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height);
    D2D1_RECT_F offset = D2D1::RectF(2.0f, 2.0f, getClientF().width, getClientF().height);
    D2D1_RECT_F r = D2D1::RectF((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
    dc->FillRectangle (r, getGreyBrush());

    wchar_t wStr[200];
    swprintf (wStr, 200, L"helloColin");
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), rt, getWhiteBrush());
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
  void makeVidFrame (int frameIndex, BYTE* yptr, BYTE* uptr, BYTE* vptr, int width, int height) {

    // create vidFrame wicBitmap 24bit BGR
    int pitch = width;
    if (width % 32)
      pitch = ((width + 31) / 32) * 32;
    int pad = pitch - width;

    getWicImagingFactory()->CreateBitmap (pitch, height, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand, &vidFrames[frameIndex]);

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

      if (!(y & 2)) {
        uptr -= width/2;
        vptr -= width/2;
        }
      }
    // release vidFrame wicBitmap buffer
    wicBitmapLock->Release();
    mCurVidFrame = frameIndex;
    }
  //}}}
  //{{{
  void player() {

    openDecode (filename);

    for (int frame = 0; frame < maxFrame; frame++) {
      extDecodedPicList* pic = (extDecodedPicList*)decodeFrame();

      int iWidth = pic->iWidth*((pic->iBitDepth+7)>>3);
      int iHeight = pic->iHeight;
      int iStride = pic->iYBufStride;

          //printf ("frame %d %d- %d %d %d\n", frame, pic->iYUVFormat, iWidth, iHeight, iStride);
      if (iWidth && iHeight) {
        makeVidFrame (frame, pic->pY, pic->pU, pic->pV, iWidth, iHeight);
        changed();
        }
      }
    closeDecode();
    }
  //}}}

  int mCurVidFrame;
  ID2D1Bitmap* mD2D1Bitmap;
  IWICBitmap* vidFrames[maxFrame];
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
