// main.cpp = 320000  using libfaad randomly broke
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"
#include "decoder_test.h"

#include "../common/winAudio.h"
#pragma comment(lib,"libfaad.lib")
//}}}
extern void decodeMain();

int vidPlayFrame = 0;
int lastVidFrame = -1;
ID2D1Bitmap* mD2D1Bitmap = NULL;
int vidFramesLoaded = 0;
IWICBitmap* vidFrames [100];

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() {}
  ~cAppWindow() {}

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

    if (mD2D1Bitmap)
      dc->DrawBitmap (mD2D1Bitmap, D2D1::RectF(0.0f,0.0f,getClientF().width, getClientF().height));
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
  void player() {

    decodeMain();
    }
  //}}}
  };

cAppWindow* appWindow = NULL;

//{{{
int wmain (int argc, wchar_t* argv[]) {

  printf ("Player %d\n", argc);

  cAppWindow appWindow;
  appWindow.run (L"appWindow", 480, 272);
  }
//}}}
