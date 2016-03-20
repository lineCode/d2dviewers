// cSensorWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2DWindow.h"

#include "../common/usbUtils.h"
#include "../common/jpegHeader.h"

#include "../inc/jpeglib/jpeglib.h"
#pragma comment (lib,"turbojpeg-static")
//}}}
//{{{  static const
static const D2D1_BITMAP_PROPERTIES kBitmapProperties = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 96.0f, 96.0f };
//}}}

class cSensorWindow : public cD2dWindow {
public:
  cSensorWindow() {}
  virtual ~cSensorWindow() {}
  //{{{
  void run() {

    initialise (L"sensorWindow", 800, 600);

    // samples
    mSamples = (uint8_t*)malloc (mMaxSamples);
    mMaxSamplePtr = mSamples + mMaxSamples;
    setSamplesPerPixel (mMaxSamplesPerPixel);

    initUSB();

    mSensorId = readReg (0x3000);
    if (mSensorId != 0x1519) {
      writeReg (0xF0, 0);
      mSensorId = readReg (0x00);
      wcout << L"sensorid " << mSensorId << endl;
      }

    if (mSensorId == 0x1519)
      pll (16, 1, 1);

    preview();

    auto loaderThread = thread([=]() { loader(); });
    SetThreadPriority (loaderThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    loaderThread.detach();

    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x10: // shift
      case 0x11: // control
      case 0x00: return false;
      case 0x1B: return true; // escape abort

      case 0x21: setSamplesPerPixel (mSamplesPerPixel /= 2.0); break; // page up
      case 0x22: setSamplesPerPixel (mSamplesPerPixel *= 2.0); break; // page down

      case 0x23: setMidSample (mSamplesLoaded); break; // end
      case 0x24: setMidSample (0.0); break;                         // home

      case 0x25: setMidSample (mMidSample - (getClientF().width/4.0) * mSamplesPerPixel); break; // left arrow
      case 0x27: setMidSample (mMidSample + (getClientF().width/4.0) * mSamplesPerPixel); break; // right arrow

      case 0x26:
        //{{{  up arrow
        if (mSamplesPerPixel > 1.0)
          setMidSample (mMidSample - (mControlKeyDown ? 4.0 : mShiftKeyDown ? 2.0 : 1.0) * mSamplesPerPixel);
        else if (floor (mMidSample) != mMidSample)
          setMidSample (floor (mMidSample));
        else
          setMidSample (mMidSample - 1.0);
        break;
        //}}}
      case 0x28:
        //{{{  down arrow
        if (mSamplesPerPixel > 1.0)
          setMidSample (mMidSample + (mControlKeyDown ? 4.0 : mShiftKeyDown ? 2.0 : 1.0) * mSamplesPerPixel);
        else if (ceil (mMidSample) != mMidSample)
          setMidSample (mMidSample);
        else
          setMidSample (mMidSample + 1.0);
        break;
        //}}}

      case 'O': mWaveform = !mWaveform; changed(); break;
      case 'H': mHistogram = !mHistogram; changed(); break;
      case 'V': mVector = !mVector; changed(); break;

      case 'B': setFocus (--mFocus); break;
      case 'N': setFocus (++mFocus); break;

      case 'Q': preview(); break;
      case 'W': capture(); break;
      case 'E': bayer(); break;
      case 'J': jpeg(); break;

      case 'A': pll (++pllm, plln, pllp); break;
      case 'Z': pll (--pllm, plln, pllp); break;
      case 'S': pll (pllm, ++plln, pllp); break;
      case 'X': pll (pllm, --plln, pllp); break;
      case 'D': pll (pllm, plln, ++pllp); break;;
      case 'C': pll (pllm, plln, --pllp); break;
      case 'R': pll (80, 11, 0); break;  // 80 Mhz
      case 'T': pll (16, 1, 1); break;  // 48 Mhz

      default: wcout << L"key:" << key << endl;
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    double ratio = mControlKeyDown ? 1.5 : mShiftKeyDown ? 1.25 : 2.0;
    if (delta > 0)
      ratio = 1.0/ratio;

    setSamplesPerPixel (mSamplesPerPixel * ratio);
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    ULONG mask;
    if (getMask (x, y, mask))
      measure (mask, int(mMidSample + (x - getClientF().width/2.0f) * mSamplesPerPixel));
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (mControlKeyDown)
      setFocus (mFocus + xInc);

    else if (right) {
      ULONG mask = 0;
      if (getMask (mDownMousex, mDownMousey, mask))
        count (mask,
               int(mMidSample + (mDownMousex - getClientF().width/2.0f) * mSamplesPerPixel),
               int(mMidSample + (x - getClientF().width/2.0f) * mSamplesPerPixel));
      }
    else
      setMidSample (mMidSample - (xInc * mSamplesPerPixel));
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!right)
      if (!mouseMoved)
        setMidSample (mMidSample + (x - getClientF().width/2.0) * mSamplesPerPixel);
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    drawBackground (dc, mFramePtr);
    drawSamplesFramesTitle (dc);
    drawSensor (dc, mSensorId);

    if (mWaveform) {
      drawLeftLabels (dc, 8);
      drawSamples (dc, 0x100);
      drawGraticule (dc, 8);
      drawMidLine (dc, 8);
      }

    if (!mBayer10 && !mJpeg422 && mHistogram)
      drawLumaHistogram (dc);

    if (!mBayer10 && !mJpeg422 && mVector)
      drawChromaVector (dc);
    }
  //}}}

private:
  //{{{
  void drawBackground (ID2D1DeviceContext* dc, uint8_t* framePtr) {

    if (framePtr && (framePtr != mLastFramePtr)) {
      mGrabbedFrameBytes = mFrameBytes;
      makeBitmap (framePtr, mFrameBytes);
      framePtr = mLastFramePtr;
      }
    if (mBitmap)
      dc->DrawBitmap (mBitmap, RectF(0,0, getClientF().width, getClientF().height));
    else
      dc->Clear (ColorF(ColorF::Black));
    }
  //}}}
  //{{{
  void drawSamplesFramesTitle (ID2D1DeviceContext* dc) {

    wstringstream str;
    str << L" fs:" << mFrames / getTimer()
        << L" bytes:" << mGrabbedFrameBytes
        << L" w:" << mWidth
        << L" h:" << mHeight
        << L" focus:" << mFocus;
    dc->DrawText (str.str().data(), (uint32_t)str.str().size(), getTextFormat(),
                  RectF(mLeftPixels, 0, getClientF().width, getClientF().height), getWhiteBrush());
    }
  //}}}
  //{{{
  void drawSensor (ID2D1DeviceContext* dc, int sensorid) {

    if (sensorid == 0x1519) {
      wstring wstr = L"Mt9d111";
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(),
                    RectF(getClientF().width - 2*mRightPixels, 0, getClientF().width, mRowPixels), getWhiteBrush());
      }
    }
  //}}}
  //{{{
  void drawLeftLabels (ID2D1DeviceContext* dc, int rows) {

    auto r = RectF(0.0f, mRowPixels, getClientF().width, getClientF().height);
    for (auto dq = 0; dq < rows; dq++) {
      wstring wstr = L"dq" + to_wstring (dq);
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), r, getWhiteBrush());
      r.top += mRowPixels;
      }
    }
  //}}}
  //{{{
  void drawSamples (ID2D1DeviceContext* dc, ULONG maxMask) {

    auto r = RectF (mLeftPixels, mRowPixels, 0, 0);
    auto lastSampleIndex = 0;
    for (auto j = int(mLeftPixels - getClientF().width/2.0f); j < int(getClientF().width/2.0f); j++) {
      r.right = r.left + 1.0f;

      int sampleIndex = int(mMidSample + (j * mSamplesPerPixel));
      if ((sampleIndex >= 0) && (sampleIndex < mSamplesLoaded)) {
        ULONG transition = 0;
        //{{{  set transition bitMask for samples spanned by this pixel
        if (lastSampleIndex && (lastSampleIndex != sampleIndex)) {
          // look for transition from lastSampleIndex to sampleIndex+1
          if (sampleIndex - lastSampleIndex <= 32) {
            // use all samples
            for (int index = lastSampleIndex; index < sampleIndex; index++)
              transition |= mSamples[index % mMaxSamples] ^ mSamples[(index+1) % mMaxSamples];
            }
          else {
            // look at some samples for transition
            int indexInc = (sampleIndex - lastSampleIndex) / 13;
            for (int index = lastSampleIndex; (index + indexInc <= sampleIndex)  && (index + indexInc < mSamplesLoaded - 1); index += indexInc)
              transition |= mSamples[index % mMaxSamples] ^ mSamples[(index+indexInc) % mMaxSamples];
            }
          }
        lastSampleIndex = sampleIndex;
        //}}}

        r.top = mRowPixels;
        ULONG mask = 1;
        while ((mask != maxMask) && (r.top < getClientF().height)) {
          //{{{  draw rows for sample at this pixel
          auto nextTop = r.top + ((mask & 0x80808080) ? groupRowPixels : mRowPixels);

          if (transition & mask)
            r.bottom = r.top + mBarPixels;
          else {
            if (!(mSamples[sampleIndex % mMaxSamples] & mask)) // lo
              r.top += mBarPixels - 1.0f;
            r.bottom = r.top + 1.0f;
            }

          dc->FillRectangle (r, getBlueBrush());

          r.top = nextTop;

          mask <<= 1;
          }
          //}}}
        }

      r.left = r.right;
      }
    }
  //}}}
  //{{{
  void drawGraticule (ID2D1DeviceContext* dc, int rows) {

    dc->DrawText (mGraticuleStr.data(), (uint32_t)mGraticuleStr.size(), getTextFormat(),
                  RectF (0, 0, mLeftPixels, mRowPixels), getWhiteBrush());

    auto rg = RectF (0, mRowPixels, 0, (rows+1)*mRowPixels);
    auto leftSample = mMidSample - ((getClientF().width/2.0f - mLeftPixels) * mSamplesPerPixel);
    auto graticule = int(leftSample + (mSamplesPerGraticule - 1.0)) / (int)mSamplesPerGraticule;
    auto graticuleSample = graticule * mSamplesPerGraticule;

    auto more = true;
    while (more) {
      more = graticuleSample <= mMaxSamples;
      if (!more)
        graticuleSample = (double)mMaxSamples;

      rg.left = float((graticuleSample - mMidSample) / mSamplesPerPixel) + getClientF().width/2.0f;
      rg.right = rg.left+1;
      if (graticule > 0)
        dc->FillRectangle (rg, getGreyBrush());

      graticule++;
      graticuleSample += mSamplesPerGraticule;

      rg.left = rg.right;
      more &= rg.left < getClientF().width;
      }
    }
  //}}}
  //{{{
  void drawMidLine (ID2D1DeviceContext* dc, int rows) {

    dc->FillRectangle (RectF(getClientF().width/2.0f, mRowPixels, getClientF().width/2.0f + 1.0f, (rows+1)*mRowPixels),
                       getBlackBrush());
    }
  //}}}
  //{{{
  void drawLumaHistogram (ID2D1DeviceContext* dc) {

    auto r = RectF (10.0f, 0, 0, getClientF().height - 10.0f);

    auto inc = 1.0f;
    for (auto i = 0; i < mBucket; i++)
      inc *= 2.0f;

    for (auto i = 0; i < 0x100 >> mBucket; i++) {
      r.right = r.left + inc;
      r.top = r.bottom - mHistogramValues[i] - 1;
      dc->FillRectangle (r, getWhiteBrush());
      r.left = r.right;
      }
    }
  //}}}
  //{{{
  void drawChromaVector (ID2D1DeviceContext* dc) {

    dc->FillRectangle (RectF (getClientF().width - 128.0f, getClientF().height - 256.0f,
                       getClientF().width - 128.0f + 1.0f, getClientF().height), getBlackBrush());
    dc->FillRectangle (RectF (getClientF().width - 256.0f, getClientF().height - 128.0f,
                       getClientF().width, getClientF().height - 128.0f - 1.0f), getBlackBrush());

    auto r = RectF (getClientF().width - 256.0f, getClientF().height - 256.0f, 0, 0);
    auto index = 0;
    for (auto j = 0; j < 0x80; j++) {
      r.bottom = r.top + 2.0f;
      for (int i = 0; i < 0x80; i++) {
        r.right = r.left + 2.0f;
        if (mVectorValues[index++])
          dc->FillRectangle (r, getWhiteBrush());
        r.left = r.right;
        }
      r.left = getClientF().width - 256.0f;
      r.top = r.bottom;
      }
    }
  //}}}

  //{{{  samples
  //{{{
  void setSamplesPerPixel (double newSamplesPerPixel) {
  // set display window scale, samplesPerPixel

    if (newSamplesPerPixel < mMinSamplesPerPixel)
      mSamplesPerPixel = mMinSamplesPerPixel;
    else if (newSamplesPerPixel > mMaxSamplesPerPixel)
      mSamplesPerPixel = mMaxSamplesPerPixel;
    else
      mSamplesPerPixel = newSamplesPerPixel;

    mSamplesPerGraticule = 10.0;
    while (mSamplesPerGraticule < (100.0 * mSamplesPerPixel))
      mSamplesPerGraticule *= 10.0;

    wstringstream stringStream;
    if (mSamplesPerGraticule <= 10.0)
      stringStream << int (mSamplesPerGraticule / 0.1) << L"ns";
    else if (mSamplesPerGraticule <= 10000.0)
      stringStream << int (mSamplesPerGraticule / 100) << L"us";
    else if (mSamplesPerGraticule <= 10000000.0)
      stringStream << int (mSamplesPerGraticule / 100000) << L"ms";
    else if (mSamplesPerGraticule <= 10000000000.0)
      stringStream << int (mSamplesPerGraticule / 100000000) << L"s";
    else
      stringStream << int (mSamplesPerGraticule / 6000000000) << L"m";
    mGraticuleStr = stringStream.str();

    changed();
    }
  //}}}
  //{{{
  void setMidSample (double newMidSample) {
  // set sample of pixel in middle of display window

    if (newMidSample < 0.0)
      mMidSample = 0.0;
    else if (newMidSample > (double)mSamplesLoaded)
      mMidSample = (double)mSamplesLoaded;
    else
      mMidSample = newMidSample;

    changed();
    }
  //}}}
  //{{{
  void incSamplesLoaded (int packetLen) {

    bool tailing = (mMidSample == mSamplesLoaded);
    mSamplesLoaded += packetLen;
    if (tailing)
      setMidSample (mSamplesLoaded);
    }
  //}}}
  //}}}
  //{{{  measure
  //{{{
  bool getMask (int x, int y, ULONG& mask) {

    float row = mRowPixels;

    mask = 0x00000001;
    while ((mask != 0x10000) && (row < getClientF().height)) {
     float  nextRow = row + ((mask & 0x80808080) ? groupRowPixels : mRowPixels);
      if ((y > row) && (y < nextRow))
        return true;

      row = nextRow;
      mask <<= 1;
      }
    return false;
    }
  //}}}
  //{{{
  void measure (ULONG mask, int sample) {

    wstringstream stringStream;

    if ((sample > 0) && (sample < mSamplesLoaded)) {
      int back = sample;
      while ((back > 0) && !((mSamples[back] ^ mSamples[back - 1]) & mask) && ((sample - back) < 100000000))
        back--;

      int forward = sample;
      while ((forward < (mSamplesLoaded - 1)) &&
             !((mSamples[forward] ^ mSamples[forward + 1]) & mask) && ((forward - sample) < 100000000))
        forward++;

      if (((sample - back) != 100000000) && ((forward - sample) != 100000000)) {
        double width = (forward - back) / mSamplesPerSecond;

        if (width > 1.0)
          stringStream << width << L"s";
        else if (width > 0.001)
          stringStream << width*1000.0<< L"ms";
        else if (width > 0.000001)
          stringStream << width*1000000.0<< L"us";
        else
          stringStream << width*1000000000.0<< L"ns";
        changed();
        }
      }

    mMeasureStr = stringStream.str();
    }
  //}}}
  //{{{
  void count (ULONG mask, int firstSample, int lastSample) {

    int count = 0;
    int sample = firstSample;
    if ((sample > 0) && (sample < mSamplesLoaded)) {
      while ((sample < lastSample) && (sample < (mSamplesLoaded -2))) {
        if ((mSamples[sample] ^ mSamples[sample + 1]) & mask)
          count++;
        sample++;
        }

      if (count > 0) {
        wstringstream stringStream;
        stringStream << count;
        mMeasureStr = stringStream.str();
        changed();
        }
      }
    }
  //}}}
  //}}}
  //{{{
  uint8_t limit (double v) {

    if (v <= 0.0)
      return 0;

    if (v >= 255.0)
      return 255;

    return (uint8_t)v;
    }
  //}}}
  //{{{
  void makeBitmap (uint8_t* frameBuffer, int frameBytes) {

    // create bitmap from frameBuffer
    auto bufferLen = mWidth * mHeight * 4;
    auto bufferAlloc = (uint8_t*)malloc (bufferLen);
    auto buffer = bufferAlloc;

    if (mJpeg422) {
      if ((frameBytes != 800*600*2) && (frameBytes != 1600*800*2) && (frameBytes != 1600*800)) {
        //{{{  decode jpeg
        auto endPtr = frameBuffer + frameBytes - 4;

        int jpegBytes = *endPtr++;
        jpegBytes += (*endPtr++) << 8;
        jpegBytes += (*endPtr++) << 16;

        auto status = *endPtr;
        if ((status & 0x0f) == 0x01) {
          cinfo.err = jpeg_std_error (&jerr);
          jpeg_create_decompress (&cinfo);
          jpegHeaderBytes = setJpegHeader (jpegHeader, mWidth, mHeight, 0, 6);

          jpeg_mem_src (&cinfo, jpegHeader, jpegHeaderBytes);
          jpeg_read_header (&cinfo, TRUE);
          cinfo.out_color_space = JCS_EXT_BGRA;

          //wraparound problem if (frame >= maxSamplePtr)  frame = samples;
          frameBuffer[jpegBytes] = 0xff;
          frameBuffer[jpegBytes+1] = 0xd9;
          jpeg_mem_src (&cinfo, frameBuffer, jpegBytes+2);
          jpeg_start_decompress (&cinfo);

          while (cinfo.output_scanline < cinfo.output_height) {
            unsigned char* bufferArray[1];
            bufferArray[0] = buffer + (cinfo.output_scanline * cinfo.output_width * cinfo.output_components);
            jpeg_read_scanlines(&cinfo, bufferArray, 1);
            }

          jpeg_finish_decompress (&cinfo);
          jpeg_destroy_decompress (&cinfo);
          }
        else
          printf ("err status %x %d %d\n", status, frameBytes, jpegBytes);
        }
        //}}}
      }
    else if (mBayer10) {
      //{{{  bayer
      //  g r    right to left
      //  b g
      int x = 0;
      int y = 0;

      auto bufferEnd = buffer + bufferLen;
      while (buffer < bufferEnd) {
        if (frameBuffer >= mMaxSamplePtr)
          frameBuffer = mSamples;

        if ((y & 0x01) == 0) {
          // even lines
          *buffer++ = 0;
          *buffer++ = *frameBuffer++;; // g1
          *buffer++ = 0;
          *buffer++ = 255;

          *buffer++ = 0;
          *buffer++ = 0;
          *buffer++ = *frameBuffer++;  // r
          *buffer++ = 255;
          }

        else {
          // odd lines
          *buffer++ = *frameBuffer++; // b
          *buffer++ = 0;
          *buffer++ = 0;
          *buffer++ = 255;

          *buffer++ = 0;
          *buffer++ = *frameBuffer++; // g2
          *buffer++ = 0;
          *buffer++ = 255;
          }
        x += 2;

        if (x >= mWidth) {
          y++;
          x = 0;
          }
        }
      }
      //}}}
    else {
      //{{{  yuv
      if (mHistogram)
        for (auto i = 0; i < 0x100; i++)
          mHistogramValues[i] = 0;

      if (mVector)
        for (auto i = 0; i < 0x4000; i++)
          mVectorValues[i] = false;

      auto bufferEnd = buffer + bufferLen;
      while (buffer < bufferEnd) {
        if (frameBuffer >= mMaxSamplePtr)
          frameBuffer = mSamples;

        int y1 = *frameBuffer++;
        int u = *frameBuffer++;
        int y2 = *frameBuffer++;
        int v = *frameBuffer++;

        if (mHistogram) {
          mHistogramValues [y1 >> mBucket]++;
          mHistogramValues [y2 >> mBucket]++;
          }

        if (mVector)
          mVectorValues [((v << 6) & 0x3F80) | (u >> 1)] = true;

        u -= 128;
        v -= 128;

        *buffer++ = limit (y1 + (1.8556 * u));
        *buffer++ = limit (y1 - (0.1873 * u) - (0.4681 * v));
        *buffer++ = limit (y1 + (1.5748 * v));
        *buffer++ = 255;

        *buffer++ = limit (y2 + (1.8556 * u));
        *buffer++ = limit (y2 - (0.1873 * u) - (0.4681 * v));
        *buffer++ = limit (y2 + (1.5748 * v));
        *buffer++ = 255;
        }

      if (mHistogram) {
        int maxHistogram = 0;
        for (auto i = 0; i < (0x100 >> mBucket) - 1; i++)
          if (mHistogramValues[i] > maxHistogram)
            maxHistogram = mHistogramValues[i];
        for (auto i = 0; i < 0x100 >> mBucket; i++)
          mHistogramValues[i] = (mHistogramValues[i] << 8) / maxHistogram;
        }
      }
      //}}}

    if (mBitmap) {
      auto pixelSize = mBitmap->GetPixelSize();
      if ((pixelSize.width != mWidth) || (pixelSize.height != mHeight)) {
        mBitmap->Release();
        mBitmap = nullptr;
        }
      }
    if (!mBitmap)
      getDeviceContext()->CreateBitmap (SizeU(mWidth, mHeight), kBitmapProperties, &mBitmap);

    mBitmap->CopyFromMemory (&RectU (0, 0, mWidth, mHeight), bufferAlloc, mWidth*4);
    free (bufferAlloc);
    }
  //}}}

  //{{{
  void pll (int m, int n, int p) {

    pllm = m;
    plln = n;
    pllp = p;

    if (mSensorId == 0x1519) {
      printf ("pll m:%d n:%d p:%d Fpfd(2-13):%d Fvco(110-240):%d Fout(6-80):%d\n",
              pllm, plln, pllp,
              24000000 / (plln+1),
              (24000000 / (plln+1))*pllm,
              ((24000000 / (plln+1) * pllm)/ (2*(pllp+1))) );

      //  PLL (24mhz/(N+1))*M / 2*(P+1)
      writeReg (0x66, (pllm << 8) | plln);  // PLL Control 1    M:15:8,N:7:0 - M=16, N=1 (24mhz/(n1+1))*m16 / 2*(p1+1) = 48mhz
      writeReg (0x67, 0x0500 | pllp);  // PLL Control 2 0x05:15:8,P:7:0 - P=3
      writeReg (0x65, 0xA000);  // Clock CNTRL - PLL ON
      writeReg (0x65, 0x2000);  // Clock CNTRL - USE PLL
      }
    }
  //}}}
  //{{{
  void capture() {

    wcout << L"capture" << endl;

    mWidth = 1600;
    mHeight = 1200;
    mJpeg422 = false;
    mBayer10 = false;

    if (mSensorId == 0x1519) {
      writeReg (0xF0, 1);
      writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
      writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
      writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
      }
    else {
      writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0002); // sequencer.params.mode - capture video
      writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0002); // sequencer.cmd - goto capture mode B
      }

    mFramePtr = NULL;
    }
  //}}}
  //{{{
  void preview() {

    wcout << L"preview" << endl;

    if (mSensorId == 0x1519) {
      mWidth = 800;
      mHeight = 600;
      writeReg (0xF0, 1);
      writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
      writeReg (0xC6, 0xA120); writeReg (0xC8, 0x00); // Sequencer.params.mode - none
      writeReg (0xC6, 0xA103); writeReg (0xC8, 0x01); // Sequencer goto preview A
      }
    else {
      mWidth = 640;
      mHeight = 480;
      writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0000); // sequencer.params.mode - none
      writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0001); // sequencer.cmd - goto preview mode A
      }
    mJpeg422 = false;
    mBayer10 = false;

    mFramePtr = NULL;
    }
  //}}}
  //{{{
  void jpeg() {

    mWidth = 1600;
    mHeight = 1200;
    mJpeg422 = true;
    mBayer10 = false;

    //{{{  last data byte status ifp page2 0x02
    // b:0 = 1  transfer done
    // b:1 = 1  output fifo overflow
    // b:2 = 1  spoof oversize error
    // b:3 = 1  reorder buffer error
    // b:5:4    fifo watermark
    // b:7:6    quant table 0 to 2
    //}}}
    writeReg (0xf0, 0x0001);

    // mode_config JPG bypass - shadow ifp page2 0x0a
    writeReg (0xc6, 0x270b); writeReg (0xc8, 0x0010); // 0x0030 to disable B

    //{{{  jpeg.config id=9  0x07
    // b:0 =1  video
    // b:1 =1  handshake on error
    // b:2 =1  enable retry on error
    // b:3 =1  host indicates ready
    // ------
    // b:4 =1  scaled quant
    // b:5 =1  auto select quant
    // b:6:7   quant table id
    //}}}
    writeReg (0xc6, 0xa907); writeReg (0xc8, 0x0031);

    //{{{  mode fifo_config0_B - shadow ifp page2 0x0d
    //   output config  ifp page2  0x0d
    //   b:0 = 1  enable spoof frame
    //   b:1 = 1  enable pixclk between frames
    //   b:2 = 1  enable pixclk during invalid data
    //   b:3 = 1  enable soi/eoi
    //   -------
    //   b:4 = 1  enable soi/eoi during FV
    //   b:5 = 1  enable ignore spoof height
    //   b:6 = 1  enable variable pixclk
    //   b:7 = 1  enable byteswap
    //   -------
    //   b:8 = 1  enable FV on LV
    //   b:9 = 1  enable status inserted after data
    //   b:10 = 1  enable spoof codes
    //}}}
    writeReg (0xc6, 0x2772); writeReg (0xc8, 0x0067);

    //{{{  mode fifo_config1_B - shadow ifp page2 0x0e
    //   b:3:0   pclk1 divisor
    //   b:7:5   pclk1 slew
    //   -----
    //   b:11:8  pclk2 divisor
    //   b:15:13 pclk2 slew
    //}}}
    writeReg (0xc6, 0x2774); writeReg (0xc8, 0x0101);

    //{{{  mode fifo_config2_B - shadow ifp page2 0x0f
    //   b:3:0   pclk3 divisor
    //   b:7:5   pclk3 slew
    //}}}
    writeReg (0xc6, 0x2776); writeReg (0xc8, 0x0001);

    // mode OUTPUT WIDTH HEIGHT B
    writeReg (0xc6, 0x2707); writeReg (0xc8, mWidth);
    writeReg (0xc6, 0x2709); writeReg (0xc8, mHeight);

    // mode SPOOF WIDTH HEIGHT B
    writeReg (0xc6, 0x2779); writeReg (0xc8, mWidth);
    writeReg (0xc6, 0x277b); writeReg (0xc8, mHeight);

    //writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B

    //writeReg (0xc6, 0xa907); printf ("JPEG_CONFIG %x\n",readReg (0xc8));
    //writeReg (0xc6, 0x2772); printf ("MODE_FIFO_CONF0_B %x\n",readReg (0xc8));
    //writeReg (0xc6, 0x2774); printf ("MODE_FIFO_CONF1_B %x\n",readReg (0xc8));
    //writeReg (0xc6, 0x2776); printf ("MODE_FIFO_CONF2_B %x\n",readReg (0xc8));
    //writeReg (0xc6, 0x2707); printf ("MODE_OUTPUT_WIDTH_B %d\n",readReg (0xc8));
    //writeReg (0xc6, 0x2709); printf ("MODE_OUTPUT_HEIGHT_B %d\n",readReg (0xc8));
    //writeReg (0xc6, 0x2779); printf ("MODE_SPOOF_WIDTH_B %d\n",readReg (0xc8));
    //writeReg (0xc6, 0x277b); printf ("MODE_SPOOF_HEIGHT_B %d\n",readReg (0xc8));
    //writeReg (0xc6, 0xa906); printf ("JPEG_FORMAT %d\n",readReg (0xc8));
    //writeReg (0xc6, 0x2908); printf ("JPEG_RESTART_INT %d\n", readReg (0xc8));
    //writeReg (0xc6, 0xa90a); printf ("JPEG_QSCALE_1 %d\n",readReg (0xc8));
    //writeReg (0xc6, 0xa90b); printf ("JPEG_QSCALE_2 %d\n",readReg (0xc8));
    //writeReg (0xc6, 0xa90c); printf ("JPEG_QSCALE_3 %d\n",readReg (0xc8));

    mFramePtr = NULL;
    }
  //}}}
  //{{{
  void bayer() {

    if (mSensorId == 0x1519) {
      wcout << L"bayer" << endl;
      mWidth = 1608;
      mHeight = 1200;
      mJpeg422 = false;
      mBayer10 = true;

      writeReg (0xF0, 1);
      writeReg (0x09, 0x0008); // factory bypass 10 bit sensor
      writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
      writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
      }

    mFramePtr = NULL;
    }
  //}}}
  //{{{
  void setFocus (int value) {

    // printf ("focus %d\r", value);
    if (value < 0)
      mFocus = 0;
    else if (mFocus > 255)
      mFocus = 255;
    else
      mFocus = value;

    sendFocus (mFocus);
    }
  //}}}

  //{{{
  uint8_t* nextPacket (uint8_t* samplePtr, int packetLen) {
  // return nextPacket start address, wraparound if no room for another packetLen packet

    if (samplePtr + packetLen + packetLen <= mMaxSamplePtr)
      return samplePtr + packetLen;
    else
      return mSamples;
    }
  //}}}
  //{{{
  void loader() {

    #define QUEUESIZE 64
    uint8_t* bufferPtr[QUEUESIZE];
    uint8_t* contexts[QUEUESIZE];
    OVERLAPPED overLapped[QUEUESIZE];

    auto samplePtr = mSamples;
    auto packetLen = getBulkEndPoint()->MaxPktSize - 16;
    for (auto xferIndex = 0; xferIndex < QUEUESIZE; xferIndex++) {
      //{{{  setup QUEUESIZE transfers
      bufferPtr[xferIndex] = samplePtr;
      samplePtr = nextPacket (samplePtr, packetLen);
      overLapped[xferIndex].hEvent = CreateEvent (NULL, false, false, NULL);

      contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
      if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
        // error
        wcout << L"BeginDataXfer init failed " << getBulkEndPoint()->NtStatus
                   << L" " << getBulkEndPoint()->UsbdStatus
                   << endl;
        return;
        }
      }
      //}}}

    startStreaming();
    startTimer();

    auto frameLoadingPtr = mSamples;
    auto xferIndex = 0;
    while (true) {
      if (!getBulkEndPoint()->WaitForXfer (&overLapped[xferIndex], mTimeout)) {
        wcout << L"- timeOut" << getBulkEndPoint()->LastError << endl;
        getBulkEndPoint()->Abort();
        if (getBulkEndPoint()->LastError == ERROR_IO_PENDING)
          WaitForSingleObject (overLapped[xferIndex].hEvent, 2000);
        }

      LONG rxLen = packetLen;
      if (getBulkEndPoint()->FinishDataXfer (bufferPtr[xferIndex], rxLen, &overLapped[xferIndex], contexts[xferIndex])) {
        incSamplesLoaded (packetLen);
        if (rxLen < packetLen) {
          // short packet =  endOfFrame
          //printf ("\r%d %2d %5d %d %xd", frames, xferIndex, rxLen, samplesLoaded, (int)frameLoadingPtr);
          mFramePtr = frameLoadingPtr;
          mFrameBytes = (bufferPtr[xferIndex] + rxLen) - frameLoadingPtr;
          frameLoadingPtr = nextPacket (bufferPtr[xferIndex], packetLen);
          mFrames++;
          }

        bufferPtr[xferIndex] = samplePtr;
        samplePtr = nextPacket (samplePtr, packetLen);
        contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
        if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
          //{{{  error
          wcout << L"BeginDataXfer init failed " << getBulkEndPoint()->NtStatus
                     << L" " << getBulkEndPoint()->UsbdStatus
                     << endl;
          return;
          }
          //}}}
        }
      else {
        //{{{  error
        wcout << L"FinishDataXfer init failed" << endl;
        return;
        }
        //}}}

      xferIndex = xferIndex & (QUEUESIZE-1);
      }
    }
  //}}}

  //{{{  private vars
  int mSensorId = 0;

  float mLeftPixels = 50.0f;
  float mRightPixels = 50.0f;

  int mTimeout = 2000;
  bool mRestart = false;

  // samples
  uint8_t* mSamples = nullptr;
  uint8_t* mMaxSamplePtr = nullptr;
  int mSamplesLoaded = 0;
  size_t mMaxSamples = (16384-16) * 0x10000;

  double mMidSample = 0;
  double mSamplesPerSecond = 80000000;
  double mSamplesPerGraticule = mSamplesPerSecond;  // graticule every secoond
  double mMinSamplesPerPixel = 1.0/16.0;
  double mMaxSamplesPerPixel = (double)mMaxSamples / (800 - mLeftPixels - mRightPixels);
  double mSamplesPerPixel = mMaxSamplesPerPixel;

  // frames
  int mFrames = 0;
  uint8_t* mFramePtr = nullptr;
  uint8_t* mLastFramePtr = nullptr;
  int mFrameBytes = 0;
  int mGrabbedFrameBytes = 0;

  int mWidth = 800;
  int mHeight = 600;
  ID2D1Bitmap* mBitmap = nullptr;

  bool mBayer10 = false;
  bool mJpeg422 = false;
  bool mWaveform = false;
  bool mHistogram = true;
  bool mVector = true;

  int mFocus = 0;
  uint8_t mBucket = 1;
  int mHistogramValues [0x100];
  bool mVectorValues [0x4000];

  //{{{  layout
  float mBarPixels = 16.0f;
  float mRowPixels = 22.0f;
  float groupRowPixels = 28.0f;

  wstring mGraticuleStr;
  wstring mMeasureStr;
  //}}}
  //{{{  measure
  int hiCount [32];
  int loCount [32];
  int transitionCount [32];
  //}}}
  //{{{  pll
  int pllm = 16;
  int plln = 1;
  int pllp = 1;
  //}}}
  //{{{  jpeg
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  uint8_t jpegHeader[1000];
  int jpegHeaderBytes = 0;
  //}}}
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  cSensorWindow sensorWindow;
  sensorWindow.run();
  }
//}}}
