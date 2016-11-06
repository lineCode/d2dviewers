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
#include "../../shared/widgets/cSelectBox.h"
#include "../../shared/widgets/cPicWidget.h"
#include "../../shared/widgets/cNumBox.h"
#include "../../shared/widgets/cBmpWidget.h"

#include "../../shared/decoders/cTinyJpeg.h"
#include "../../shared/decoders/cMp3Decoder.h"

#include "../../shared/icons/radioIcon.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#include "../common/cHttp.h"
//}}}
static const bool kAudio = true;

//{{{
class cHlsChan {
public:
  cHlsChan() {}
  virtual ~cHlsChan() {}

  // gets
  int getChan() { return mChan; }
  int getSampleRate() { return kSampleRate; }
  int getSamplesPerFrame() { return kSamplesPerFrame; }
  int getFramesPerChunk() { return kFramesPerChunk; }
  int getFramesFromSec (int sec) { return int (sec * getFramesPerSecond()); }
  float getFramesPerSecond() { return (float)kSampleRate / (float)kSamplesPerFrame; }

  int getLowBitrate() { return 48000; }
  int getMidBitrate() { return 128000; }
  int getHighBitrate() { return 320000; }

  int getBaseSeqNum() { return mBaseSeqNum; }

  std::string getHost() { return mHost; }
  std::string getTsPath (int seqNum, int bitrate) { return getPathRoot (bitrate) + '-' + toString (seqNum) + ".ts"; }
  std::string getChanName (int chan) { return kChanNames [chan]; }
  std::string getDateTime() { return mDateTime; }
  std::string getChanInfoStr() { return mChanInfoStr; }

  // set
  //{{{
  void setChan (cHttp& http, int chan) {

    mChan = chan;
    mHost = "as-hls-uk-live.bbcfmt.vo.llnwd.net";

    #ifdef STM32F769I_DISCO
      BSP_LED_On (LED2);
    #endif

    //cLcd::debug (mHost);
    if (http.get (mHost, getM3u8path()) == 302) {
      mHost = http.getRedirectedHost();
      http.get (mHost, getM3u8path());
      //cLcd::debug (mHost);
      }
    else
      mHost = http.getRedirectedHost();
    //cLcd::debug (getM3u8path());

    // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
    auto extSeq = strstr ((char*)http.getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
    auto extSeqEnd = strchr (extSeq, '\n');
    *extSeqEnd = '\0';
    mBaseSeqNum = atoi (extSeq) + 3;

    auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
    auto extDateTimeEnd = strchr (extDateTime, '\n');
    *extDateTimeEnd = '\0';
    mDateTime = extDateTime;

    mChanInfoStr = toString (http.getResponse()) + ' ' + http.getInfoStr() + ' ' + getM3u8path() + ' ' + mDateTime;
    //cLcd::debug (mDateTime + " " + cLcd::dec (mBaseSeqNum));
    }
  //}}}

private:
  //{{{  const
  const int kSampleRate = 48000;
  const int kSamplesPerFrame = 1024;
  const int kFramesPerChunk = 300;

  const int kPool        [7] = {      0,      7,      7,      7,      6,      6,      6 };
  const char* kChanNames [7] = { "none", "radio1", "radio2", "radio3", "radio4", "radio5", "radio6" };
  const char* kPathNames [7] = { "none", "bbc_radio_one", "bbc_radio_two", "bbc_radio_three",
                                         "bbc_radio_fourfm", "bbc_radio_five_live", "bbc_6music" };
  //}}}
  //{{{
  std::string getPathRoot (int bitrate) {
    return "pool_" + toString (kPool[mChan]) + "/live/" + kPathNames[mChan] + '/' + kPathNames[mChan] +
           ".isml/" + kPathNames[mChan] + "-audio=" + toString (bitrate);
    }
  //}}}
  std::string getM3u8path () { return getPathRoot (getMidBitrate()) + ".norewind.m3u8"; }

  // vars
  int mChan = 0;
  int mBaseSeqNum = 0;
  std::string mHost;
  std::string mDateTime;
  std::string mChanInfoStr;
  };
//}}}
//{{{
class cHlsChunk {
public:
  //{{{
  cHlsChunk() {
    mAudio = (int16_t*)pvPortMalloc (300 * 1024 * 2 * 2);
    mPower = (uint8_t*)pvPortMalloc (300 * 2);
    }
  //}}}
  //{{{
  ~cHlsChunk() {
    NeAACDecClose (mDecoder);
    if (mPower)
      vPortFree (mPower);
    if (mAudio)
      vPortFree (mAudio);
    }
  //}}}

  // gets
  int getSeqNum() { return mSeqNum; }
  int getBitrate() { return mBitrate; }
  int getFramesLoaded() { return mFramesLoaded; }
  bool getLoaded() { return mLoaded; }
  bool getLoading() { return mLoading; }
  std::string getInfoStr() { return mInfoStr; }

  int16_t* getSamples (int frameInChunk) { return (mAudio + (frameInChunk * mSamplesPerFrame * 2)); }
  //{{{
  uint8_t* getPower (int frameInChunk, int& frames) {
    frames = getFramesLoaded() - frameInChunk;
    return mPower + (frameInChunk * 2);
    }
  //}}}

  //{{{
  bool load (cHttp& http, cHlsChan* hlsChan, int seqNum, int bitrate) {

    auto ok = true;
    mLoading = true;

    mFramesLoaded = 0;
    mSeqNum = seqNum;
    mBitrate = bitrate;

    auto response = http.get (hlsChan->getHost(), hlsChan->getTsPath (seqNum, mBitrate));
    if (response == 200) {
      // aacHE has double size frames, treat as two normal frames
      int framesPerAacFrame = mBitrate <= 48000 ? 2 : 1;
      mSamplesPerFrame = hlsChan->getSamplesPerFrame();
      int samplesPerAacFrame = mSamplesPerFrame * framesPerAacFrame;

      auto loadPtr = http.getContent();
      auto loadEnd = http.getContentEnd();
      loadEnd = packTsBuffer (loadPtr, loadEnd);

      int16_t* buffer = mAudio;

      if (!mDecoder) {
        //{{{  create, init aac decoder
        mDecoder = NeAACDecOpen();

        NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
        config->outputFormat = FAAD_FMT_16BIT;
        NeAACDecSetConfiguration (mDecoder, config);

        unsigned long sampleRate;
        uint8_t chans;
        NeAACDecInit (mDecoder, loadPtr, 2048, &sampleRate, &chans);

        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
        }
        //}}}

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
        loadPtr += frameInfo.bytesconsumed;
        for (int i = 0; i < framesPerAacFrame; i++) {
          //{{{  calc left, right power
          int valueL = 0;
          int valueR = 0;
          for (int j = 0; j < mSamplesPerFrame; j++) {
            short sample = (*buffer++) >> 4;
            valueL += sample * sample;
            sample = (*buffer++) >> 4;
            valueR += sample * sample;
            }

          uint8_t leftPix = (uint8_t)sqrt(valueL / (mSamplesPerFrame * 32.0f));
          *powerPtr++ = (272/2) - leftPix;
          *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (mSamplesPerFrame * 32.0f));
          mFramesLoaded++;
          }
          //}}}
        }

      http.freeContent();
      mInfoStr = "ok " + toString (seqNum) + ':' + toString (bitrate/1000) + 'k';
      }
    else {
      mSeqNum = 0;
      mInfoStr = toString (response) + ':' + toString (seqNum) + ':' + toString (bitrate/1000) + "k " + http.getInfoStr();
      ok = false;

      }

    mLoaded = true;
    mLoading = false;
    return ok;
    }
  //}}}
  //{{{
  void invalidate() {
    mSeqNum = 0;
    mFramesLoaded = 0;
    mLoaded = false;
    }
  //}}}

private:
  //{{{
  uint8_t* packTsBuffer (uint8_t* ptr, uint8_t* endPtr) {
  // pack transportStream to aac frames in same buffer

    auto toPtr = ptr;
    while ((ptr < endPtr) && (*ptr++ == 0x47)) {
      auto payStart = *ptr & 0x40;
      auto pid = ((*ptr & 0x1F) << 8) | *(ptr+1);
      auto headerBytes = (*(ptr+2) & 0x20) ? (4 + *(ptr+3)) : 3;
      ptr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;
      if (pid == 34) {
        if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == 0xC0)) {
          int pesHeaderBytes = 9 + *(ptr+8);
          ptr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (toPtr, ptr, tsFrameBytesLeft);
        toPtr += tsFrameBytesLeft;
        }
      ptr += tsFrameBytesLeft;
      }

    return toPtr;
    }
  //}}}

  // vars
  NeAACDecHandle mDecoder = 0;
  int16_t* mAudio = nullptr;
  uint8_t* mPower = nullptr;

  int mSeqNum = 0;
  int mBitrate = 0;
  int mFramesLoaded = 0;
  int mSamplesPerFrame = 0;

  bool mLoaded = false;
  bool mLoading = false;
  std::string mInfoStr;
  };
//}}}
//{{{
class cHlsLoader : public cHlsChan {
public:
  cHlsLoader() {}
  virtual ~cHlsLoader() {}

  int getBitrate() { return mBitrate; }
  int getLoading() { return mLoading; }
  //{{{
  std::string getFrameStr (int frame) {

    int secsSinceMidnight = int (frame / getFramesPerSecond());
    int secs = secsSinceMidnight % 60;
    int mins = (secsSinceMidnight / 60) % 60;
    int hours = secsSinceMidnight / (60*60);

    return toString (hours) + ':' + toString (mins) + ':' + toString (secs);
    }
  //}}}
  //{{{
  std::string getInfoStr (int frame) {
    return getChanName (getChan()) + ':' + toString (getBitrate()/1000) + "k " + getFrameStr (frame);
    }
  //}}}
  //{{{
  std::string getChunkInfoStr (int chunk) {
    return getChunkNumStr (chunk) + ':' + mChunks[chunk].getInfoStr();
    }
  //}}}
  //{{{
  void getChunkLoad (int chunk, bool& loaded, bool& loading) {
    loaded = mChunks[chunk].getLoaded();
    loading = mChunks[chunk].getLoading();
    }
  //}}}

  //{{{
  uint8_t* getPower (int frame, int& frames) {
  // return pointer to frame power org,len uint8_t pairs
  // frames = number of valid frames

    int seqNum;
    int chunk = 0;
    int frameInChunk;
    frames = 0;
    return findFrame (frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getPower (frameInChunk, frames) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getSamples (int frame, int& seqNum) {
  // return audio buffer for frame

    int chunk;
    int frameInChunk;
    return findFrame (frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getSamples (frameInChunk) : nullptr;
    }
  //}}}

  //{{{
  int changeChan (int chan) {

    setChan (mHttp, chan);
    mBitrate = getMidBitrate();

    int hour = ((getDateTime()[11] - '0') * 10) + (getDateTime()[12] - '0');
    int min =  ((getDateTime()[14] - '0') * 10) + (getDateTime()[15] - '0');
    int sec =  ((getDateTime()[17] - '0') * 10) + (getDateTime()[18] - '0');
    int secsSinceMidnight = (hour * 60 * 60) + (min * 60) + sec;
    mBaseFrame = getFramesFromSec (secsSinceMidnight);

    invalidateChunks();
    return mBaseFrame;
    }
  //}}}
  void setBitrate (int bitrate) { mBitrate = bitrate; }

  //{{{
  bool load (int frame) {
  // return false if any load failed

    bool ok = true;

    mLoading = 0;
    int chunk;
    int seqNum = getSeqNumFromFrame (frame);

    if (!findSeqNumChunk (seqNum, mBitrate, 0, chunk)) {
      // load required chunk
      mLoading++;
      ok &= mChunks[chunk].load (mHttp, this, seqNum, mBitrate);
      }

    if (!findSeqNumChunk (seqNum, mBitrate, 1, chunk)) {
      // load chunk before
      mLoading++;
      ok &= mChunks[chunk].load (mHttp, this, seqNum+1, mBitrate);
      }

    if (!findSeqNumChunk (seqNum, mBitrate, -1, chunk)) {
      // load chunk after
      mLoading++;
      ok &= mChunks[chunk].load (mHttp, this, seqNum-1, mBitrate);
      }
    mLoading = 0;

    return ok;
    }
  //}}}

private:
  //{{{
  int getSeqNumFromFrame (int frame) {
  // works for -ve frame

    int r = frame - mBaseFrame;
    if (r >= 0)
      return getBaseSeqNum() + (r / getFramesPerChunk());
    else
      return getBaseSeqNum() - 1 - ((-r-1)/ getFramesPerChunk());
    }
  //}}}
  //{{{
  int getFrameInChunkFromFrame (int frame) {
  // works for -ve frame

    int r = (frame - mBaseFrame) % getFramesPerChunk();
    return r < 0 ? r + getFramesPerChunk() : r;
    }
  //}}}
  //{{{
  std::string getChunkNumStr (int chunk) {
    return mChunks[chunk].getSeqNum() ? toString (mChunks[chunk].getSeqNum() - getBaseSeqNum()) : "*";
    }
  //}}}

  //{{{
  bool findFrame (int frame, int& seqNum, int& chunk, int& frameInChunk) {
  // return true, seqNum, chunk and frameInChunk of loadedChunk from frame
  // - return false if not found

    auto findSeqNum = getSeqNumFromFrame (frame);
    for (auto i = 0; i < 3; i++) {
      if ((mChunks[i].getSeqNum() != 0) && (findSeqNum == mChunks[i].getSeqNum())) {
        auto findFrameInChunk = getFrameInChunkFromFrame (frame);
        if ((mChunks[i].getFramesLoaded() > 0) && (findFrameInChunk < mChunks[i].getFramesLoaded())) {
          seqNum = findSeqNum;
          chunk = i;
          frameInChunk = findFrameInChunk;
          return true;
          }
        }
      }

    seqNum = 0;
    chunk = 0;
    frameInChunk = 0;
    return false;
    }
  //}}}
  //{{{
  bool findSeqNumChunk (int seqNum, int bitrate, int offset, int& chunk) {
  // return true if match found
  // - if not, chunk = best reuse
  // - reuse same seqNum chunk if diff bitrate ?

    // look for matching chunk
    chunk = 0;
    while (chunk < 3) {
      if (seqNum + offset == mChunks[chunk].getSeqNum())
        return true;
        //return bitrate != mChunks[chunk].getBitrate();
      chunk++;
      }

    // look for stale chunk
    chunk = 0;
    while (chunk < 3) {
      if ((mChunks[chunk].getSeqNum() < seqNum-1) || (mChunks[chunk].getSeqNum() > seqNum+1))
        return false;
      chunk++;
      }

    chunk = 0;
    return false;
    }
  //}}}
  //{{{
  void invalidateChunks() {
    for (auto i = 0; i < 3; i++)
      mChunks[i].invalidate();
    }
  //}}}

  // private vars
  cHttp mHttp;
  cHlsChunk mChunks[3];
  int mBaseFrame = 0;
  int mBitrate = 0;
  int mLoading = 0;
  std::string mInfoStr;
  };
//}}}

static int mTuneChan = 1;
static bool mTuneChanChanged = false;
static cHlsLoader* mHlsLoader;
static HANDLE mSemaphore;

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

    if (kAudio) {
      bool mValueChanged = false;
      int mValue = 6;
      mRoot->addTopLeft (new cBmpWidget (r1x80, 1, mTuneChan, mValueChanged, 4, 4));
      mRoot->add (new cBmpWidget (r2x80, 2, mTuneChan, mValueChanged, 4, 4));
      mRoot->add (new cBmpWidget (r3x80, 3, mTuneChan, mValueChanged, 4, 4));
      mRoot->add (new cBmpWidget (r4x80, 4, mTuneChan, mValueChanged, 4, 4));
      mRoot->add (new cBmpWidget (r5x80, 5, mTuneChan, mValueChanged, 4, 4));
      mRoot->add (new cBmpWidget (r6x80, 6, mTuneChan, mValueChanged, 4, 4));
      mRoot->add (new cSelectBox ("radio7", 7, mTuneChan, mValueChanged, 3, 2));

      mChangeChannel = 4-1;
      mHlsLoader = new cHlsLoader();
      mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1

      // launch loaderThread
      std::thread ([=]() { loader(); } ).detach();

      // launch playerThread, higher priority
      auto playerThread = std::thread ([=]() { player(); });
      //SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
      playerThread.detach();


      //if (fileName.empty())
      //  mFileList.push_back ("C:/Users/colin/Desktop/Bread.mp3");
      //else if (GetFileAttributesA (fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
      //  listDirectory (std::string(), fileName, "*.mp3");
        //std::thread ([=]() { listThread (fileName ? fileName : "D:/music"); } ).detach();
      //else
      //  mFileList.push_back (fileName);
       //
      //mFramePosition = (int*)malloc (60*60*60*2*sizeof(int));
      //mWave = (uint8_t*)malloc (60*60*60*2*sizeof(uint8_t));
      //mVolume = 0.3f;
      //mSamples = (int16_t*)malloc (1152*2*2);
      //memset (mSamples, 0, 1152*2*2);
      //std::thread([=]() { audioThread(); }).detach();
      //std::thread([=]() { playThread(); }).detach();
      }

    else {
      listDirectory (std::string(), fileName.empty() ? "C:/Users/colin/Desktop/lamorna" : fileName, "*.jpg");
      std::thread([=]() { loadJpegThread(); }).detach();
      }


    messagePump();
    };
  //}}}
  //{{{  lcd interface
  uint16_t getWidthPix() { return mRoot->getWidthPix(); }
  uint16_t getHeightPix() { return mRoot->getHeightPix(); }

  uint8_t getFontHeight() { return 16; }
  uint8_t getLineHeight() { return 19; }
  //}}}
  //{{{  iDraw interface
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
    getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), width, height, &textLayout);

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

  //{{{
  void size (const uint8_t* src, uint8_t* dst, uint16_t components,
             uint16_t srcWidth, uint16_t srcHeight, uint16_t dstWidth, uint16_t dstHeight) {

    uint32_t xStep16 = ((srcWidth - 1) << 16) / (dstWidth - 1);
    uint32_t yStep16 = ((srcHeight - 1) << 16) / (dstHeight - 1);

    uint32_t ySrcOffset = srcWidth * components;

    for (uint32_t y16 = 0; y16 < dstHeight * yStep16; y16 += yStep16) {
      uint8_t yweight2 = (y16 >> 9) & 0x7F;
      uint8_t yweight1 = 0x80 - yweight2;
      const uint8_t* srcy = src + (y16 >> 16) * ySrcOffset;

      for (uint32_t x16 = 0; x16 < dstWidth * xStep16; x16 += xStep16) {
        uint8_t xweight2 = (x16 >> 9) & 0x7F;
        uint8_t xweight1 = 0x80 - xweight2;

        const uint8_t* srcy1x1 = srcy + (x16 >> 16) * components;
        const uint8_t* srcy1x2 = srcy1x1 + components;
        const uint8_t* srcy2x1 = srcy1x1 + ySrcOffset;
        const uint8_t* srcy2x2 = srcy2x1 + components;
        for (auto component = 0; component < components; component++)
          *dst++ = (((*srcy1x1++ * xweight1 + *srcy1x2++ * xweight2) * yweight1) +
                     (*srcy2x1++ * xweight1 + *srcy2x2++ * xweight2) * yweight2) >> 14;
        *dst++ = 0;
        }
      }
    }
  //}}}

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

    if (y + height > getHeightPix()) {
      // bottom yclip
      if (y >= getHeightPix())
        return;
      height = getHeightPix() - y;
      }

    stamp (colour, src, x, y, width, height);
    }
  //}}}
  //{{{
  void rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    if (x >= getWidthPix())
      return;
    if (y >= getHeightPix())
      return;

    int xend = x + width;
    if (xend <= 0)
      return;

    int yend = y + height;
    if (yend <= 0)
      return;

    if (x < 0)
      x = 0;
    if (xend > getWidthPix())
      xend = getWidthPix();

    if (y < 0)
      y = 0;
    if (yend > getHeightPix())
      yend = getHeightPix();

    if (!width)
      return;
    if (!height)
      return;

    rect (colour, x, y, xend - x, yend - y);
    }
  //}}}
  //{{{
  void rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t thickness) {

    rectClipped (colour, x, y, width, thickness);
    rectClipped (colour, x + width-thickness, y, thickness, height);
    rectClipped (colour, x, y + height-thickness, width, thickness);
    rectClipped (colour, x, y, thickness, height);
    }
  //}}}
  //{{{
  void clear (uint32_t colour) {

    rect (colour, 0, 0, getWidthPix(), getHeightPix());
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
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x10: // shift
      case 0x11: // control
      case 0x00: return false;
      case 0x1B: return true; // escape
      case 0x20: mPlaying = ! mPlaying; return false; // space

      case 0x26: mFileIndex--; mFileIndexChanged = true; break; // up arrow
      case 0x28: mFileIndex++; mFileIndexChanged = true; break; // down arrow

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

    int fileIndex = 0;
    bool fileIndexChanged = false;

    mPlayFrame = 0;
    bool mFrameChanged = false;;
    mRoot->addAt (new cListWidget (mFileList, fileIndex, fileIndexChanged, mRoot->getWidth(), mRoot->getHeight() - 13), 0, 4);
    mRoot->add (new cWaveCentreWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, mRoot->getWidth(), 3));
    mRoot->add (new cWaveWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, mRoot->getWidth(), 3));
    mRoot->add (new cWaveLensWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged, mRoot->getWidth(), 3));

    bool mVolumeChanged = false;
    mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, 1, mRoot->getHeight()-6));

    cMp3Decoder mMp3Decoder;
    while (true) {
      //{{{  open and load file into fileBuffer
      auto fileHandle = CreateFileA (mFileList[fileIndex].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
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
          while (mWait && !fileIndexChanged)
            Sleep (2);
          mPlayFrame++;
          }

        if (mWaveChanged) {
          filePosition = mFramePosition [mPlayFrame];
          mWaveChanged = false;
          }
        } while (!fileIndexChanged && (bytesUsed > 0) && (filePosition < mFileSize));

      if (!fileIndexChanged)
        fileIndex = (fileIndex + 1) % mFileList.size();
      fileIndexChanged = false;
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
    mFileIndex = 0;

    bool mPicValueChanged = false;

    mRoot->addTopRight (new cNumBox ("list ", mCount, mCountChanged, 6.0f));
    mRoot->addBelow (new cNumBox ("show ", mNumWidget, mNumChanged, 6.0f));

    mRoot->addTopLeft (new cListWidget (mFileList, mFileIndex, mFileIndexChanged, mRoot->getWidth(), 10));

    for (auto i = 0; i < 18; i++) {
      auto picWidget = new cPicWidget (8.0f, 6.0f, i, mFileIndex, mFileIndexChanged);
      mRoot->add (picWidget);
      mPicWidgets.push_back(picWidget);
      }

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

  //{{{
  void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    CoInitialize (NULL);

    cHttp http;
    while (true) {
      if (mTuneChan != mChangeChannel) {
        mPlayFrame = mHlsLoader->changeChan (mChangeChannel);
        mTuneChan = mChangeChannel;
        }

      if (!mHlsLoader->load (mPlayFrame)) {
        Sleep (1000);
        }
      mHttpRxBytes = http.getRxBytes();
      wait();
      }

    CoUninitialize();
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);
    audOpen (48000, 16, 2);

    auto lastSeqNum = 0;
    while (true) {
      int seqNum;
      auto audSamples = mHlsLoader->getSamples (mPlayFrame, seqNum);
      audPlay (audSamples, 4096, 1.0f);

      if (audSamples)
        mPlayFrame++;

      if (!seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        signal();
        }
      }

    audClose();
    CoUninitialize();
    }
  //}}}
  //{{{
  void wait() {
    WaitForSingleObject (mSemaphore, 20 * 1000);
    }
  //}}}
  //{{{
  void signal() {
    ReleaseSemaphore (mSemaphore, 1, NULL);
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
             ((jpegDecoder.getWidth() / scale > getWidthPix()) || (jpegDecoder.getHeight() /scale > getHeightPix()))) {
        scale *= 2;
        scaleShift++;
        }

      mPicWidgets [int(mNumWidget) % mPicWidgets.size()]->setPic (
        jpegDecoder.decodeBody (scaleShift), jpegDecoder.getWidth() >> scaleShift, jpegDecoder.getHeight() >> scaleShift, 4);
      mPicWidgets [int(mNumWidget) % mPicWidgets.size()]->setMyValue (fileIndex);
      mNumWidget++;
      }
    }
  //}}}

  //{{{  vars
  cRootContainer* mRoot = nullptr;

  int mFileIndex = 0;
  bool mFileIndexChanged = false;
  std::vector <std::string> mFileList;
  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  uint8_t* mFileBuffer = nullptr;
  int mFileSize = 0;

  int mPlayFrame = 0;
  int mLoadedFrame = 0;
  int mMaxFrame = 0;
  bool mWaveChanged = false;
  uint8_t* mWave = nullptr;
  int* mFramePosition = nullptr;

  float mCount = 0;
  bool mCountChanged = false;
  float mNumWidget = 0;
  bool mNumChanged = false;

  int16_t* mSamples = nullptr;
  bool mWait = false;
  bool mReady = false;
  bool mPlaying = true;

  ID2D1SolidColorBrush* mBrush;
  std::vector <cPicWidget*> mPicWidgets;

  int mChangeChannel = 0;
  int mHttpRxBytes = 0;
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
  mp3Window.run (L"mp3window", 480*2, 272*2, argv[1] ? std::string(argv[1]) : std::string());
  }
//}}}
