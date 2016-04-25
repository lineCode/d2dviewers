// cMp3Window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"
#include "../common/cAudio.h"

#include "../../shared/widgets/cWidget.h"
#include "../../shared/widgets/cContainer.h"
#include "../../shared/widgets/cRootContainer.h"
#include "../../shared/widgets/cListWidget.h"
#include "../../shared/widgets/cWaveWidget.h"
#include "../../shared/widgets/cWaveformWidget.h"
#include "../../shared/widgets/cTextBox.h"
#include "../../shared/widgets/cValueBox.h"

#include "../../shared/decoders/cMp3Decoder.h"
//}}}

class cMp3Window : public iDraw, public cAudio, public cD2dWindow {
public:
  //{{{
  void run (wchar_t* title, int width, int height, char* fileName) {

    initialise (title, width+10, height+6);

    if (GetFileAttributesA (fileName) & FILE_ATTRIBUTE_DIRECTORY) {
      //std::thread ([=]() { listThread (fileName ? fileName : "D:/music"); } ).detach();
      listThread (fileName ? fileName : "D:/music");
      }
    else
      mMp3Files.push_back (fileName);

    mSamples = (int16_t*)malloc (1152*2*2);
    memset (mSamples, 0, 1152*2*2);
    mSilence = (int16_t*)malloc (1152*2*2);
    memset (mSilence, 0, 1152*2*2);
    std::thread([=]() { audioThread(); }).detach();

    getDeviceContext()->CreateSolidColorBrush (ColorF(ColorF::CornflowerBlue), &mBrush);

    mRoot = new cRootContainer (width, height);
    std::thread([=]() { loadThread (fileName ? fileName : "D:/music"); }).detach();
    setChangeRate (1);

    messagePump();
    };
  //}}}
  //{{{
  void listDirectory (std::string parentName, std::string directoryName, char* pathMatchName) {

    std::string mFullDirName = parentName.empty() ? directoryName : parentName + "/" + directoryName;

    std::string searchStr (mFullDirName +  "/*");
    WIN32_FIND_DATAA findFileData;
    auto file = FindFirstFileExA (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                  FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (file != INVALID_HANDLE_VALUE) {
      do {
        if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (findFileData.cFileName[0] != '.'))
          listDirectory (mFullDirName, findFileData.cFileName, pathMatchName);
        else if (PathMatchSpecA (findFileData.cFileName, pathMatchName))
          mMp3Files.push_back (mFullDirName + "/" + findFileData.cFileName);
        } while (FindNextFileA (file, &findFileData));

      FindClose (file);
      }
    }
  //}}}

  // iDraw
  //{{{  iDraw utils
  uint16_t getWidth() { return mRoot->getWidth(); }
  uint16_t getHeight() { return mRoot->getHeight(); }
  uint8_t getFontHeight() { return 16; }
  uint8_t getLineHeight() { return 19; }

  //{{{
  void pixelClipped (uint32_t colour, int16_t x, int16_t y) {

      rectClipped (colour, x, y, 1, 1);
    }
  //}}}
  //{{{
  void stampClipped (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    if (!width || !height || x < 0)
      return;

    if (y < 0) {
      // top clip
      if (y + height <= 0)
        return;
      height += y;
      src += -y * width;
      y = 0;
      }

    if (y + height > getHeight()) {
      // bottom yclip
      if (y >= getHeight())
        return;
      height = getHeight() - y;
      }

    stamp (colour, src, x, y, width, height);
    }
  //}}}
  //{{{
  void rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    if (x >= getWidth())
      return;
    if (y >= getHeight())
      return;

    int xend = x + width;
    if (xend <= 0)
      return;

    int yend = y + height;
    if (yend <= 0)
      return;

    if (x < 0)
      x = 0;
    if (xend > getWidth())
      xend = getWidth();

    if (y < 0)
      y = 0;
    if (yend > getHeight())
      yend = getHeight();

    if (!width)
      return;
    if (!height)
      return;

    rect (colour, x, y, xend - x, yend - y);
    }
  //}}}
  //{{{
  void rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    rectClipped (colour, x, y, width, 1);
    rectClipped (colour, x + width, y, 1, height);
    rectClipped (colour, x, y + height, width, 1);
    rectClipped (colour, x, y, 1, height);
    }
  //}}}
  //{{{
  void clear (uint32_t colour) {

    rect (colour, 0, 0, getWidth(), getHeight());
    }
  //}}}

  //{{{
  void ellipse (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

    if (!xradius)
      return;
    if (!yradius)
      return;

    int x1 = 0;
    int y1 = -yradius;
    int err = 2 - 2*xradius;
    float k = (float)yradius / xradius;

    do {
      rectClipped (colour, (x-(uint16_t)(x1 / k)), y + y1, (2*(uint16_t)(x1 / k) + 1), 1);
      rectClipped (colour, (x-(uint16_t)(x1 / k)), y - y1, (2*(uint16_t)(x1 / k) + 1), 1);

      int e2 = err;
      if (e2 <= x1) {
        err += ++x1 * 2 + 1;
        if (-y1 == x && e2 <= y1)
          e2 = 0;
        }
      if (e2 > y1)
        err += ++y1*2 + 1;
      } while (y1 <= 0);
    }
  //}}}
  //{{{
  void ellipseOutline (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

    if (xradius && yradius) {
      int x1 = 0;
      int y1 = -yradius;
      int err = 2 - 2*xradius;
      float k = (float)yradius / xradius;

      do {
        rectClipped (colour, x - (uint16_t)(x1 / k), y + y1, 1, 1);
        rectClipped (colour, x + (uint16_t)(x1 / k), y + y1, 1, 1);
        rectClipped (colour, x + (uint16_t)(x1 / k), y - y1, 1, 1);
        rectClipped (colour, x - (uint16_t)(x1 / k), y - y1, 1, 1);

        int e2 = err;
        if (e2 <= x1) {
          err += ++x1*2 + 1;
          if (-y1 == x1 && e2 <= y1)
            e2 = 0;
          }
        if (e2 > y1)
          err += ++y1*2 + 1;
        } while (y1 <= 0);
      }
    }
  //}}}

  #define ABS(X) ((X) > 0 ? (X) : -(X))
  //{{{
  void line (uint32_t colour, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {

    int16_t deltax = ABS(x2 - x1);        /* The difference between the x's */
    int16_t deltay = ABS(y2 - y1);        /* The difference between the y's */
    int16_t x = x1;                       /* Start x off at the first pixel */
    int16_t y = y1;                       /* Start y off at the first pixel */

    int16_t xinc1;
    int16_t xinc2;
    if (x2 >= x1) {               /* The x-values are increasing */
      xinc1 = 1;
      xinc2 = 1;
      }
    else {                         /* The x-values are decreasing */
      xinc1 = -1;
      xinc2 = -1;
      }

    int yinc1;
    int yinc2;
    if (y2 >= y1) {                 /* The y-values are increasing */
      yinc1 = 1;
      yinc2 = 1;
      }
    else {                         /* The y-values are decreasing */
      yinc1 = -1;
      yinc2 = -1;
      }

    int den = 0;
    int num = 0;
    int num_add = 0;
    int num_pixels = 0;
    if (deltax >= deltay) {        /* There is at least one x-value for every y-value */
      xinc1 = 0;                  /* Don't change the x when numerator >= denominator */
      yinc2 = 0;                  /* Don't change the y for every iteration */
      den = deltax;
      num = deltax / 2;
      num_add = deltay;
      num_pixels = deltax;         /* There are more x-values than y-values */
      }
    else {                         /* There is at least one y-value for every x-value */
      xinc2 = 0;                  /* Don't change the x for every iteration */
      yinc1 = 0;                  /* Don't change the y when numerator >= denominator */
      den = deltay;
      num = deltay / 2;
      num_add = deltax;
      num_pixels = deltay;         /* There are more y-values than x-values */
    }

    for (int curpixel = 0; curpixel <= num_pixels; curpixel++) {
      rectClipped (colour, x, y, 1, 1);   /* Draw the current pixel */
      num += num_add;                            /* Increase the numerator by the top of the fraction */
      if (num >= den) {                          /* Check if numerator >= denominator */
        num -= den;                             /* Calculate the new numerator value */
        x += xinc1;                             /* Change the x as appropriate */
        y += yinc1;                             /* Change the y as appropriate */
        }
      x += xinc2;                               /* Change the x as appropriate */
      y += yinc2;                               /* Change the y as appropriate */
      }
    }
  //}}}
  //}}}
  //{{{
  int text (uint32_t colour, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    // create layout
    std::wstring wstr = converter.from_bytes (str.data());
    IDWriteTextLayout* textLayout;
    getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), width, height, &textLayout);

    // draw it
    mBrush->SetColor (ColorF(((colour & 0x00FF0000) >> 16) / 255.0f, ((colour & 0x0000FF00) >> 8) / 255.0f,
                              (colour & 0x000000FF) / 255.0f,        ((colour & 0xFF000000) >> 24) / 255.0f));
    getDeviceContext()->DrawTextLayout (Point2F(float(x), float(y)), textLayout, mBrush);

    // return measure
    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics (&metrics);
    return (int)metrics.width;
    }
  //}}}
  //{{{
  void pixel (uint32_t colour, int16_t x, int16_t y) {
    rect (colour, x, y, 1, 1);
    }
  //}}}
  //{{{
  void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    rect (0xC0000000 | (colour & 0xFFFFFF), x,y, width, height);
    }
  //}}}
  //{{{
  void rect (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    mBrush->SetColor (ColorF(((colour & 0x00FF0000) >> 16) / 255.0f, ((colour & 0x0000FF00) >> 8) / 255.0f,
                              (colour & 0x000000FF) / 255.0f,        ((colour & 0xFF000000) >> 24) / 255.0f));
    getDeviceContext()->FillRectangle (RectF (float(x), float(y), float(x+width), float(y + height)), mBrush);
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x10: // shift
      case 0x11: // control
      case 0x00: return false;
      case 0x1B: return true; // escape
      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  void onMouseDown (bool right, int x, int y) { mRoot->press (0, x, y, 0,  0, 0); }
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) { mRoot->press (1, x, y, 0, xInc, yInc); }
  void onMouseUp (bool right, bool mouseMoved, int x, int y) { mRoot->release(); }
  void onDraw (ID2D1DeviceContext* dc) { mRoot->draw (this); }

private:
  //{{{
  void listThread (char* fileName) {

    auto time = getTimer();
    if (GetFileAttributesA (fileName) & FILE_ATTRIBUTE_DIRECTORY)
      listDirectory (std::string(), fileName, "*.mp3");

    printf ("files listed %s %d took %f\n", fileName, (int)mMp3Files.size(), getTimer() - time);
    }
  //}}}
  //{{{
  void loadThread (char* fileName) {

    //{{{  create filelist widget
    int fileIndex = 0;
    bool fileIndexChanged;
    mRoot->addTopLeft (new cListWidget (mMp3Files, fileIndex, fileIndexChanged, mRoot->getWidth(), mRoot->getHeight()));
    //}}}
    //{{{  create volume widget
    bool mVolumeChanged;
    mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, cWidget::getBoxHeight()-1, mRoot->getHeight()-6));
    //}}}
    //{{{  create position widget
    float position = 0.0f;
    bool positionChanged = false;
    mRoot->addBottomLeft (new cValueBox (position, positionChanged, COL_BLUE, mRoot->getWidth(), 8));
    //}}}
    int mPlayFrame = 0;
    auto mWaveform = (uint8_t*)malloc (40*60*60*2);
    mRoot->addTopLeft (new cWaveWidget (mPlayFrame, mWaveform, mRoot->getWidth(), 30));
    mRoot->addBottomLeft (new cWaveformWidget (mPlayFrame, mWaveform, mRoot->getWidth(), 30));

    while (fileIndex < mMp3Files.size()) {
      auto fileHandle = CreateFileA (mMp3Files[fileIndex].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      auto fileSize = (int)GetFileSize (fileHandle, NULL);
      auto fileBuffer = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);

      mPlayFrame = 0;
      auto playPtr = 0;
      while (playPtr < fileSize) {
        mWait = true;
        mReady = false;
        float waveformSample[2];
        int bytesUsed = mMp3Decoder.decodeNextFrame (fileBuffer + playPtr, fileSize - playPtr, waveformSample, mSamples);
        mReady = true;
        if (bytesUsed > 0) {
          while (mWait)
            Sleep (2);
          playPtr = playPtr + bytesUsed;

          mWaveform[mPlayFrame * 2] = (uint8_t)waveformSample[0];
          mWaveform[(mPlayFrame * 2) + 1] = (uint8_t)waveformSample[1];
          mPlayFrame++;
          }
        else
          break;

        if (fileIndexChanged)
          break;
        else if (positionChanged) {
          //{{{  skip
          playPtr = int(position * fileSize);
          positionChanged = false;
          }
          //}}}
        else
          position = (float)playPtr / (float)fileSize;
        }

      if (!fileIndexChanged)
        fileIndex++;
      fileIndexChanged = false;

      UnmapViewOfFile (fileBuffer);
      CloseHandle (fileHandle);
      }
    }
  //}}}
  //{{{
  void audioThread() {

    CoInitialize (NULL);
    audOpen (44100, 16, 2);

    while (true) {
      audPlay (mReady ? mSamples : mSilence, 1152*2*2, 1.0f);
      mWait = false;
      }

    CoUninitialize();
    }
  //}}}
  //{{{  vars
  cRootContainer* mRoot = nullptr;

  std::vector <std::string> mMp3Files;
  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  cMp3Decoder mMp3Decoder;

  int16_t* mSamples = nullptr;
  int16_t* mSilence = nullptr;
  bool mWait = false;
  bool mReady = false;

  ID2D1SolidColorBrush* mBrush;
  //}}}
  };

//{{{
int main (int argc, char* argv[]) {
  startTimer();
  cMp3Window mp3Window;
  mp3Window.run (L"mp3window", 480*2, 272*2, argv[1]);
  }
//}}}
