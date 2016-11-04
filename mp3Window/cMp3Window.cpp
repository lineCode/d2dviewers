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
#include "../../shared/widgets/cWaveCentreWidget.h"
#include "../../shared/widgets/cWaveOverviewWidget.h"
#include "../../shared/widgets/cWaveLensWidget.h"
#include "../../shared/widgets/cTextBox.h"
#include "../../shared/widgets/cValueBox.h"
#include "../../shared/widgets/cSelectBox.h"
#include "../../shared/widgets/cPicWidget.h"
#include "../../shared/widgets/cBitmapWidget.h"
#include "../../shared/widgets/cNumBox.h"
#include "../../shared/widgets/cSelectBmpWidget.h"

#include "../../shared/decoders/cTinyJpeg.h"
#include "../../shared/decoders/cMp3Decoder.h"

#include "../../shared/icons/radioIcon.h"
//}}}
static const bool kAudio = true;

//{{{
class cFileMapTinyJpeg : public cTinyJpeg {
public:
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
      if (GetFileAttributesA (fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
        listDirectory (std::string(), fileName.empty() ? "D:/music" : fileName, "*.mp3");
        //std::thread ([=]() { listThread (fileName ? fileName : "D:/music"); } ).detach();
      else
        mFileList.push_back (fileName);

      mFramePosition = (int*)malloc (60*60*60*2*sizeof(int));
      mWave = (uint8_t*)malloc (60*60*60*2*sizeof(uint8_t));
      mVolume = 0.3f;
      mSamples = (int16_t*)malloc (1152*2*2);
      memset (mSamples, 0, 1152*2*2);
      std::thread([=]() { audioThread(); }).detach();
      std::thread([=]() { playThread(); }).detach();
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
      for (auto j = y; j < y + height; j++)
        for (auto i = x; i < x + width; i++) {
          uint32_t colour = *((uint32_t*)src);
          mBrush->SetColor (ColorF (colour));
          getDeviceContext()->FillRectangle (RectF (float(i), float(j), float(i + 1), float(j + 1)), mBrush);
          src++;
          }
      }
    }
  //}}}
  void copy (uint8_t* src, int16_t srcx, int16_t srcy, uint16_t srcWidth, int16_t srcHeight,
             int16_t dstx, int16_t dsty, uint16_t dstWidth, uint16_t dstHeight) {};
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
    bool mVolumeChanged = false;
    bool mFrameChanged = false;;
    mPlayFrame = 0;

    bool mValueChanged = false;
    int mValue = 6;

    bool mTuneChanChanged = false;
    int mTuneChan = 6;

    mRoot->addTopLeft (new cListWidget (mFileList, fileIndex, fileIndexChanged,
                                        mRoot->getWidth(), mRoot->getHeight() - 9));
    mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW,
                                       1 , mRoot->getHeight()-6));
    mRoot->addBottomLeft (new cWaveLensWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged,
                                               mRoot->getWidth(), 3));
    mRoot->addAbove (new cWaveWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged,
                                          mRoot->getWidth(), 3));
    mRoot->addAbove (new cWaveCentreWidget (mWave, mPlayFrame, mLoadedFrame, mMaxFrame, mWaveChanged,
                                                mRoot->getWidth(), 3));

    mRoot->addTopLeft (new cSelectBox ("radio4", 4, mValue, mValueChanged, 3, 2));
    mRoot->add (new cSelectBox ("radio5", 5, mValue, mValueChanged, 3, 2));
    mRoot->add (new cSelectBox ("radio6", 6, mValue, mValueChanged, 3, 2));

    mRoot->add (new cSelectBmpWidget (r1x80, 1, mTuneChan, mTuneChanChanged, 3, 3));
    mRoot->add (new cSelectBmpWidget (r2x80, 1, mTuneChan, mTuneChanChanged, 3, 3));

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

    uint32_t* piccy = nullptr;
    uint16_t picWidth = 0;
    uint16_t picHeight = 0;
    mFileIndex = 0;

    for (auto j = 0; j < 4; j++)
      for (auto i = 0; i < 6; i++)
        mBitmapWidgets.push_back (mRoot->addAtPix (new cBitmapWidget (160, 120), i * 160, j * mRoot->getHeightPix()/4));
    mRoot->addTopLeft (new cListWidget (mFileList, mFileIndex, mFileIndexChanged, mRoot->getWidth(), mRoot->getHeight()));
    mRoot->addTopRight (new cNumBox ("list ", mCount, mCountChanged, 100));
    mRoot->add (new cNumBox ("show ", mNumWidget, mNumChanged, 100));

    while (!mFileIndexChanged && (mFileIndex < mFileList.size()-1)) {
      jpegDecode (mFileList[mFileIndex].c_str(), piccy, picWidth, picHeight);
      mFileIndex++;
      }

    mFileIndexChanged = true;
    while (true) {
      if (mFileIndexChanged) {
        jpegDecode (mFileList[mFileIndex].c_str(), piccy, picWidth, picHeight);
        mFileIndexChanged = false;
        }
      else
        Sleep (100);
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
  void jpegDecode (std::string fileName, uint32_t*& pic, uint16_t& picWidth, uint16_t& picHeight) {

    cFileMapTinyJpeg jpegDecoder;
    if (jpegDecoder.readHeader (fileName)) {
      // scale to fit
      auto scale = 1;
      auto scaleShift = 0;
      while ((scaleShift < 3) &&
             ((jpegDecoder.getWidth() / scale > getWidthPix()) || (jpegDecoder.getHeight() /scale > getHeightPix()))) {
        scale *= 2;
        scaleShift++;
        }

      ID2D1Bitmap* bitmap;
      auto picWidth = jpegDecoder.getWidth() >> scaleShift;
      auto picHeight = jpegDecoder.getHeight() >> scaleShift;
      getDeviceContext()->CreateBitmap (SizeU (picWidth, picHeight), getBitmapProperties(), &bitmap);
      auto pic = jpegDecoder.decodeBody (scaleShift);
      bitmap->CopyFromMemory (&RectU (0, 0, picWidth, picHeight), pic, picWidth*4);
      free (pic);

      ((cBitmapWidget*)mBitmapWidgets[(int(mNumWidget++)) % 24])->setPic (bitmap);
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
  std::vector <cWidget*> mBitmapWidgets;
  //}}}
  };

//{{{
int main (int argc, char* argv[]) {
  startTimer();
  cMp3Window mp3Window;
  mp3Window.run (L"mp3window", 480*2, 272*2, argv[1] ? std::string(argv[1]) : std::string());
  }
//}}}
