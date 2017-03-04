// tvGrabWindow.cpp
//{{{  includes
#include "pch.h"

#include "../../shared/utils.h"
#include "../../shared/d2dwindow/cD2dWindow.h"

#include "../common/cBda.h"
#include "../../shared/decoders/cTransportStream.h"
//}}}

//{{{
class cDecodeTransportStream : public cTransportStream {
public:
  cDecodeTransportStream() {}
  virtual ~cDecodeTransportStream() {}

  //{{{
  void drawPids (ID2D1DeviceContext* dc, D2D1_SIZE_F client, IDWriteTextFormat* textFormat,
                 ID2D1SolidColorBrush* whiteBrush, ID2D1SolidColorBrush* blueBrush,
                 ID2D1SolidColorBrush* blackBrush, ID2D1SolidColorBrush* greyBrush) {

    if (!mPidInfoMap.empty()) {
      float lineY = 20.0f;
      std::string str = mTimeStr + " services:" + to_string (mServiceMap.size());
      auto textr = D2D1::RectF(0, 20.0f, client.width, client.height);
      dc->DrawText (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), textFormat, textr, whiteBrush);
      lineY+= 16.0f;

      for (auto pidInfo : mPidInfoMap) {
        float total = (float)pidInfo.second.mTotal;
        if (total > mLargest)
          mLargest = total;
        auto len = (total / mLargest) * (client.width - 50.0f);
        dc->FillRectangle (RectF(40.0f, lineY+2.0f, 40.0f + len, lineY+14.0f), blueBrush);

        textr.top = lineY;
        str = to_string (pidInfo.first) +
              " " + to_string (pidInfo.second.mStreamType) +
              " " + pidInfo.second.mInfo +
              " " + to_string (pidInfo.second.mTotal) +
              " d:" + to_string (pidInfo.second.mDisContinuity) +
              " r:" + to_string (pidInfo.second.mRepeatContinuity);
        dc->DrawText (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                      textFormat, textr, whiteBrush);
        lineY += 14.0f;
        }
      }
    }
  //}}}
  //{{{
  void startProgram (int vpid, int apid, std::string name, std::string startTime) {
    printf ("now changed %d %d %s %s\n", vpid, apid, name.c_str(), startTime.c_str());
    }
  //}}}

private:
  float mLargest = 10000.0f;
  };
//}}}

class cTvGrabWindow : public cD2dWindow {
public:
  cTvGrabWindow() {}
  virtual ~cTvGrabWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* arg) {

    initialise (title, width, height);

    getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
                                          DWRITE_FONT_WEIGHT_REGULAR,
                                          DWRITE_FONT_STYLE_NORMAL,
                                          DWRITE_FONT_STRETCH_NORMAL,
                                          12.0f, L"en-us", &mSmallTextFormat);

    auto freq = arg ? _wtoi (arg) : 650000; // default 650000 674000 706000

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
    swprintf (wStr, 200, L"%s %4.3fm dis:%d packets:%4.3fm",
              mFileName, mFilePtr / 1000000.0, mTs.getDiscontinuity(), mTs.getPackets()/1000000.0);
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                  RectF(0, 0, getClientF().width, getClientF().height), getWhiteBrush());

    mTs.drawPids (dc, getClientF(), mSmallTextFormat,
                  getWhiteBrush(), getBlueBrush(), getBlackBrush(), getGreyBrush());
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
        changed();
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
      changed();

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
  IDWriteTextFormat* mSmallTextFormat = nullptr;

  bool mShowChannel = false;
  wchar_t* mFileName = nullptr;
  int mFilePtr = 0;

  cDecodeTransportStream mTs;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  #ifndef _DEBUG
    //FreeConsole();
  #endif

  cTvGrabWindow tvGrabWindow;
  tvGrabWindow.run (L"tvGrabWindow", 800, 960, argv[1]);
  }
//}}}
