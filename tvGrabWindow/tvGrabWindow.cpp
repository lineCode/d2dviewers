// tvGrabWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"
#include "../common/timer.h"

#include "../common/cTsSection.h"
#include "../common/cBda.h"
//}}}

class cTvGrabWindow : public cD2dWindow {
public:
  cTvGrabWindow() {}
  virtual ~cTvGrabWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* arg) {

    initialise (title, width, height);

    int freq = arg ? _wtoi (arg) : 674000; // 650000 674000 706000

    if (freq)
      bdaReader (freq);
    else
      tsFileReader (arg);

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
    swprintf(wStr, 200, L"nnnnnnnn %d", mFilePtr);
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                  RectF(0, 0, getClientF().width, getClientF().height), getWhiteBrush());

    mTsSection.renderPidInfo  (dc, getClientF(), getTextFormat(), getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
    }
  //}}}

private:
  //{{{
  void tsFileReader (wchar_t* wFileName) {

    uint8_t tsBuf[256*188];

    HANDLE readFile = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    mFilePtr = 0;
    DWORD numberOfBytesRead = 0;
    while (ReadFile (readFile, tsBuf, 256*188, &numberOfBytesRead, NULL)) {
      if (numberOfBytesRead) {
        mFilePtr += numberOfBytesRead;
        mTsSection.tsParser (tsBuf, tsBuf + (256*188));
        }
      else
        break;
      }

    CloseHandle (readFile);
    }
  //}}}
  //{{{
  void bdaReader (int freq) {

    cBda bda (128*240*188);
    bda.createGraph (freq);
    printf ("tvGrab %d\n", freq);

    wchar_t fileName[100];
    swprintf (fileName, 100, L"C:/Users/colin/Desktop/%d.ts", freq);
    HANDLE file = CreateFile (fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

    int blockLen = 0;
    while (true) {
      uint8_t* ptr = bda.getContiguousBlock (blockLen);
      if (blockLen) {
        DWORD numberOfBytesWritten;
        WriteFile (file, ptr, blockLen, &numberOfBytesWritten, NULL);
        if (numberOfBytesWritten != blockLen)
          printf ("writefile error%d %d\n", blockLen, numberOfBytesWritten);

        mTsSection.tsParser (ptr, ptr + blockLen);
        bda.decommitBlock (blockLen);
        }

      else
        Sleep (1);
      }
    }
  //}}}

  int mFilePtr = 0;
  cTsSection mTsSection;
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  //SetPriorityClass (GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

  #ifndef _DEBUG
    //FreeConsole();
  #endif

  cTvGrabWindow tvGrabWindow;
  tvGrabWindow.run (L"tvGrabWindow", 896, 504, argv[1]);

  CoUninitialize();
  }
//}}}
