// cMp3Window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cAudio.h"
#include "../common/cMp3File.h"
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
      thread ([=]() { mMp3File->load (getDeviceContext(), getBitmapProperties(), true); }).detach();
      }

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
    changed();
    }
  }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {

    if (x > getClientF().width - 100)
      mDownConsumed = true;
    else if (x < 100) {
      mProxFile = pick (Point2F ((float)x, (float)y));
      if (mProxFile && (mProxFile->isLoaded() > 0)) {
        mMp3File = mProxFile;
        mMp3File->selected (getDeviceContext(), getBitmapProperties());
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
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

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

    auto loadSamples = true;
    while (slept < 20) {
      cMp3File* mp3File = nextLoadFile();
      if (mp3File) {
        mp3File->load (getDeviceContext(), getBitmapProperties(), loadSamples);
        loadSamples = false;
        }
      else
        Sleep (++slept);
      }

    wcout << L"filesLoader:" << index << L" slept:" << slept << endl;
    }
  //}}}
  //{{{
  void oldPlayer() {

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
  //{{{
  void player() {

    CoInitialize (NULL);

    cMp3Decoder* mMp3Decoder = new cMp3Decoder();

    while (!mMp3File || mMp3File->getMaxSecs() < 1)
      Sleep (10);
    auto samples = (int16_t*)malloc (1152*2*2);
    audOpen (getAudSampleRate(), 16, 2);

    auto fileHandle = CreateFile (mMp3File->getFullFileName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    auto mFileSize = (int)GetFileSize (fileHandle, NULL);

    int chunkSize = 8192;
    auto chunkBuffer = (uint8_t*)malloc (chunkSize + 1044);

    mPlaySecs = 0;
    auto mPlayBytes = 0;
    while (mPlayBytes < mFileSize) {
      DWORD chunkBytesLeft;
      ReadFile (fileHandle, chunkBuffer, chunkSize, &chunkBytesLeft, NULL);
      printf ("readChunk %d of %d\n", chunkSize, chunkBytesLeft);
      if (!chunkBytesLeft)
        break;

      auto chunkPtr = chunkBuffer;
      int bytesUsed;
      while (bytesUsed = mMp3Decoder->findNextHeader (chunkPtr, chunkBytesLeft)) {
        chunkPtr += bytesUsed;
        chunkBytesLeft -= bytesUsed;
        mPlayBytes += bytesUsed;

        if (mMp3Decoder->getFrameBodySize() > (int)chunkBytesLeft) {
          // load rest of frame
          DWORD bytesLoaded;
          ReadFile (fileHandle, chunkPtr + chunkBytesLeft, mMp3Decoder->getFrameBodySize() - chunkBytesLeft, &bytesLoaded, NULL);
          printf ("readRestFrame %d of %d\n", mMp3Decoder->getFrameBodySize() - chunkBytesLeft, bytesLoaded);
          if (!bytesLoaded)
            break;
          chunkBytesLeft += bytesLoaded;
          }

        bytesUsed = mMp3Decoder->decodeFrameBody (chunkPtr, nullptr, samples);
        audPlay (samples, 1152*2*2, 1.0f);
        chunkPtr += bytesUsed;
        chunkBytesLeft -= bytesUsed;
        mPlayBytes += bytesUsed;
        if (mPlaySecs  < mMp3File->getMaxSecs()) {
          mPlaySecs += getSecsPerAudFrame();
          changed();
          }
        }
      }
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
  mp3Window.run (L"mp3window", 1280, 720, argv[1]);
  }
//}}}
