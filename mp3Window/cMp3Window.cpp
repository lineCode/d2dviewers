// mp3window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cJpegImage.h"

#include "../common/cAudio.h"
#include "../common/cAudFrame.h"
#include "../common/cMp3decoder.h"

#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")
//}}}
//{{{  typedef
class cMp3Window;
typedef void (cMp3Window::*cMp3WindowFunc)();

class cMp3Item;
typedef void (cMp3Window::*cMp3WindowItemFunc)(cMp3Item* item);
//}}}

//{{{
class cMp3Item {
public:
  cMp3Item (wchar_t* filename) : mFilename(filename), mFullFilename (filename) {}
  cMp3Item (wstring& parentName, wchar_t* filename) : mFilename(filename), mFullFilename (parentName + L"\\" + filename) {}

  //{{{
  int isLoaded() {
    return mLoaded;
    }
  //}}}
  //{{{
  wstring getFilename() {
    return mFilename;
    }
  //}}}
  //{{{
  wstring getFullFilename() {
    return mFullFilename;
    }
  //}}}
  //{{{
  int getSampleRate() {
    return mSampleRate;
    }
  //}}}
  //{{{
  int getChannels() {
    return mChannels;
    }
  //}}}
  //{{{
  int getBitRate() {
    return mBitRate;
    }
  //}}}
  //{{{
  int getMode() {
    return mMode;
    }
  //}}}
  //{{{
  double getMaxSecs() {
    return mFrames.size() * getSecsPerFrame();
    }
  //}}}
  //{{{
  double getSecsPerFrame() {
    return 1152.0 / mSampleRate;
    }
  //}}}

  //{{{
  cAudFrame* getFrame (int frame) {
    return (frame >= 0) && (frame < mFrames.size()) ? mFrames[frame] : nullptr;
    }
  //}}}
  //{{{
  ID2D1Bitmap* getBitmap() {
    return mBitmap;
    }
  //}}}

  //{{{
  D2D1_RECT_F& getLayout() {
    return mLayout;
    }
  //}}}
  //{{{
  void setLayout (D2D1_RECT_F& layout) {
    mLayout = layout;
    }
  //}}}
  //{{{
  bool pickItem (D2D1_POINT_2F& point) {

    return (point.x > mLayout.left) && (point.x < mLayout.right) &&
           (point.y > mLayout.top) && (point.y < mLayout.bottom);
    }
  //}}}

  //{{{
  void load (ID2D1DeviceContext* dc) {

    mLoaded = 1;
    auto fileHandle = CreateFile (mFullFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    auto mFileBytes = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (uint8_t*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    auto tagSize = id3tag (dc, fileBuffer, mFileBytes);

    cMp3Decoder mMp3Decoder;
    auto ptr = fileBuffer + tagSize;
    int bufferBytes = mFileBytes - tagSize;
    while (bufferBytes > 0) {
      int16_t samples[1152*2];
      int bytesUsed = mMp3Decoder.decodeFrame (ptr, bufferBytes, samples);
      if (bytesUsed > 0) {
        //{{{  got frame
        mSampleRate = mMp3Decoder.getSampleRate();
        mBitRate = mMp3Decoder.getBitRate();
        mChannels = mMp3Decoder.getNumChannels();
        mMode = mMp3Decoder.getMode();

        ptr += bytesUsed;
        bufferBytes -= bytesUsed;

        cAudFrame* audFrame = new cAudFrame();
        audFrame->set (0, 2, mSampleRate, 1152);
        auto samplePtr = audFrame->mSamples;

        auto valueL = 0.0;
        auto valueR = 0.0;
        auto lrPtr = samples;

        if (mChannels == 1)
          // fake stereo
          for (auto i = 0; i < 1152; i++) {
            *samplePtr = *lrPtr;
            valueL += pow (*samplePtr++, 2);
            *samplePtr = *lrPtr++;
            valueR += pow (*samplePtr++, 2);
            }
        else
          for (auto i = 0; i < 1152; i++) {
            *samplePtr = *lrPtr++;
            valueL += pow (*samplePtr++, 2);
            *samplePtr = *lrPtr++;
            valueR += pow (*samplePtr++, 2);
            }

        audFrame->mPower[0] = (float)sqrt (valueL) / (1152 * 2.0f);
        audFrame->mPower[1] = (float)sqrt (valueR) / (1152 * 2.0f);
        mFrames.push_back (audFrame);
        //changed();
        //printf ("if bytesused loaded %d %d\n", bufferBytes, bytesUsed);
        }
        //}}}
      else
        break;
      //printf ("while bufferBytes loaded %d %d\n", bufferBytes, bytesUsed);
      }

    mLoaded = 2;
    printf ("finished load\n");

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}
  //{{{
  //void loaderffmpeg (const wchar_t* wFilename) {

    //auto fileHandle = CreateFile (wFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    //int mFileBytes = GetFileSize (fileHandle, NULL);
    //auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    //auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    //av_register_all();
    //AVCodecParserContext* audParser = av_parser_init (AV_CODEC_ID_MP3);
    //AVCodec* audCodec = avcodec_find_decoder (AV_CODEC_ID_MP3);
    //AVCodecContext*audContext = avcodec_alloc_context3 (audCodec);
    //avcodec_open2 (audContext, audCodec, NULL);

    //AVPacket avPacket;
    //av_init_packet (&avPacket);
    //avPacket.data = fileBuffer;
    //avPacket.size = 0;

    //auto time = startTimer();

    //int pesLen = mFileBytes;
    //uint8_t* pesPtr = fileBuffer;
    //int mLoadAudFrame = 0;
    //while (pesLen) {
      //auto lenUsed = av_parser_parse2 (audParser, audContext, &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      //pesPtr += lenUsed;
      //pesLen -= lenUsed;

      //if (avPacket.data) {
        //auto got = 0;
        //auto avFrame = av_frame_alloc();
        //auto bytesUsed = avcodec_decode_audio4 (audContext, avFrame, &got, &avPacket);
        //if (bytesUsed >= 0) {
          //avPacket.data += bytesUsed;
          //avPacket.size -= bytesUsed;
          //if (got) {
            //{{{  got frame
            //mSampleRate = avFrame->sample_rate;
            //mAudFrames[mLoadAudFrame] = new cAudFrame();
            //mAudFrames[mLoadAudFrame]->set (0, avFrame->channels, 48000, avFrame->nb_samples);
            //auto samplePtr = (short*)mAudFrames[mLoadAudFrame]->mSamples;
            //double valueL = 0;
            //double valueR = 0;
            //short* leftPtr = (short*)avFrame->data[0];
            //short* rightPtr = (short*)avFrame->data[1];
            //for (int i = 0; i < avFrame->nb_samples; i++) {
              //*samplePtr = *leftPtr++;
              //valueL += pow (*samplePtr++, 2);
              //*samplePtr = *rightPtr++;
              //valueR += pow (*samplePtr++, 2);
              //}
            //mAudFrames[mLoadAudFrame]->mPower[0] = (float)sqrt (valueL) / (avFrame->nb_samples * 2.0f);
            //mAudFrames[mLoadAudFrame]->mPower[1] = (float)sqrt (valueR) / (avFrame->nb_samples * 2.0f);
            //mLoadAudFrame++;
            //changed();
            //}
            //}}}
          //av_frame_free (&avFrame);
          //}
        //}
      //}

    //UnmapViewOfFile (fileBuffer);
    //CloseHandle (fileHandle);
    //}
  //}}}

private:
  //{{{
  int id3tag (ID2D1DeviceContext* dc, uint8_t* buffer, int bufferLen) {
  // check for ID3 tag

    auto ptr = buffer;
    auto tag = ((*ptr)<<24) | (*(ptr+1)<<16) | (*(ptr+2)<<8) | *(ptr+3);

    if (tag == 0x49443303)  {
     // got ID3 tag
      auto tagSize = (*(ptr+6)<<21) | (*(ptr+7)<<14) | (*(ptr+8)<<7) | *(ptr+9);
      printf ("%c%c%c ver:%d %02x flags:%02x tagSize:%d\n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), tagSize);
      ptr += 10;

      while (ptr < buffer + tagSize) {
        auto tag = ((*ptr)<<24) | (*(ptr+1)<<16) | (*(ptr+2)<<8) | *(ptr+3);
        auto frameSize = (*(ptr+4)<<24) | (*(ptr+5)<<16) | (*(ptr+6)<<8) | (*(ptr+7));
        if (!frameSize)
          break;
        auto frameFlags1 = *(ptr+8);
        auto frameFlags2 = *(ptr+9);

        printf ("%c%c%c%c - %02x %02x - frameSize:%d - ", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), frameFlags1, frameFlags2, frameSize);
        for (auto i = 0; i < (tag == 0x41504943 ? 11 : frameSize); i++)
          printf ("%c", *(ptr+10+i));
        printf ("\n");

        for (auto i = 0; i < (frameSize < 32 ? frameSize : 32); i++)
          printf ("%02x ", *(ptr+10+i));
        printf ("\n");

        if (tag == 0x41504943) {
          auto jpegImage = new cJpegImage();
          if (jpegImage->loadBuffer (dc, 1, ptr + 10 + 14, frameSize - 14))
            mBitmap = jpegImage->getFullBitmap();
          }
        ptr += frameSize + 10;
        }
      return tagSize + 10;
      }
    else {
      // print start of file
      for (auto i = 0; i < 32; i++)
        printf ("%02x ", *(ptr+i));
      printf ("\n");
      return 0;
      }
    }
  //}}}

  int mLoaded = 0;
  wstring mFilename;
  wstring mFullFilename;

  int mSampleRate = 44100;
  int mChannels = 2;
  int mBitRate = 0;
  int mMode = 0;

  D2D1_RECT_F mLayout = {0,0,0,0};

  ID2D1Bitmap* mBitmap = nullptr;
  concurrency::concurrent_vector<cAudFrame*> mFrames;
  };
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
          mItems.push_back (new cMp3Item (mFullDirName, findFileData.cFileName));
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
  cMp3Item* pickItem (D2D1_POINT_2F& point) {

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
  cMp3Item* nextLoadItem() {

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
  concurrency::concurrent_vector<cMp3Item*> mItems;
  concurrency::concurrent_vector<cMp3Files*> mDirectories;
  };
//}}}

class cMp3Window : public cD2dWindow, public cAudio, public cMp3Files {
public:
  cMp3Window() {}
  ~cMp3Window() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFilename) {

    initialise (title, width, height);

    mIsDirectory = (GetFileAttributes (wFilename) & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (mIsDirectory) {
      thread ([=]() { fileScanner (wFilename); }).detach();
      thread ([=]() { filesLoader (0); } ).detach();
      }
    else {
      mMp3Item = new cMp3Item (wFilename);
      thread ([=]() { mMp3Item->load (getDeviceContext()); }).detach();
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

      case 0x23 : setPlaySecs (mMp3Item->getMaxSecs()); changed(); break;
      case 0x24 : setPlaySecs (0); changed(); break;

      case 0x25 : incPlaySecs (-1); changed(); break;
      case 0x27 : incPlaySecs (1); changed(); break;

      case 0x26 : setPlaying (false); incPlaySecs (-mMp3Item->getSecsPerFrame()); changed(); break;
      case 0x28 : setPlaying (false); incPlaySecs (mMp3Item->getSecsPerFrame()); break;

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

    if (y < getClientF().width - 50) {
      auto item = pickItem (Point2F ((float)x, (float)y));
      if (item) {
        if (item->isLoaded() > 0)  {
          mMp3Item = item;
          mPlaySecs = 0;
          }
        mDownConsumed = true;
        }
      else
        mDownConsumed = false;
      }
    else
       mDownConsumed = true;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (x > int(getClientF().width-50))
      setVolume (y / (getClientF().height * 0.8f));
    else
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

    if (mMp3Item && mMp3Item->getBitmap())
      dc->DrawBitmap (mMp3Item->getBitmap(), RectF (0.0f, 0.0f, getClientF().width, getClientF().height));

    // yellow volume bar
    dc->FillRectangle (RectF (getClientF().width-20,0, getClientF().width, getVolume()*0.8f*getClientF().height), getYellowBrush());

    if (mMp3Item) {
      int rows = 6;
      int frame = int(mPlaySecs / mMp3Item->getSecsPerFrame());
      for (int i = 0; i < rows; i++) {
        for (float f = 0.0f; f < getClientF().width; f++) {
          auto rWave = RectF (!(i & 1) ? f : (getClientF().width-f), 0, !(i & 1) ? (f+1.0f) : (getClientF().width-f+1.0f), 0);
          if (mMp3Item->getFrame (frame)) {
            rWave.top    = ((i + 0.5f) * (getClientF().height / rows)) - mMp3Item->getFrame(frame)->mPower[0];
            rWave.bottom = ((i + 0.5f) * (getClientF().height / rows)) + mMp3Item->getFrame(frame)->mPower[1];
            dc->FillRectangle (rWave, getBlueBrush());
            }
          frame++;
          }
        }
      }

    if (mIsDirectory) {
      wchar_t wStr[200];
      swprintf (wStr, 200, L"files:%d dirs:%d", mNumNestedItems, mNumNestedDirs);
      dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                    RectF(0.0f, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());
      }
    else if (mMp3Item) {
      wchar_t wStr[200];
      swprintf (wStr, 200, L"%3.2f %3.2f %dk %d %d %x",
                mPlaySecs, mMp3Item->getMaxSecs(), mMp3Item->getBitRate()/1000, mMp3Item->getSampleRate(),
                mMp3Item->getChannels(), mMp3Item->getMode());
      dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                    RectF(0.0f, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());
      }

    traverseItems (this, &cMp3Window::drawItem);
    }
  //}}}

private:
  //{{{
  void player() {

    CoInitialize (NULL);  // for winAudio

    while (!mMp3Item || mMp3Item->getMaxSecs() < 1)
      Sleep (10);

    audOpen (mMp3Item->getSampleRate(), 16, 2);

    mPlaySecs = 0;
    while (true) {
      if (mPlaying) {
        int audFrame = int (mPlaySecs / mMp3Item->getSecsPerFrame());
        audPlay (mMp3Item->getFrame(audFrame)->mSamples, mMp3Item->getFrame(audFrame)->mNumSampleBytes, 1.0f);
        if (mPlaySecs < mMp3Item->getMaxSecs()) {
          mPlaySecs += mMp3Item->getSecsPerFrame();
          changed();
          }
        }
      else
        audSilence();
      }

    CoUninitialize();
    }
  //}}}
  //{{{  iPlayer
  //{{{
  int getAudSampleRate() {
    return mMp3Item->getSampleRate();
    }
  //}}}
  //{{{
  double getSecsPerAudFrame() {
    return mMp3Item->getSecsPerFrame();
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
    else if (secs > mMp3Item->getMaxSecs())
      mPlaySecs = mMp3Item->getMaxSecs();
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
  //}}}
  //{{{
  void drawItem (cMp3Item* item) {

    wchar_t wStr[200];
    swprintf (wStr, 200, L"%s", item->getFilename().c_str());
    getDeviceContext()->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), item->getLayout(),
                                  item->isLoaded() ? getWhiteBrush() : getGreyBrush());
    }
  //}}}
  //{{{
  void fileScanner (wchar_t* rootDir) {

    auto time1 = getTimer();
    scan (wstring(), rootDir, L"*.mp3", mNumNestedItems, mNumNestedDirs,  this, &cMp3Window::changed);
    int rows = 0;
    simpleLayout (RectF (0, mLayoutOffset, getClientF().width, mLayoutOffset + 20.0f));
    auto time2 = getTimer();
    changed();

    wcout << L"scanDirectoryFunc exit Items:" << mNumNestedItems
          << L" directories:" << mNumNestedDirs
          << L" took:" << time2-time1
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
      cMp3Item* mp3Item = nextLoadItem();
      if (mp3Item)
        mp3Item->load (getDeviceContext());
      else
        Sleep (++slept);
      }

    wcout << L"filesLoader:" << index << L" slept:" << slept << endl;
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
  cMp3Item* mMp3Item = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  startTimer();

  cMp3Window mp3Window;
  mp3Window.run (L"mp3window", 1280, 720, argv[1]);
  }
//}}}
