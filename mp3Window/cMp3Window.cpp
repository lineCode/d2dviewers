// cMp3Window.cpp
//{{{  includes
#include "stdafx.h"

#include <vector>

#include "../common/timer.h"
#include "../common/cD2dWindow.h"
#include "../common/cAudio.h"

#include "../../shared/libfaad/neaacdec.h"
//#pragma comment (lib,"libfaad.lib")

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../../shared/utils.h"
#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/net/cWinEsp8266Http.h"

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

#include "../../shared/hls/hls.h"
#include "../../shared/decoders/cMp3.h"
//}}}
//{{{  debugHeap
#define MAX_HEAP_DEBUG 2000

//{{{
class cHeapAlloc {
public:
  void* mPtr = nullptr;
  size_t mSize = 0;
  uint8_t mHeap = 0;
  };
//}}}

cHeapAlloc mHeapDebugAllocs [MAX_HEAP_DEBUG];
const char* kDebugHeapLabels[3] = { "big", "sml", "new" };
size_t mHeapDebugAllocated[3] = { 0,0,0 };
size_t mHeapDebugHighwater[3] = { 0, 0, 0 };
uint32_t mHeapDebugOutAllocs[3] = { 0, 0, 0 };

//{{{
void* debugMalloc (size_t size, const char* tag, uint8_t heap) {

  auto ptr = malloc (size);

  mHeapDebugOutAllocs[heap]++;
  mHeapDebugAllocated[heap] += size;
  for (auto i = 0; i < MAX_HEAP_DEBUG; i++) {
    if (!mHeapDebugAllocs[i].mPtr) {
      mHeapDebugAllocs[i].mPtr = ptr;
      mHeapDebugAllocs[i].mSize = size;
      mHeapDebugAllocs[i].mHeap = heap;
      break;
      }
    if (i >= MAX_HEAP_DEBUG-1)
      printf ("debugMAlloc::not enough heapDebugAllocs\n");
    }

  if (mHeapDebugAllocated[heap] > mHeapDebugHighwater[heap]) {
    mHeapDebugHighwater[heap] = mHeapDebugAllocated[heap];
    printf ("%s %3d %7zd %7zd %s\n", kDebugHeapLabels[heap],
            mHeapDebugOutAllocs[heap], mHeapDebugAllocated[heap], size, tag ? tag : "");
    }

  return ptr;
  }
//}}}
//{{{
void debugFree (void* ptr) {

  if (ptr) {
    for (auto i = 0; i < MAX_HEAP_DEBUG; i++) {
      if (mHeapDebugAllocs[i].mPtr && mHeapDebugAllocs[i].mPtr == ptr) {
        mHeapDebugAllocs[i].mPtr = nullptr;
        mHeapDebugOutAllocs[mHeapDebugAllocs[i].mHeap]--;
        mHeapDebugAllocated[mHeapDebugAllocs[i].mHeap] -= mHeapDebugAllocs[i].mSize;
        break;
        }
      if (i >= MAX_HEAP_DEBUG-1)
        printf ("debugFree::ptr not found %llx\n", (int64_t)ptr);
      }
    }

  free (ptr);
  }
//}}}

void* operator new (size_t size) { return debugMalloc (size, "", 2); }
void operator delete (void* ptr) { debugFree (ptr); }
void* operator new[](size_t size) { printf("new[] %d\n", int(size)); return debugMalloc (size, "", '['); }
void operator delete[](void *ptr) { printf ("delete[]\n"); debugFree (ptr); }
//}}}
//#define ESP8266

class cMp3Window : public iDraw, public cAudio, public cD2dWindow {
public:
  //{{{
  void run (wchar_t* title, int width, int height, std::string fileName) {

    initialise (title, width+10, height+6);

    // create brush
    getDeviceContext()->CreateSolidColorBrush (ColorF(ColorF::CornflowerBlue), &mBrush);

    mRoot = new cRootContainer (width, height);
    setChangeRate (1);

    if (fileName.empty()) {
      //{{{  hls player
      mHls = new cHls();
      mHlsSem = CreateSemaphore (NULL, 0, 1, L"hlsSem");  // initial 0, max 1

      mReSamples = (int16_t*)malloc (4096);
      memset (mReSamples, 0, 4096);

      hlsMenu (mRoot, mHls);

      // launch loaderThread
      std::thread ([=]() { hlsLoaderThread(); } ).detach();

      // launch playerThread, higher priority
      auto playerThread = std::thread ([=]() { hlsPlayerThread(); });
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
  uint16_t getLcdWidthPix() { return mRoot->getPixWidth(); }
  uint16_t getLcdHeightPix() { return mRoot->getPixHeight(); }

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
  void copy (uint8_t* src, int16_t srcx, int16_t srcy, uint16_t srcWidth, uint16_t srcHeight,
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

      case 0x20: if (mHls) mHls->togglePlaying(); break;         // space
      case 0x21: if (mHls) mHls->incPlaySec (-60); break;       // page up
      case 0x22: if (mHls) mHls->incPlaySec (60); break;        // page down
      case 0x25: if (mHls) mHls->incPlaySec (-keyInc()); break; // left arrow
      case 0x27: if (mHls) mHls->incPlaySec (keyInc()); break;  // right arrow

      case 0x26:
        //{{{  up arrow
        mFileIndex--;
        mFileIndexChanged = true;

        if (mHls && mHls->mHlsChan > 1) {
          mHls->mHlsChan--;
          mHls->mChanChanged = true;
          }
        break;
        //}}}
      case 0x28:
        //{{{  down arrow
        mFileIndex++;
        mFileIndexChanged = true;

        if (mHls && mHls->mHlsChan < 6) {
          mHls->mHlsChan++;
          mHls->mChanChanged = true;
          }
        break;
        //}}}

      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36: mHls->mHlsChan = key - '0'; mHls->mChanChanged = true; break;

      default: debug ("key " + hex(key));
      }
    return false;
    }
  //}}}
  void onMouseDown (bool right, int x, int y) { mRoot->press (0, x, y, 0,  0, 0); }
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) { mRoot->press (1, x, y, 0, xInc, yInc); }
  void onMouseUp (bool right, bool mouseMoved, int x, int y) { mRoot->release(); }
  void onDraw (ID2D1DeviceContext* dc) { if (mRoot) mRoot->render (this); }

private:
  //{{{
  void hlsLoaderThread() {

    CoInitialize (NULL);
  #ifdef ESP8266
    cWinEsp8266Http http;
  #else
    cWinSockHttp http;
  #endif
    http.initialise();

    mHls->mChanChanged = true;
    while (true) {
      if (mHls->mChanChanged)
        mHls->setChan (http, mHls->mHlsChan, mHls->mHlsBitrate);

      mHls->loadPicAtPlayFrame (http);
      if (!mHls->loadAtPlayFrame (http))
        Sleep (1000);

      WaitForSingleObject (mHlsSem, 20 * 1000);
      }

    CoUninitialize();
    }
  //}}}
  //{{{
  void hlsPlayerThread() {

    CoInitialize (NULL);
    audOpen (48000, 16, 2);

    uint32_t seqNum = 0;
    uint32_t numSamples = 0;
    uint32_t lastSeqNum = 0;
    uint16_t scrubCount = 0;
    double scrubSample = 0;
    while (true) {
      if (mHls->getScrubbing()) {
        if (scrubCount == 0)
          scrubSample = mHls->getPlaySample();
        if (scrubCount < 3) {
          auto sample = mHls->getPlaySamples (scrubSample + (scrubCount * kSamplesPerFrame), seqNum, numSamples);
          audPlay (sample, 4096, 1.0f);
          }
        else
          audPlay (nullptr, 4096, 1.0f);
        if (scrubCount++ > 3)
          scrubCount = 0;
        //int srcSamplesConsumed = mHls->getReSamples (mHls->getPlaySample(), seqNum, numSamples, mReSamples, mHls->mSpeed);
        //audPlay (mReSamples, 4096, 1.0f);
        //mHls->incPlaySample (srcSamplesConsumed);
        }
      else if (mHls->getPlaying()) {
        auto sample = mHls->getPlaySamples (mHls->getPlaySample(), seqNum, numSamples);
        audPlay (sample, 4096, 1.0f);
        if (sample)
          mHls->incPlayFrame (1);
        }
      else
        audPlay (nullptr, 4096, 1.0f);

      if (mHls->mChanChanged || !seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        ReleaseSemaphore (mHlsSem, 1, NULL);
        }

      if (mHls->mVolumeChanged) {
        setVolume (mHls->mVolume);
        mHls->mVolumeChanged = false;
        }
      }

    audClose();
    CoUninitialize();
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
        else if (PathMatchSpecA (findFileData.cFileName, pathMatchName))
          if ((findFileData.cFileName[0] != '.') && (findFileData.cFileName[0] != '..'))
            mFileList.push_back (mFullDirName + "/" + findFileData.cFileName);
        } while (FindNextFileA (file, &findFileData));

      FindClose (file);
      }
    }
  //}}}
  //{{{
  void initMp3Menu() {
    mRoot->addTopLeft (new cListWidget (mFileList, mFileIndex, mFileIndexChanged, 0, -6));
    mRoot->add (new cWaveCentreWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, 0, 2));
    mRoot->add (new cWaveWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, 0, 2));
    mRoot->add (new cWaveLensWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, 0, 2));

    mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, 1, -6));
    }
  //}}}
  //{{{
  void waveThread() {

    auto time = getTimer();

    cMp3 mMp3;
    auto tagBytes = mMp3.findId3tag (mFileBuffer, mFileSize);
    auto filePosition = tagBytes;

    // use mWave[0] as max
    *mWave = 0;
    auto wavePtr = mWave + 1;

    mLoadedFrame = 0;
    int bytesUsed = 0;
    do {
      mFramePosition [mLoadedFrame] = filePosition;

      bytesUsed = mMp3.decodeNextFrame (mFileBuffer + filePosition, mFileSize - filePosition, wavePtr, nullptr);
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

    debug ("wave frames " + dec(mLoadedFrame) + " " + dec(mFileSize));
    }
  //}}}
  //{{{
  void playThread() {

    cMp3 mMp3;
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
      auto filePosition = mMp3.findId3tag (mFileBuffer, mFileSize);
      do {
        mWait = true;
        mReady = false;
        bytesUsed = mMp3.decodeNextFrame (mFileBuffer + filePosition, mFileSize - filePosition, nullptr, mSamples);
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

  //{{{  vars
  cRootContainer* mRoot = nullptr;

  // d2dwindow
  ID2D1SolidColorBrush* mBrush;
  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  // audio
  int16_t* mSamples = nullptr;
  int16_t* mReSamples = nullptr;
  bool mWait = false;
  bool mReady = false;
  bool mPlaying = true;

  // mp3
  bool mVolumeChanged = false;
  float mVolume = 0.7f;

  int mFileIndex = 0;
  bool mFileIndexChanged = false;
  std::vector <std::string> mFileList;
  uint8_t* mFileBuffer = nullptr;
  int mFileSize = 0;

  int mPlayFrame = 0;
  int mLoadedFrame = 0;
  int mMaxFrame = 0;
  bool mWaveChanged = false;
  uint8_t* mWave = nullptr;
  int* mFramePosition = nullptr;

  // hls
  cHls* mHls;
  HANDLE mHlsSem;
  //}}}
  };

//{{{
int main (int argc, char* argv[]) {

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    debug ("WSAStartup failed");
    exit (0);
    }
    //}}}

  startTimer();
  cMp3Window mp3Window;
  //mp3Window.run (L"mp3window", 800, 480, argv[1] ? std::string(argv[1]) : std::string());
  mp3Window.run (L"mp3window", 480, 272, argv[1] ? std::string(argv[1]) : std::string());
  }
//}}}
