// mp3window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cAudio.h"
#include "../common/cMp3File.h"

#include "Shlwapi.h" // for shell path functions
#pragma comment(lib,"shlwapi.lib")

#include "concurrent_vector.h"
//}}}

//{{{  typedef
class cMp3Window;
typedef void (cMp3Window::*cMp3WindowFunc)();
typedef void (cMp3Window::*cMp3WindowItemFunc)(cMp3File* item);
//}}}
//{{{
class cMp3Files {
public:
  cMp3Files() {}
  virtual ~cMp3Files() {}
  //{{{
  void scan (wstring& parentName, wchar_t* directoryName, wchar_t* pathMatchName, int& numItems, int& numDirs,
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
          directory->scan (mFullDirName, findFileData.cFileName, pathMatchName, numItems, numDirs, mp3Window, func);
          mDirectories.push_back (directory);
         (mp3Window->*func)();
          }
        else if (PathMatchSpec (findFileData.cFileName, pathMatchName)) {
          mItems.push_back (new cMp3File (mFullDirName, findFileData.cFileName));
          (mp3Window->*func)();
          }
        } while (FindNextFile (file, &findFileData));
      FindClose (file);
      }

    numDirs += (int)mDirectories.size();
    numItems += (int)mItems.size();
    }
  //}}}

  //{{{
  cMp3File* pickItem (D2D1_POINT_2F& point) {

    for (auto directory : mDirectories) {
      auto pickedItem = directory->pickItem (point);
      if (pickedItem)
        return pickedItem;
      }

    for (auto item : mItems)
      if (item->pickItem (point))
        return item;

    return nullptr;
    }
  //}}}
  //{{{
  void traverseItems (cMp3Window* mp3Window, cMp3WindowItemFunc mp3Func) {

    for (auto directory : mDirectories)
      directory->traverseItems (mp3Window, mp3Func);

    for (auto item : mItems)
      (mp3Window->*mp3Func)(item);
    }
  //}}}
  //{{{
  void simpleLayout (D2D1_RECT_F& rect) {

    for (auto directory : mDirectories)
      directory->simpleLayout (rect);

    for (auto item : mItems) {
      item->setLayout (rect);
      rect.top += 20.0f;
      rect.bottom += 20.0f;
      }
    }
  //}}}
  //{{{
  cMp3File* nextLoadItem() {

    for (auto directory : mDirectories) {
      auto item = directory->nextLoadItem();
      if (item)
        return item;
      }

    for (auto item : mItems)
      if (!item->isLoaded())
        return item;

    return nullptr;
    }
  //}}}

private:
  // private vars
  wstring mDirName;
  wstring mFullDirName;
  concurrency::concurrent_vector<cMp3File*> mItems;
  concurrency::concurrent_vector<cMp3Files*> mDirectories;
  };
//}}}

class cMp3Window : public cD2dWindow, public cAudio, public cMp3Files {
public:
  cMp3Window() {}
  ~cMp3Window() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFileName) {

    initialise (title, width, height);

    mIsDirectory = (GetFileAttributes (wFileName) & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (mIsDirectory) {
      thread ([=]() { fileScanner (wFileName); }).detach();
      thread ([=]() { filesLoader (0); } ).detach();
      }
    else {
      mMp3File = new cMp3File (wFileName);
      thread ([=]() { mMp3File->load (getDeviceContext()); }).detach();
      }

    thread ([=]() { player(); }).detach();

    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true;

      case 0x20 : togglePlaying(); break;

      case 0x21 : incPlaySecs (-5); changed(); break;
      case 0x22: incPlaySecs(5); changed(); break;

      case 0x23 : setPlaySecs (mMp3File->getMaxSecs()); changed(); break;
      case 0x24 : setPlaySecs (0); changed(); break;

      case 0x25 : incPlaySecs (-1); changed(); break;
      case 0x27 : incPlaySecs (1); changed(); break;

      case 0x26 : setPlaying (false); incPlaySecs (-getSecsPerAudFrame()); changed(); break;
      case 0x28 : setPlaying (false); incPlaySecs (getSecsPerAudFrame()); break;

      default   : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    mLayoutOffset += delta/6;
    if (mLayoutOffset > 20.0f)
      mLayoutOffset = 20.0f;
    simpleLayout (RectF (0, mLayoutOffset, getClientF().width, mLayoutOffset + 20.0f));

    //auto ratio = controlKeyDown ? 1.5f : shiftKeyDown ? 1.2f : 1.1f;
    //if (delta > 0)
    //  ratio = 1.0f/ratio;

    //setVolume (getVolume() * ratio);

    changed();
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {

    mDownConsumed = false;
    if (x > getClientF().width - 100)
      mDownConsumed = true;
    else if (x < 100) {
      auto item = pickItem (Point2F ((float)x, (float)y));
      if (item && (item->isLoaded() > 0)) {
        mMp3File = item;
        mPlaySecs = 0;
        setPlaying (true);
        mDownConsumed = true;
        }
      }
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (x > int(getClientF().width - 100))
      setVolume (y / (getClientF().height * 0.8f));
    else if (!mDownConsumed)
      incPlaySecs (-xInc * getSecsPerAudFrame());

    changed();
    }
  //}}}
  //{{{
  void onMouseUp  (bool right, bool mouseMoved, int x, int y) {

    if (!mouseMoved && !mDownConsumed)
      togglePlaying();

    mDownConsumed = true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->Clear (ColorF(ColorF::Black));

    if (mMp3File && mMp3File->getBitmap())
      dc->DrawBitmap (mMp3File->getBitmap(), RectF (0.0f, 0.0f, getClientF().width, getClientF().height));

    // yellow volume bar
    dc->FillRectangle (RectF (getClientF().width-20,0, getClientF().width, getVolume()*0.8f*getClientF().height), getYellowBrush());

    wchar_t wStr[200];
    mIsDirectory ? swprintf (wStr, 200, L"files:%d dirs:%d", mNumNestedItems, mNumNestedDirs)
                 : swprintf (wStr, 200, L"%3.2f %3.2f %dk %d %d %x",
                             mPlaySecs, mMp3File->getMaxSecs(), mMp3File->getBitRate()/1000, getAudSampleRate(),
                             mMp3File->getChannels(), mMp3File->getMode());
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                  RectF(0.0f, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());

    if (mMp3File) {
      int rows = 6;
      int frame = int(mPlaySecs / getSecsPerAudFrame());
      for (int i = 0; i < rows; i++) {
        for (float f = 0.0f; f < getClientF().width; f++) {
          if (mMp3File->getFrame (frame))
            dc->FillRectangle (
              RectF (!(i & 1) ? f : (getClientF().width - f),
                     ((i + 0.5f) * (getClientF().height / rows)) - mMp3File->getFrame (frame)->mPower[0],
                    !(i & 1) ? (f + 1.0f) : (getClientF().width - f + 1.0f),
                    ((i + 0.5f) * (getClientF().height / rows)) + mMp3File->getFrame (frame)->mPower[1]), getBlueBrush());
          frame++;
          }
        }
      }

    traverseItems (this, &cMp3Window::drawItem);
    }
  //}}}

private:
  //{{{
  void drawItem (cMp3File* item) {

    wchar_t wStr[200];
    swprintf (wStr, 200, L"%s", item->getFileName().c_str());
    getDeviceContext()->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), item->getLayout(),
                                  item->isLoaded() ? getWhiteBrush() : getGreyBrush());
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

  //{{{
  void fileScanner (wchar_t* rootDir) {

    auto time = getTimer();

    scan (wstring(), rootDir, L"*.mp3", mNumNestedItems, mNumNestedDirs,  this, &cMp3Window::changed);
    simpleLayout (RectF (0, mLayoutOffset, getClientF().width, mLayoutOffset + 20.0f));
    changed();

    wcout << L"scanDirectoryFunc exit Items:" << mNumNestedItems
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
      cMp3File* mp3File = nextLoadItem();
      if (mp3File)
        mp3File->load (getDeviceContext());
      else
        Sleep (++slept);
      }

    wcout << L"filesLoader:" << index << L" slept:" << slept << endl;
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);

    while (!mMp3File || mMp3File->getMaxSecs() < 1)
      Sleep (10);

    audOpen (getAudSampleRate(), 16, 2);

    mPlaySecs = 0;
    while (true) {
      if (mPlaying) {
        int audFrame = int (mPlaySecs / getSecsPerAudFrame());
        audPlay (mMp3File->getFrame(audFrame)->mSamples, mMp3File->getFrame(audFrame)->mNumSampleBytes, 1.0f);
        if (mPlaySecs < mMp3File->getMaxSecs()) {
          mPlaySecs += getSecsPerAudFrame();
          changed();
          }
        }
      else
        audSilence();
      }

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
  int mNumNestedItems = 0;
  cMp3File* mMp3File = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  startTimer();

  cMp3Window mp3Window;
  mp3Window.run (L"mp3window", 1280, 720, argv[1]);
  }
//}}}
