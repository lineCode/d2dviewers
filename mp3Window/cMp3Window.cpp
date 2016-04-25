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
    mRoot = new cRootContainer (width, height);

    auto loaderThread = std::thread([=]() { loadThread(fileName ? fileName : "D:/music"); });
    //auto loaderThread = std::thread([=]() { loadThread(fileName ? fileName : "D:/music/_singles"); });
    SetThreadPriority (loaderThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    loaderThread.detach();

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
  //{{{  iDraw gets
  uint16_t getWidth() { return 480; }
  uint16_t getHeight() { return 272; }
  uint8_t getFontHeight() { return 16; }
  uint8_t getLineHeight() { return 19; }
  //}}}
  //{{{
  int text (uint32_t colour, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    // create layout
    std::wstring wstr = converter.from_bytes (str.data());
    IDWriteTextLayout* textLayout;
    getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), width, height, &textLayout);

    // draw it
    ID2D1SolidColorBrush* brush;
    getDeviceContext()->CreateSolidColorBrush (ColorF (((colour & 0x00FF0000) >> 16) / 255.0f,
                                                       ((colour & 0x0000FF00)>> 8) / 255.0f,
                                                        (colour & 0x000000FF) / 255.0f,
                                                       ((colour & 0xFF000000) >> 24) / 255.0f), &brush);
    getDeviceContext()->DrawTextLayout (Point2F(float(x), float(y)), textLayout, brush);

    // return measure
    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics (&metrics);
    return (int)metrics.width;
    }
  //}}}
  //{{{
  int measure (int fontHeight, std::string str) {
    return 400;
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
    ID2D1SolidColorBrush* brush;
    getDeviceContext()->CreateSolidColorBrush (ColorF (((colour & 0x00FF0000) >> 16) / 255.0f,
                                                       ((colour & 0x0000FF00)>> 8) / 255.0f,
                                                        (colour & 0x000000FF) / 255.0f,
                                                       ((colour & 0xFF000000) >> 24) / 255.0f), &brush);
    getDeviceContext()->FillRectangle (RectF (float(x), float(y), float(x+width), float(y + height)), brush);
    }
  //}}}
  //{{{  iDraw utils
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

protected:
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

    CoInitialize (NULL);

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
    //{{{  create waveform widget
    int mPlayFrame = 0;
    auto mWaveform = (float*)malloc (480*2*4);
    mRoot->addTopLeft (new cWaveformWidget (mPlayFrame, mWaveform, mRoot->getWidth(), mRoot->getHeight()));
    //}}}

    if (GetFileAttributesA (fileName) & FILE_ATTRIBUTE_DIRECTORY) {
      std::thread ([=]() { listThread (fileName); } ).detach();
      }
    else
      mMp3Files.push_back (fileName);
    audOpen (44100, 16, 2);

    cMp3Decoder mMp3Decoder;
    auto samples = (int16_t*)malloc (1152*2*2);
    memset (samples, 0, 1152*2*2);

    while (fileIndex < mMp3Files.size()) {
      memset (mWaveform, 0, 480*2*4);
      auto fileHandle = CreateFileA (mMp3Files[fileIndex].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      auto fileSize = (int)GetFileSize (fileHandle, NULL);
      auto fileBuffer = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);

      auto playPtr = 0;
      while (playPtr < fileSize) {
        int bytesUsed = mMp3Decoder.decodeNextFrame (fileBuffer + playPtr, fileSize - playPtr,  &mWaveform[(mPlayFrame % 480) * 2], samples);
        if (bytesUsed > 0) {
          audPlay (samples, 1152*2*2, 1.0f);
          playPtr = playPtr + bytesUsed;
          mPlayFrame++;
          //printf ("%d %f %f %f\n", skipped, playPtr, mPlaySecs, mMp3File->getMaxSecs());
          changed();
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

    CoUninitialize();
    }
  //}}}
  //{{{  vars
  cRootContainer* mRoot = nullptr;

  std::vector <std::string> mMp3Files;

  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;
  //}}}
  };

//{{{
int main (int argc, char* argv[]) {
  startTimer();
  cMp3Window mp3Window;
  mp3Window.run (L"mp3window", 480, 272, argv[1]);
  }
//}}}
