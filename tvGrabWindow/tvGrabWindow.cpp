// tvGrabWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cBda.h"
#include "../common/cTransportStream.h"
//}}}

class cTvGrabWindow : public cD2dWindow {
public:
  cTvGrabWindow() {}
  virtual ~cTvGrabWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* arg) {

    initialise (title, width, height);

    auto freq = arg ? _wtoi (arg) : 674000; // default 650000 674000 706000

    if (freq) {
      SetPriorityClass (GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
      auto bdaReaderThread = std::thread ([=]() { bdaReader (freq); });
      SetThreadPriority (bdaReaderThread.native_handle(), THREAD_PRIORITY_HIGHEST);
      bdaReaderThread.detach();
      }
    else
      thread ([=]() { fileReader (arg); }).detach();

    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true;

      case 0x20 : break;

      case 0x21 :  break;
      case 0x22 :  break;

      case 0x23 : break; // home
      case 0x24 : break; // end

      case 0x25 : break;
      case 0x27 : break;

      case 0x26 : break;
      case 0x28 : break;

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
      case 0x39 : break;

      default   : printf ("key %x\n", key);
      }
    return false;
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    auto showChannel = mShowChannel;
    mShowChannel = inClient && (x < 100);
    if (showChannel != mShowChannel)
      changed();
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->Clear (ColorF(ColorF::Black));

    wchar_t wStr[200];
    swprintf (wStr, 200, L"%s %4.3fm dis:%d p:%4.3fm",
              mFileName, mFilePtr / 1000000.0, mTs.getDiscontinuity(), mTs.getPackets()/1000000.0);
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                  RectF(0, 0, getClientF().width, getClientF().height), getWhiteBrush());

    if (mShowChannel)
      mTs.drawPids (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
    }
  //}}}

private:
  //{{{
  void fileReader (wchar_t* wFileName) {

    mFileName = wFileName;

    uint8_t tsBuf[240*188];

    auto readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    mFilePtr = 0;
    DWORD numberOfBytesRead = 0;
    while (ReadFile (readFile, tsBuf, 240*188, &numberOfBytesRead, NULL)) {
      if (numberOfBytesRead) {
        mFilePtr += numberOfBytesRead;
        mTs.demux (tsBuf, tsBuf + (240*188), false);
        }
      else
        break;
      }

    CloseHandle (readFile);
    }
  //}}}
  //{{{
  void bdaReader (int freq) {

    CoInitialize (NULL);

    mFileName = L"Live Bda";

    cBda bda (128*240*188);
    bda.createGraph (freq);
    printf ("tvGrab %d\n", freq);

    wchar_t fileName[100];
    swprintf (fileName, 100, L"C:/Users/colin/Desktop/%d.ts", freq);
    HANDLE file = CreateFile (fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

    auto blockLen = 0;
    while (true) {
      auto ptr = bda.getContiguousBlock (blockLen);

      if (blockLen)
        mTs.demux (ptr, ptr + blockLen, false);

      if (blockLen) {
        DWORD numberOfBytesWritten;
        WriteFile (file, ptr, blockLen, &numberOfBytesWritten, NULL);
        if (numberOfBytesWritten != blockLen)
          printf ("writefile error%d %d\n", blockLen, numberOfBytesWritten);

        bda.decommitBlock (blockLen);
        }

      else
        Sleep (1);
      }

    CoUninitialize();
    }
  //}}}
  //{{{  vars
  bool mShowChannel = false;
  wchar_t* mFileName = nullptr;
  int mFilePtr = 0;
  cTransportStream mTs;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  #ifndef _DEBUG
    //FreeConsole();
  #endif

  cTvGrabWindow tvGrabWindow;
  tvGrabWindow.run (L"tvGrabWindow", 896, 504, argv[1]);
  }
//}}}
