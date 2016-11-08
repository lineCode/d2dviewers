// cMp3Window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"
#include "../common/cAudio.h"

#include "../../shared/widgets/cRootContainer.h"
#include "../../shared/widgets/cListWidget.h"
#include "../../shared/widgets/cWaveCentreWidget.h"
#include "../../shared/widgets/cWaveOverviewWidget.h"
#include "../../shared/widgets/cWaveLensWidget.h"
#include "../../shared/widgets/cTextBox.h"
#include "../../shared/widgets/cValueBox.h"
#include "../../shared/widgets/cSelectText.h"
#include "../../shared/widgets/cPicWidget.h"
#include "../../shared/widgets/cNumBox.h"
#include "../../shared/widgets/cBmpWidget.h"

#include "../../shared/icons/radioIcon.h"

#include "../../shared/decoders/cTinyJpeg.h"
#include "../../shared/decoders/cMp3Decoder.h"

#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")
#include "../common/cHttp.h"

#include "../../shared/decoders/cHlsLoader.h"
#include "../../shared/widgets/cHlsPowerWidget.h"
#include "../../shared/widgets/cHlsInfoBox.h"
#include "../../shared/widgets/cHlsIncBox.h"
#include "../../shared/widgets/cHlsDotsBox.h"
//}}}
static const bool kJpeg = false;
//{{{
class cFileMapTinyJpeg : public cTinyJpeg {
public:
  cFileMapTinyJpeg (uint16_t components) : cTinyJpeg(components) {}

  ~cFileMapTinyJpeg() {
    UnmapViewOfFile (mFileBuffer);
    CloseHandle (mFileHandle);
    }

  bool readHeader (std::string fileName) {
    mFileHandle = CreateFileA (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    mFileSize = (int)GetFileSize (mFileHandle, NULL);
    mFileBuffer = (uint8_t*)MapViewOfFile (CreateFileMapping (mFileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);
    mFileBufferPtr = mFileBuffer;

    bool ok = cTinyJpeg::readHeader();
    if (ok && getThumbBytes()) {
      mFileBufferPtr = mFileBuffer + getThumbOffset();
      ok = cTinyJpeg::readHeader();
      }
    return ok;
    }

protected:
  uint32_t read (uint8_t* buffer, uint32_t bytes) {
    memcpy (buffer, mFileBufferPtr, bytes);
    mFileBufferPtr += bytes;
    return bytes;
    }

private:
  HANDLE mFileHandle;
  uint8_t* mFileBuffer = nullptr;
  uint8_t* mFileBufferPtr = nullptr;
  int mFileSize = 0;
  };
//}}}

class cMp3Window : public iDraw, public cAudio, public cD2dWindow {
public:
  //{{{
  void run (wchar_t* title, int width, int height, std::string fileName) {

    initialise (title, width+10, height+6);

    // create brush
    getDeviceContext()->CreateSolidColorBrush (ColorF(ColorF::CornflowerBlue), &mBrush);

    mRoot = new cRootContainer (width, height);
    setChangeRate (1);

    if (kJpeg) {
      //{{{  jpeg viewer
      listDirectory (std::string(), fileName.empty() ? "C:/Users/colin/Desktop/lamorna" : fileName, "*.jpg");
      std::thread([=]() { loadJpegThread(); }).detach();
      }
      //}}}
    else if (fileName.empty()) {
      //{{{  hls player
      mHlsLoader = new cHlsLoader();
      mHlsSem = CreateSemaphore (NULL, 0, 1, L"hlsSem");  // initial 0, max 1

      initHlsMenu();

      mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, 1, mRoot->getHeight()));

      // launch loaderThread
      std::thread ([=]() { hlsLoader(); } ).detach();

      // launch playerThread, higher priority
      auto playerThread = std::thread ([=]() { hlsPlayer(); });
      SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
      playerThread.detach();
      }
      //}}}
    else {
      //{{{  mp3 player
      bool mFrameChanged = false;;
      mPlayFrame = 0;
      mFramePosition = (int*)malloc (60*60*60*2*sizeof(int));
      mWave = (uint8_t*)malloc (60*60*60*2*sizeof(uint8_t));
      mSamples = (int16_t*)malloc (1152*2*2);
      memset (mSamples, 0, 1152*2*2);

      initMp3Menu();

      if (fileName.empty())
        mFileList.push_back ("C:/Users/colin/Desktop/Bread.mp3");
      else if (GetFileAttributesA (fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
        listDirectory (std::string(), fileName, "*.mp3");
        //std::thread ([=]() { listThread (fileName ? fileName : "D:/music"); } ).detach();
      else
        mFileList.push_back (fileName);

      std::thread([=]() { audioThread(); }).detach();
      std::thread([=]() { playThread(); }).detach();
      }
      //}}}

    messagePump();
    };
  //}}}
  //{{{  iDraw
  uint16_t getLcdWidthPix() { return mRoot->getWidthPix(); }
  uint16_t getLcdHeightPix() { return mRoot->getHeightPix(); }

  //{{{
  void pixel (uint32_t colour, int16_t x, int16_t y) {
    rectClipped (colour, x, y, 1, 1);
    }
  //}}}
  //{{{
  void rect (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    mBrush->SetColor (ColorF (colour, ((colour & 0xFF000000) >> 24) / 255.0f));
    getDeviceContext()->FillRectangle (RectF (float(x), float(y), float(x + width), float(y + height)), mBrush);
    }
  //}}}
  //{{{
  void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    rect (0xC0000000 | (colour & 0xFFFFFF), x,y, width, height);
    }
  //}}}
  //{{{
  int text (uint32_t colour, uint16_t fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    // create layout
    std::wstring wstr = converter.from_bytes (str.data());
    IDWriteTextLayout* textLayout;
    getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(),
                                          fontHeight <= cWidget::getFontHeight() ? getTextFormat() : getTextFormatSize (fontHeight),
                                          width, height, &textLayout);

    // draw it
    mBrush->SetColor (ColorF (colour, ((colour & 0xFF000000) >> 24) / 255.0f));
    getDeviceContext()->DrawTextLayout (Point2F(float(x), float(y)), textLayout, mBrush);

    // return measure
    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics (&metrics);
    return (int)metrics.width;
    }
  //}}}

  //{{{
  void copy (uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    if (src && width && height) {
      ID2D1Bitmap* bitmap;
      getDeviceContext()->CreateBitmap (SizeU (width, height), getBitmapProperties(), &bitmap);
      bitmap->CopyFromMemory (&RectU (0, 0, width, height), src, width*4);
      getDeviceContext()->DrawBitmap (bitmap, RectF ((float)x, (float)y, (float)x+width,(float)y+height));
      bitmap->Release();
      }
    }
  //}}}
  void copy (uint8_t* src, int16_t srcx, int16_t srcy, uint16_t srcWidth, int16_t srcHeight,
             int16_t dstx, int16_t dsty, uint16_t dstWidth, uint16_t dstHeight) {};

  //{{{
  ID2D1Bitmap* allocBitmap (uint16_t width, uint16_t height) {

    ID2D1Bitmap* bitmap;
    getDeviceContext()->CreateBitmap (SizeU (width, height), getBitmapProperties(), &bitmap);
    return bitmap;
    }
  //}}}
  //{{{
  void copy (ID2D1Bitmap* bitMap, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    getDeviceContext()->DrawBitmap (bitMap, RectF ((float)x, (float)y, (float)x+width,(float)y+height));
    }
  //}}}
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00:
      case 0x10: // shift
      case 0x11: // control
        return false;
      case 0x1B: // escape
        return true;

      case 0x20: if (mHlsLoader) mHlsLoader->togglePlaying(); break;              // space
      case 0x21: if (mHlsLoader) mHlsLoader->incPlayFrameSecs (-60); break;       // page up
      case 0x22: if (mHlsLoader) mHlsLoader->incPlayFrameSecs (60); break;        // page down
      case 0x25: if (mHlsLoader) mHlsLoader->incPlayFrameSecs (-keyInc()); break; // left arrow
      case 0x27: if (mHlsLoader) mHlsLoader->incPlayFrameSecs (keyInc()); break;  // right arrow

      case 0x26:
        //{{{  up arrow
        mFileIndex--;
        mFileIndexChanged = true;

        if (mHlsLoader && mHlsChan > 1) {
          mHlsChan--;
          mHlsLoader->mChanChanged;
          }
        break;
        //}}}
      case 0x28:
        //{{{  down arrow
        mFileIndex++;
        mFileIndexChanged = true;

        if (mHlsLoader && mHlsChan < 6) {
          mHlsChan++;
          mHlsLoader->mChanChanged;
          }
        break;
        //}}}

      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36: mHlsChan = key - '0'; mHlsLoader->mChanChanged; break;

      default: printf ("key %x\n", key);
      }
    return false;
    }
  //}}}
  void onMouseDown (bool right, int x, int y) { mRoot->press (0, x, y, 0,  0, 0); }
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) { mRoot->press (1, x, y, 0, xInc, yInc); }
  void onMouseUp (bool right, bool mouseMoved, int x, int y) { mRoot->release(); }
  void onDraw (ID2D1DeviceContext* dc) { mRoot->render (this); }

private:
  //{{{
  void initHlsMenu() {
    mRoot->addBottomLeft (new cPowerWidget (mHlsLoader, mRoot->getWidth(), -3 + mRoot->getHeight()));

    mRoot->addTopLeft (new cBmpWidget (r1x80, 1, mHlsChan, mHlsLoader->mChanChanged, 4, 4));
    mRoot->add (new cBmpWidget (r2x80, 2, mHlsChan, mHlsLoader->mChanChanged, 4, 4));
    mRoot->add (new cBmpWidget (r3x80, 3, mHlsChan, mHlsLoader->mChanChanged, 4, 4));
    mRoot->add (new cBmpWidget (r4x80, 4, mHlsChan, mHlsLoader->mChanChanged, 4, 4));
    mRoot->add (new cBmpWidget (r5x80, 5, mHlsChan, mHlsLoader->mChanChanged, 4, 4));
    mRoot->add (new cBmpWidget (r6x80, 6, mHlsChan, mHlsLoader->mChanChanged, 4, 4));
    mRoot->addAt (new cInfoTextBox (mHlsLoader, mRoot->getWidth(), 2), -3 + mRoot->getWidth()/2.0f, -2 + mRoot->getHeight());

    mRoot->addBottomRight (new cDotsBox (mHlsLoader));
    mRoot->addLeft (new cHlsIncBox (mHlsLoader,  "m",  60, 2));
    mRoot->addLeft (new cHlsIncBox (mHlsLoader, "-m", -60,  2));

    mRoot->addBottomLeft (new cSelectText("48", 48000, mHlsBitrate, mHlsLoader->mChanChanged, 2));
    mRoot->add (new cSelectText ("128", 128000, mHlsBitrate, mHlsLoader->mChanChanged, 2));
    mRoot->add (new cSelectText ("320", 320000, mHlsBitrate, mHlsLoader->mChanChanged, 2));
    }
  //}}}
  //{{{
  void hlsLoader() {

    CoInitialize (NULL);

    mHlsLoader->mChanChanged = true;
    while (true) {
      if (mHlsLoader->mChanChanged)
        mHlsLoader->changeChan (mHlsChan, mHlsBitrate);

      if (!mHlsLoader->load())
        Sleep (1000);

      WaitForSingleObject (mHlsSem, 20 * 1000);
      }

    CoUninitialize();
    }
  //}}}
  //{{{
  void hlsPlayer() {

    CoInitialize (NULL);
    audOpen (48000, 16, 2);

    auto lastSeqNum = 0;
    while (true) {
      int seqNum;
      auto audSamples = mHlsLoader->getSamples (seqNum);
      audPlay (audSamples, 4096, 1.0f);

      if (mHlsLoader->mChanChanged || !seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        ReleaseSemaphore (mHlsSem, 1, NULL);
        }

      if (mVolumeChanged) {
        setVolume (mVolume);
        mVolumeChanged = false;
        }
      }

    audClose();
    CoUninitialize();
    }
  //}}}

  //{{{
  void initMp3Menu() {
    mRoot->addTopLeft (new cListWidget (mFileList, mFileIndex, mFileIndexChanged, mRoot->getWidth(), mRoot->getHeight() - 9));
    mRoot->add (new cWaveCentreWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, mRoot->getWidth(), 3));
    mRoot->add (new cWaveWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, mRoot->getWidth(), 3));
    mRoot->add (new cWaveLensWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, mRoot->getWidth(), 3));

    mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, 1, mRoot->getHeight()-6));
    }
  //}}}
  //{{{
  void waveThread() {

    auto time = getTimer();

    cMp3Decoder mMp3Decoder;
    auto tagBytes = mMp3Decoder.findId3tag (mFileBuffer, mFileSize);
    auto filePosition = tagBytes;

    // use mWave[0] as max
    *mWave = 0;
    auto wavePtr = mWave + 1;

    mLoadedFrame = 0;
    int bytesUsed = 0;
    do {
      mFramePosition [mLoadedFrame] = filePosition;

      bytesUsed = mMp3Decoder.decodeNextFrame (mFileBuffer + filePosition, mFileSize - filePosition, wavePtr, nullptr);
      if (*wavePtr > *mWave)
        *mWave = *wavePtr;
      wavePtr++;
      if (*wavePtr > *mWave)
        *mWave = *wavePtr;
      wavePtr++;
      mLoadedFrame++;

      // predict maxFrame from running totals
      filePosition += bytesUsed;
      mMaxFrame = int(float(mFileSize - tagBytes) * float(mLoadedFrame) / float(filePosition - tagBytes));
      } while ((bytesUsed > 0) && (filePosition < mFileSize));

    // correct maxFrame to counted frame
    mMaxFrame = mLoadedFrame;

    printf ("wave frames:%d fileSize:%d tag:%d max:%d took:%f\n", mLoadedFrame, mFileSize, tagBytes, *mWave, getTimer() - time);
    }
  //}}}
  //{{{
  void playThread() {

    cMp3Decoder mMp3Decoder;
    while (true) {
      //{{{  open and load file into fileBuffer
      auto fileHandle = CreateFileA (mFileList[mFileIndex].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      mFileSize = (int)GetFileSize (fileHandle, NULL);

      auto fileBuffer = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);

      mFileBuffer = (uint8_t*)malloc (mFileSize);
      memcpy (mFileBuffer, fileBuffer, mFileSize);
      //}}}
      std::thread ([=](){waveThread();}).detach();

      int bytesUsed;
      mPlayFrame = 0;
      auto filePosition = mMp3Decoder.findId3tag (mFileBuffer, mFileSize);
      do {
        mWait = true;
        mReady = false;
        bytesUsed = mMp3Decoder.decodeNextFrame (mFileBuffer + filePosition, mFileSize - filePosition, nullptr, mSamples);
        mReady = true;
        if (bytesUsed > 0) {
          filePosition += bytesUsed;
          while (mWait && !mFileIndexChanged)
            Sleep (2);
          mPlayFrame++;
          }

        if (mWaveChanged) {
          filePosition = mFramePosition [mPlayFrame];
          mWaveChanged = false;
          }
        } while (!mFileIndexChanged && (bytesUsed > 0) && (filePosition < mFileSize));

      if (!mFileIndexChanged)
        mFileIndex = (mFileIndex + 1) % mFileList.size();
      mFileIndexChanged = false;
      mPlaying = true;

      //{{{  close file and fileBuffer
      free (mFileBuffer);
      UnmapViewOfFile (fileBuffer);
      CloseHandle (fileHandle);
      //}}}
      }
    }
  //}}}
  //{{{
  void audioThread() {

    CoInitialize (NULL);
    audOpen (44100, 16, 2);

    while (true) {
      mReady && mPlaying ? audPlay (mSamples, 1152*2*2, 1.0f) : audSilence();
      mWait = !mPlaying;
      }

    CoUninitialize();
    }
  //}}}

  //{{{
  void initJpegMenu() {
    mRoot->addTopRight (new cNumBox ("list ", mCount, mCountChanged, 6.0f));
    mRoot->addBelow (new cNumBox ("show ", mNumWidget, mNumChanged, 6.0f));
    mRoot->addTopLeft (new cListWidget (mFileList, mFileIndex, mFileIndexChanged, mRoot->getWidth(), 10));

    for (auto i = 0; i < 18; i++) {
      auto picWidget = new cPicWidget (8.0f, 6.0f, i, mFileIndex, mFileIndexChanged);
      mRoot->add (picWidget);
      mPicWidgets.push_back(picWidget);
      }
    }
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
        else if (PathMatchSpecA (findFileData.cFileName, pathMatchName)) {
          mFileList.push_back (mFullDirName + "/" + findFileData.cFileName);
          mCount++;
          }
        } while (FindNextFileA (file, &findFileData));

      FindClose (file);
      }
    }
  //}}}
  //{{{
  void jpegDecode (int fileIndex, uint16_t& picWidth, uint16_t& picHeight) {

    cFileMapTinyJpeg jpegDecoder (4);
    if (jpegDecoder.readHeader (mFileList[fileIndex].c_str())) {
      // scale to fit
      auto scale = 1;
      auto scaleShift = 0;
      while ((scaleShift < 3) &&
             ((jpegDecoder.getWidth() / scale > getLcdWidthPix()) || (jpegDecoder.getHeight() /scale > getLcdHeightPix()))) {
        scale *= 2;
        scaleShift++;
        }

      mPicWidgets [int(mNumWidget) % mPicWidgets.size()]->setPic (
        jpegDecoder.decodeBody (scaleShift), jpegDecoder.getWidth() >> scaleShift, jpegDecoder.getHeight() >> scaleShift, 4);
      mPicWidgets [int(mNumWidget) % mPicWidgets.size()]->setSelectValue (fileIndex);
      mNumWidget++;
      }
    }
  //}}}
  //{{{
  void listThread (std::string fileName) {

    auto time = getTimer();

    if (GetFileAttributesA (fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
      listDirectory (std::string(), fileName, "*.mp3");

    printf ("files listed %s %d took %f\n", fileName.c_str(), (int)mFileList.size(), getTimer() - time);
    }
  //}}}
  //{{{
  void loadJpegThread() {

    uint16_t picWidth = 0;
    uint16_t picHeight = 0;

    initJpegMenu();

    mFileIndexChanged = true;
    while (true) {
      if (mFileIndexChanged) {
        jpegDecode (mFileIndex, picWidth, picHeight);
        mFileIndexChanged = false;
        }
      else
        Sleep (100);
      }
    }
  //}}}

  //{{{  vars
  cRootContainer* mRoot = nullptr;

  ID2D1SolidColorBrush* mBrush;

  // audio
  int16_t* mSamples = nullptr;
  bool mWait = false;
  bool mReady = false;
  bool mPlaying = true;

  bool mVolumeChanged = false;
  float mVolume = 0.7f;

  // jpeg
  int mFileIndex = 0;
  bool mFileIndexChanged = false;
  bool mPicValueChanged = false;
  std::vector <std::string> mFileList;
  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  bool mCountChanged = false;
  float mCount = 0;
  bool mNumChanged = false;
  float mNumWidget = 0;
  std::vector <cPicWidget*> mPicWidgets;

  uint8_t* mFileBuffer = nullptr;
  int mFileSize = 0;

  // mp3
  int mPlayFrame = 0;
  int mLoadedFrame = 0;
  int mMaxFrame = 0;
  bool mWaveChanged = false;
  uint8_t* mWave = nullptr;
  int* mFramePosition = nullptr;

  // hls
  cHlsLoader* mHlsLoader;
  HANDLE mHlsSem;
  int mHlsChan = 4;
  int mHlsBitrate = 128000;
  //}}}
  };

//{{{
int main (int argc, char* argv[]) {

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

  startTimer();
  cMp3Window mp3Window;
  //mp3Window.run (L"mp3window", 800, 480, argv[1] ? std::string(argv[1]) : std::string());
  mp3Window.run (L"mp3window", 480, 272, argv[1] ? std::string(argv[1]) : std::string());
  }
//}}}
