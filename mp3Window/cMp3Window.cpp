// cMp3Window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cAudio.h"
#include "../common/cMp3File.h"

#include "../widgets/cWidget.h"
#include "../widgets/cContainer.h"
#include "../widgets/cRootContainer.h"
#include "../widgets/cListWidget.h"
#include "../widgets/cWaveformWidget.h"
#include "../widgets/cTextBox.h"
#include "../widgets/cValueBox.h"
//}}}
//{{{  typedef
class cMp3Window;
typedef void (cMp3Window::*cMp3WindowFunc)();
typedef void (cMp3Window::*cMp3WindowFileFunc)(cMp3File* file);
//}}}
//{{{
class cMp3Files {
public:
  cMp3Files() {}
  virtual ~cMp3Files() {}

  //{{{
  void scan (wstring& parentName, wchar_t* directoryName, wchar_t* pathMatchName, int& numFiles, int& numDirs,
             cMp3Window* mp3Window, cMp3WindowFunc func) {
  // directoryName is findFileData.cFileName wchar_t*

    mDirName = directoryName;
    mFullDirName = parentName.empty() ? directoryName : parentName + L"\\" + directoryName;
    auto searchStr (mFullDirName +  L"\\*");

    WIN32_FIND_DATA findFileData;
    auto file = FindFirstFileEx (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                 FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (file != INVALID_HANDLE_VALUE) {
      do {
        if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (findFileData.cFileName[0] != L'.'))  {
          auto directory = new cMp3Files();
          directory->scan (mFullDirName, findFileData.cFileName, pathMatchName, numFiles, numDirs, mp3Window, func);
          mDirectories.push_back (directory);
         (mp3Window->*func)();
          }
        else if (PathMatchSpec (findFileData.cFileName, pathMatchName)) {
          mFiles.push_back (new cMp3File (mFullDirName, findFileData.cFileName));
          (mp3Window->*func)();
          }
        } while (FindNextFile (file, &findFileData));
      FindClose (file);
      }

    numDirs += (int)mDirectories.size();
    numFiles += (int)mFiles.size();
    }
  //}}}

  //{{{
  cMp3File* pick (D2D1_POINT_2F& point) {

    for (auto directory : mDirectories) {
      auto pickedFile = directory->pick (point);
      if (pickedFile)
        return pickedFile;
      }

    for (auto file : mFiles)
      if (file->pick (point))
        return file;

    return nullptr;
    }
  //}}}

  //{{{
  void traverseFiles (cMp3Window* mp3Window, cMp3WindowFileFunc mp3Func) {

    for (auto directory : mDirectories)
      directory->traverseFiles (mp3Window, mp3Func);

    for (auto file : mFiles)
      (mp3Window->*mp3Func)(file);
    }
  //}}}
  //{{{
  void simpleLayout (D2D1_RECT_F& rect) {

    for (auto directory : mDirectories)
      directory->simpleLayout (rect);

    for (auto file : mFiles) {
      file->setLayout (rect);
      rect.top += 20.0f;
      rect.bottom += 20.0f;
      }
    }
  //}}}
  //{{{
  cMp3File* nextLoadFile() {

    for (auto directory : mDirectories) {
      auto file = directory->nextLoadFile();
      if (file)
        return file;
      }

    for (auto file : mFiles)
      if (!file->isLoaded())
        return file;

    return nullptr;
    }
  //}}}

private:
  // private vars
  wstring mDirName;
  wstring mFullDirName;
  concurrency::concurrent_vector<cMp3File*> mFiles;
  concurrency::concurrent_vector<cMp3Files*> mDirectories;
  };
//}}}
//{{{
class cLcd : public iDraw {
#define ABS(X) ((X) > 0 ? (X) : -(X))
public:
  cLcd() {}
  virtual ~cLcd() {}

  static uint16_t getWidth() { return 480; }
  static uint16_t getHeight() { return 272; }
  static uint8_t getFontHeight() { return 16; }
  static uint8_t getLineHeight() { return 19; }

  //{{{
  int text (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    return 1;
    }
  //}}}
  //{{{
  int measure (int fontHeight, std::string str) {
    return 1;
    }
  //}}}

  //{{{
  void pixel (uint32_t colour, int16_t x, int16_t y) {
    }
  //}}}
  //{{{
  void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    }
  //}}}
  //{{{
  void rect (uint32_t col, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    ID2D1SolidColorBrush* brush;
    mDc->CreateSolidColorBrush (ColorF (((col & 0xFF0000) >> 16) / 255.0f, ((col & 0xFF00)>> 8) / 255.0f, (col & 0xff) / 255.0f, 1.0f), &brush);
    mDc->FillRectangle (RectF (float(x), float(y), float(x+width), float(y + height)), brush);
    }
  //}}}
  //{{{
  void cLcd::pixelClipped (uint32_t colour, int16_t x, int16_t y) {

      rectClipped (colour, x, y, 1, 1);
    }
  //}}}
  //{{{
  void cLcd::stampClipped (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

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
  void cLcd::rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

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
  void cLcd::rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    rectClipped (colour, x, y, width, 1);
    rectClipped (colour, x + width, y, 1, height);
    rectClipped (colour, x, y + height, width, 1);
    rectClipped (colour, x, y, 1, height);
    }
  //}}}
  //{{{
  void cLcd::clear (uint32_t colour) {

    rect (colour, 0, 0, getWidth(), getHeight());
    }
  //}}}
  //{{{
  void cLcd::ellipse (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

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
  void cLcd::ellipseOutline (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

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
  //{{{
  void cLcd::line (uint32_t colour, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {

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

  ID2D1DeviceContext* mDc = nullptr;
  };
//}}}
static cLcd* mLcd = nullptr;

class cMp3Window : public cD2dWindow, public cAudio, public cMp3Files {
public:
  cMp3Window() {}
  ~cMp3Window() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFileName) {

    initialise (title, width+10, height+10);

    mLcd = new cLcd();
    cRootContainer rootContainer (cLcd::getWidth(), cLcd::getHeight());
    auto root = cRootContainer::get();

    bool mVolumeChanged;
    root->addTopRight (new cValueBox (mVolume, mVolumeChanged, LCD_YELLOW, cWidget::getBoxHeight()-1, root->getHeight()-6));

    //mIsDirectory = (GetFileAttributes (wFileName) & FILE_ATTRIBUTE_DIRECTORY) != 0;
    //if (mIsDirectory) {
    //  thread ([=]() { fileScanner (wFileName); }).detach();
    //  thread ([=]() { filesLoader (0); } ).detach();
    //  }
    mMp3File = new cMp3File (wFileName);
    thread ([=]() { mMp3File->load (getDeviceContext(), getBitmapProperties(), false); }).detach();

    thread ([=]() { player(); }).detach();

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

      case 0x20: togglePlaying(); break;

      case 0x21: incPlaySecs (-5); changed(); break;
      case 0x22: incPlaySecs(5); changed(); break;

      case 0x23: setPlaySecs (mMp3File->getMaxSecs()); changed(); break;
      case 0x24: setPlaySecs (0); changed(); break;

      case 0x25: incPlaySecs (-1); changed(); break;
      case 0x27: incPlaySecs (1); changed(); break;

      case 0x26: setPlaying (false); incPlaySecs (-getSecsPerAudFrame()); changed(); break;
      case 0x28: setPlaying (false); incPlaySecs (getSecsPerAudFrame()); break;

      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    mLayoutOffset += delta/6;
    if (mLayoutOffset > 0)
      mLayoutOffset = 0;
    simpleLayout (RectF (0, mLayoutOffset, getClientF().width, mLayoutOffset + 20.0f));

    //auto ratio = controlKeyDown ? 1.5f : shiftKeyDown ? 1.2f : 1.1f;
    //if (delta > 0)
    //  ratio = 1.0f/ratio;

    //setVolume (getVolume() * ratio);

    changed();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

  cMp3File* proxFile = (x < 100) ? pick (Point2F ((float)x, (float)y)) : nullptr;
  if (mProxFile != proxFile) {
    mProxFile = proxFile;
    }
  }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {

    cRootContainer::get()->press (0, x, y, 0,  0, 0);
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    cRootContainer::get()->press (1, x, y, 0, xInc, yInc);
    }
  //}}}
  //{{{
  void onMouseUp  (bool right, bool mouseMoved, int x, int y) {

    cRootContainer::get()->release();
    changed();
    }
  //}}}
  //{{{
  void onDraw1 (ID2D1DeviceContext* dc) {

    dc->Clear (ColorF(ColorF::Black));

    //if (mMp3File && mMp3File->getBitmap())
    //  dc->DrawBitmap (mMp3File->getBitmap(), RectF (0.0f, 0.0f, getClientF().width, getClientF().height));

    // yellow volume bar
    dc->FillRectangle (RectF (getClientF().width-20,0, getClientF().width, getVolume()*0.8f*getClientF().height), getYellowBrush());

    if (mMp3File) {
      auto rows = 6;
      auto frame = int(mPlaySecs / getSecsPerAudFrame());
      for (auto row = 0; row < rows; row++) {
        D2D1_RECT_F r;
        r.left = 0;
        for (auto x = 0; x < getClientF().width; x++) {
          r.right = r.left + 1.0f;
          if (mMp3File->getFrame (frame)) {
            r.top =    ((row + 0.5f) * getClientF().height / rows) - mMp3File->getFrame (frame)->mPower[0];
            r.bottom = ((row + 0.5f) * getClientF().height / rows) + mMp3File->getFrame (frame)->mPower[1];
            dc->FillRectangle (r, getBlueBrush());
            }
          r.left += 1.0f;
          frame++;
          }
        }
      }

    if (mIsDirectory)
      traverseFiles (this, &cMp3Window::drawFileInfo);
    else {
      wstringstream str;
      str << mPlaySecs
          << L" " << mMp3File->getMaxSecs()
          << L" " << mMp3File->getBitRate()
          << L" " << getAudSampleRate()
          << L" " << mMp3File->getChannels()
          << L" " << mMp3File->getMode();
      dc->DrawText (str.str().data(), (uint32_t)str.str().size(), getTextFormat(),
                    RectF(0.0f, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());
      }
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    mLcd->mDc = dc;
    cRootContainer::get()->draw (mLcd);
    changed();
    }
  //}}}

private:
  //{{{
  void drawFileInfo (cMp3File* file) {

    getDeviceContext()->DrawText (file->getFileName().data(), (uint32_t)file->getFileName().size(), getTextFormat(),
                                  file->getLayout(),
                                  !file->isLoaded() ? getGreyBrush() :
                                   ((file == mMp3File) || (file == mProxFile) ? getWhiteBrush() : getBlueBrush()));
    }
  //}}}

  // iPlayer
  //{{{
  int getAudSampleRate() {
    return mMp3File->getSampleRate();
    }
  //}}}
  //{{{
  double getSecsPerAudFrame() {
    return mMp3File->getSecsPerFrame();
    }
  //}}}
  //{{{
  double getSecsPerVidFrame() {
    return 0;
    }
  //}}}
  //{{{
  bool getPlaying() {
    return mPlaying;
    }
  //}}}
  //{{{
  double getPlaySecs() {
    return mPlaySecs;
    }
  //}}}
  //{{{
  void setPlaySecs (double secs) {
    if (secs < 0)
      mPlaySecs = 0;
    else if (secs > mMp3File->getMaxSecs())
      mPlaySecs = mMp3File->getMaxSecs();
    else
      mPlaySecs = secs;
    }
  //}}}
  //{{{
  void incPlaySecs (double inc) {
    setPlaySecs (mPlaySecs + inc);
    }
  //}}}
  //{{{
  void setPlaying (bool playing) {
    mPlaying = playing;
    }
  //}}}
  //{{{
  void togglePlaying() {
    setPlaying (!mPlaying);
    }
  //}}}

  // thread funcs
  //{{{
  void fileScanner (wchar_t* rootDir) {

    auto time = getTimer();

    scan (wstring(), rootDir, L"*.mp3", mNumNestedFiles, mNumNestedDirs,  this, &cMp3Window::changed);
    simpleLayout (RectF (0, mLayoutOffset, getClientF().width, mLayoutOffset + 20.0f));
    changed();

    wcout << L"scanDirectoryFunc - files:" << mNumNestedFiles
          << L" directories:" << mNumNestedDirs
          << L" took:" << getTimer() - time
          << endl;
    }
  //}}}
  //{{{
  void filesLoader (int index) {

    // give scanner a chance to get going
    auto slept = 0;
    while (slept < 10)
      Sleep (slept++);

    while (slept < 20) {
      cMp3File* mp3File = nextLoadFile();
      if (mp3File) {
        mp3File->load (getDeviceContext(), getBitmapProperties(), false);
        }
      else
        Sleep (++slept);
      }

    wcout << L"filesLoader:" << index << L" slept:" << slept << endl;
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);

    cMp3Decoder mMp3Decoder;
    auto samples = (int16_t*)malloc (1152*2*2);

    auto fileHandle = CreateFile (mMp3File->getFullFileName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    auto mFileSize = (int)GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (uint8_t*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    while (!mMp3File || mMp3File->getMaxSecs() < 1)
      Sleep (10);

    audOpen (getAudSampleRate(), 16, 2);

    mPlaySecs = 0;
    double lastPlaySecs = 0;
    auto playPtr = 0;

    bool skipped = false;
    while (playPtr < mFileSize) {
      int bytesUsed = mMp3Decoder.decodeNextFrame (fileBuffer + mMp3File->getTagSize() + playPtr, mFileSize - playPtr, nullptr, samples);
      if (bytesUsed > 0) {
        if (!skipped)
          audPlay (samples, 1152*2*2, 1.0f);

        skipped = lastPlaySecs != mPlaySecs;
        playPtr = skipped ? int ((mFileSize - mMp3File->getTagSize()) * (mPlaySecs / mMp3File->getMaxSecs())) : playPtr + bytesUsed;

        //printf ("%d %f %f %f\n", skipped, playPtr, mPlaySecs, mMp3File->getMaxSecs());

        mPlaySecs += getSecsPerAudFrame();
        if (mPlaySecs > mMp3File->getMaxSecs())
          mPlaySecs = mMp3File->getMaxSecs();
        lastPlaySecs = mPlaySecs;
        changed();
        }
      else
        break;
      }

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);

    CoUninitialize();
    }
  //}}}

  //{{{  private vars
  bool mIsDirectory = false;
  float mLayoutOffset = 0;

  bool mDownConsumed = false;

  bool mPlaying = true;
  double mPlaySecs = 0;

  int mNumNestedDirs = 0;
  int mNumNestedFiles = 0;

  cMp3File* mMp3File = nullptr;
  cMp3File* mProxFile = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  startTimer();

  cMp3Window mp3Window;
  mp3Window.run (L"mp3window", 480, 272, argv[1]);
  }
//}}}
