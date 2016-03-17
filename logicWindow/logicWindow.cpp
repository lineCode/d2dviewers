// logicWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2DWindow.h"
#include "../common/usbUtils.h"
//}}}
//#define mt9d111
#define mt9d112
#define QUEUESIZE 32

class cLogicWindow : public cD2dWindow {
public:
  cLogicWindow() {}
  ~cLogicWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height) {

    initialise (title, width, height);

    samples = (ULONG*)malloc (maxSamples*4);
    midSample = maxSamples / 2.0;
    setSamplesPerPixel (maxSamplesPerPixel);

    initUSB();

  #ifdef mt9d111
    pll (16, 1, 1);
    writeReg (0xF0, 0);
  #endif

    preview();
    readReg (0x00);

    auto loaderThread = thread([=]() { loader(); });
    SetThreadPriority (loaderThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    loaderThread.detach();

    thread ([=]() { analysis(); } ).detach();

    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : return false;

      case 0x1B: return true; // escape abort

      case 0x20: moreSamples(); break; // space bar

      case 0x21: setSamplesPerPixel (samplesPerPixel /= 2.0); break; // page up
      case 0x22: setSamplesPerPixel (samplesPerPixel *= 2.0); break; // page down

      case 0x23: setMidSample (samplesLoaded); break; // end
      case 0x24: setMidSample (0.0); break;                         // home

      case 0x25: setMidSample (midSample - (getClientF().width/4.0) * samplesPerPixel); break; // left arrow
      case 0x27: setMidSample (midSample + (getClientF().width/4.0) * samplesPerPixel); break; // right arrow

      case 0x26:
        //{{{  up arrow
        if (samplesPerPixel > 1.0)
          setMidSample (midSample - (mControlKeyDown ? 4.0 : mShiftKeyDown ? 2.0 : 1.0) * samplesPerPixel);
        else if (floor (midSample) != midSample)
          setMidSample (floor (midSample));
        else
          setMidSample (midSample - 1.0);
        break;
        //}}}
      case 0x28:
        //{{{  down arrow
        if (samplesPerPixel > 1.0)
          setMidSample (midSample + (mControlKeyDown ? 4.0 : mShiftKeyDown ? 2.0 : 1.0) * samplesPerPixel);
        else if (ceil (midSample) != midSample)
          setMidSample (midSample);
        else
          setMidSample (midSample + 1.0);
        break;
        //}}}

      case 'Q': preview(); break;
      case 'W': capture(); break;
      case 'E': bayer(); break;
      case 'J': jpeg (800, 600); break;

      case 'A': pllm++; pll (pllm, plln, pllp); break;
      case 'Z': pllm--; pll (pllm, plln, pllp); break;
      case 'S': plln++; pll (pllm, plln, pllp); break;
      case 'X': plln--; pll (pllm, plln, pllp); break;
      case 'D': pllp++; pll (pllm, plln, pllp); break;;
      case 'C': pllp--; pll (pllm, plln, pllp); break;
      case 'R': pll (80, 11, 0); break;  // 80 Mhz
      case 'T': pll (16, 1, 1); break;  // 48 Mhz

      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    double ratio = mControlKeyDown ? 1.5 : mShiftKeyDown ? 1.25 : 2.0;
    if (delta > 0)
      ratio = 1.0/ratio;

    setSamplesPerPixel (samplesPerPixel * ratio);
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    ULONG mask;
    if (getMask (x, y, mask))
      measure (mask, int(midSample + (x - getClientF().width/2.0f) * samplesPerPixel));
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (right) {
      ULONG mask = 0;
      if (getMask (mDownMousex, mDownMousey, mask))
        count (mask,
               int(midSample + (mDownMousex - getClientF().width/2.0f) * samplesPerPixel),
               int(midSample + (x - getClientF().width/2.0f) * samplesPerPixel));
      }
    else
      setMidSample (midSample - (xInc * samplesPerPixel));
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!right)
      if (!mouseMoved)
        setMidSample (midSample + (x - getClientF().width/2.0) * samplesPerPixel);
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    clearBackground (dc);
    drawGraticule (dc, 16);
    drawMidLine (dc, 16);
    drawSamplesTitle (dc);
    drawLeftLabels (dc, 16);
    drawSamples (dc, 0x10000);
    drawMeasure (dc);
    }
  //}}}

private:
  //{{{
  void clearBackground (ID2D1DeviceContext* dc) {

    dc->Clear (ColorF(ColorF::Black));
    }
  //}}}
  //{{{
  void drawGraticule (ID2D1DeviceContext* dc,int rows) {

    dc->DrawText (graticuleStr.c_str(), (UINT32)graticuleStr.size(), getTextFormat(),
                             RectF (0, 0, leftPixels, rowPixels), getWhiteBrush());

    auto rg = RectF (0, rowPixels, 0, (rows+1)*rowPixels);

    double leftSample = midSample - ((getClientF().width/2.0f - leftPixels) * samplesPerPixel);
    int graticule = int(leftSample + (samplesPerGraticule - 1.0)) / (int)samplesPerGraticule;
    double graticuleSample = graticule * samplesPerGraticule;

    bool more = true;
    while (more) {
      more = graticuleSample <= maxSamples;
      if (!more)
        graticuleSample = (double)maxSamples;

      rg.left = float((graticuleSample - midSample) / samplesPerPixel) + getClientF().width/2.0f;
      rg.right = rg.left+1;
      if (graticule > 0)
        dc->FillRectangle (rg, getGreyBrush());

      graticule++;
      graticuleSample += samplesPerGraticule;

      rg.left = rg.right;
      more &= rg.left < getClientF().width;
      }
    }
  //}}}
  //{{{
  void drawLeftLabels (ID2D1DeviceContext* dc, int rows) {

    auto rl = RectF(0.0f, rowPixels, getClientF().width, getClientF().height);
    for (auto dq = 0; dq < rows; dq++) {
      std::wstringstream stringStream;
      stringStream << L"dq" << dq;
      dc->DrawText (stringStream.str().c_str(), (UINT32)stringStream.str().size(), getTextFormat(),
                               rl, getWhiteBrush());
      rl.top += rowPixels;
      }
    }
  //}}}
  //{{{
  void drawSamples (ID2D1DeviceContext* dc,ULONG maxMask) {

    auto r = RectF (leftPixels, rowPixels, 0, 0);
    int lastSampleIndex = 0;
    for (auto j = int(leftPixels - getClientF().width/2.0f); j < int(getClientF().width/2.0f); j++) {
      r.right = r.left + 1.0f;

      int sampleIndex = int(midSample + (j * samplesPerPixel));
      if ((sampleIndex >= 0) && (sampleIndex < samplesLoaded)) {
        ULONG transition = 0;
        //{{{  set transition bitMask for samples spanned by this pixel
        if (lastSampleIndex && (lastSampleIndex != sampleIndex)) {
          // look for transition from lastSampleIndex to sampleIndex+1
          if (sampleIndex - lastSampleIndex <= 32) {
            // use all samples
            for (int index = lastSampleIndex; index < sampleIndex; index++)
              transition |= samples[index % maxSamples] ^ samples[(index+1) % maxSamples];
            }
          else {
            // look at some samples for transition
            int indexInc = (sampleIndex - lastSampleIndex) / 13;
            for (int index = lastSampleIndex; (index + indexInc <= sampleIndex)  && (index + indexInc < samplesLoaded - 1); index += indexInc)
              transition |= samples[index % maxSamples] ^ samples[(index+indexInc) % maxSamples];
            }
          }
        lastSampleIndex = sampleIndex;
        //}}}

        r.top = rowPixels;
        ULONG mask = 1;
        while ((mask != maxMask) && (r.top < getClientF().height)) {
          //{{{  draw rows for sample at this pixel
          float nextTop = r.top + ((mask & 0x80808080) ? groupRowPixels : rowPixels);

          if (transition & mask)
            r.bottom = r.top + barPixels;
          else {
            if (!(samples[sampleIndex % maxSamples] & mask)) // lo
              r.top += barPixels - 1.0f;
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
  void drawMidLine (ID2D1DeviceContext* dc,int rows) {

    dc->FillRectangle (
      RectF(getClientF().width/2.0f, rowPixels, getClientF().width/2.0f + 1.0f, (rows+1)*rowPixels),
      getWhiteBrush());
    }
  //}}}
  //{{{
  void drawSamplesTitle (ID2D1DeviceContext* dc) {

    std::wstringstream stringStream;
    stringStream << L"samples" << midSample / samplesPerSecond << L" of " << samplesLoaded / samplesPerSecond;
    dc->DrawText (stringStream.str().c_str(), (UINT32)stringStream.str().size(), getTextFormat(),
                             RectF(leftPixels, 0, getClientF().width, getClientF().height),
                             getWhiteBrush());
    }
  //}}}
  //{{{
  void drawMeasure (ID2D1DeviceContext* dc) {

    dc->DrawText (measureStr.c_str(), (UINT32)measureStr.size(), getTextFormat(),
                             RectF(getClientF().width/2.0f, 0.0f, getClientF().width, getClientF().height),
                             getWhiteBrush());
    }
  //}}}

  //{{{
  void setMidSample (double newMidSample) {
  // set sample of pixel in middle of display window

    if (newMidSample < 0.0)
      midSample = 0.0;
    else if (newMidSample > (double)samplesLoaded)
      midSample = (double)samplesLoaded;
    else
      midSample = newMidSample;

    changed();
    }
  //}}}
  //{{{
  void setSamplesPerPixel (double newSamplesPerPixel) {
  // set display window scale, samplesPerPixel

    if (newSamplesPerPixel < minSamplesPerPixel)
      samplesPerPixel = minSamplesPerPixel;
    else if (newSamplesPerPixel > maxSamplesPerPixel)
      samplesPerPixel = maxSamplesPerPixel;
    else
      samplesPerPixel = newSamplesPerPixel;

    samplesPerGraticule = 10.0;
    while (samplesPerGraticule < (100.0 * samplesPerPixel))
      samplesPerGraticule *= 10.0;

    std::wstringstream stringStream;
    if (samplesPerGraticule <= 10.0)
      stringStream << int (samplesPerGraticule / 0.1) << L"ns";
    else if (samplesPerGraticule <= 10000.0)
      stringStream << int (samplesPerGraticule / 100) << L"us";
    else if (samplesPerGraticule <= 10000000.0)
      stringStream << int (samplesPerGraticule / 100000) << L"ms";
    else if (samplesPerGraticule <= 10000000000.0)
      stringStream << int (samplesPerGraticule / 100000000) << L"s";
    else
      stringStream << int (samplesPerGraticule / 6000000000) << L"m";
    graticuleStr = stringStream.str();
    changed();
    }
  //}}}
  //{{{
  void loader() {

    if (getBulkEndPoint() == NULL)
      return;

    int len = getBulkEndPoint()->MaxPktSize;

    bool first = true;
    UCHAR* buffers[QUEUESIZE];
    UCHAR* contexts[QUEUESIZE];
    OVERLAPPED overLapped[QUEUESIZE];
    for (int i = 0; i < QUEUESIZE; i++)
      overLapped[i].hEvent = CreateEvent (NULL, false, false, NULL);

    while (true) {
      int done = 0;

      auto samplePtr = (UCHAR*)samples;
      auto maxSamplePtr = (UCHAR*)samples + 4*maxSamples;

      // Allocate all the buffers for the queues
      for (auto i = 0; i < QUEUESIZE; i++) {
        buffers[i] = samplePtr;
        samplePtr += len;
        contexts[i] = getBulkEndPoint()->BeginDataXfer (buffers[i] , len, &overLapped[i]);
        if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
          printf ("BeginDataXfer init failed:%d\n", getBulkEndPoint()->NtStatus);
          return;
          }
        }

      if (first)
        startStreaming();
      first = false;

      int i = 0;
      int count = 0;
      while (done < QUEUESIZE) {
        long rxLen = len;
        if (!getBulkEndPoint()->WaitForXfer (&overLapped[i], timeout)) {
          printf ("timeOut %d\n", getBulkEndPoint()->LastError);
          getBulkEndPoint()->Abort();
          if (getBulkEndPoint()->LastError == ERROR_IO_PENDING)
            WaitForSingleObject (overLapped[i].hEvent, 2000);
          }

        if (getBulkEndPoint()->FinishDataXfer (buffers[i], rxLen, &overLapped[i], contexts[i])) {
          samplesLoaded += len/4;
          changed();
          }
        else {
          printf ("FinishDataXfer failed\n");
          return;
          }

        // Re-submit this queue element to keep the queue full
        if (!restart && ((samplePtr + len) < maxSamplePtr)) {
          buffers[i] = samplePtr;
          samplePtr += len;
          contexts[i] = getBulkEndPoint()->BeginDataXfer (buffers[i], len, &overLapped[i]);
          if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
            printf ("BeginDataXfer requeue failed:%d\n", getBulkEndPoint()->NtStatus);
            return;
            }
          }
        else
          done++;

        i = (i + 1) & (QUEUESIZE-1);
        }

      while (!restart)
        Sleep (100);
      restart = false;
      }

    for (int i = 0; i < QUEUESIZE; i++)
      CloseHandle (overLapped[i].hEvent);

    closeUSB();
    }
  //}}}
  //{{{
  void analysis() {

    for (int i = 0; i < 32; i++) {
      hiCount [i] = 0;
      loCount [i] = 0;
      transitionCount [i] = 0;
      }

    int curSamplesAnaled = 0;
    int lastSamplesLoaded = 0;
    while (samplesLoaded < maxSamples-1) {
      curSamplesAnaled = samplesLoaded;
      if (curSamplesAnaled > lastSamplesLoaded) {
        for (int index = lastSamplesLoaded; index < curSamplesAnaled; index++)
          active |= samples[index] ^ samples[index+1];

        int dq = 0;
        ULONG mask = 0x00000001;
        while (mask != 0x10000) {
          for (auto index = lastSamplesLoaded; index < curSamplesAnaled; index++) {
            ULONG transition = samples[index] ^ samples[index+1];
            if (transition & mask) {
              transitionCount[dq]++;
              }
            else if (samples[index] & mask) {
              hiCount[dq]++;
              }
            else {
              loCount[dq]++;
              }
            }

          dq++;
          mask <<= 1;
          }

        lastSamplesLoaded = curSamplesAnaled;
        }
      Sleep (100);
      }
    }
  //}}}

  //{{{
  void pll (int m, int n, int p) {

    pllm = m;
    plln = n;
    pllp = p;

  #ifdef mt9d111
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
  #endif
    }
  //}}}
  //{{{
  void preview() {

    printf ("preview\n");

  #ifdef mt9d111
    writeReg (0xF0, 1);
    writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x00); // Sequencer.params.mode - none
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x01); // Sequencer goto preview A
    setFrameSize (800, 600, false);
  #endif

  #ifdef mt9d112
    writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0000); // sequencer.params.mode - none
    writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0001); // sequencer.cmd - goto preview mode A
  #endif

    }
  //}}}
  //{{{
  void capture() {

    printf ("capture\n");

  #ifdef mt9d111
    writeReg (0xF0, 1);
    writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
    setFrameSize (1600, 1200, false);
  #endif

  #ifdef mt9d112
    writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0002); // sequencer.params.mode - capture video
    writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0002); // sequencer.cmd - goto capture mode B
  #endif

    }
  //}}}
  //{{{
  void jpeg (int width, int height) {

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
    writeReg (0xc6, 0x2707); writeReg (0xc8, width);
    writeReg (0xc6, 0x2709); writeReg (0xc8, height);

    // mode SPOOF WIDTH HEIGHT B
    writeReg (0xc6, 0x2779); writeReg (0xc8, width);
    writeReg (0xc6, 0x277b); writeReg (0xc8, height);

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
    }
  //}}}
  //{{{
  void bayer() {

  #ifdef mt9d111
    printf ("bayer\n");
    bayer10 = true;

    writeReg (0xF0, 1);
    writeReg (0x09, 0x0008); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
  #endif
    }
  //}}}
  //{{{
  void moreSamples() {

    printf ("moreSamples\n");

    restart = true;
    setMidSample (maxSamples / 2.0);
    setSamplesPerPixel (maxSamplesPerPixel);
    }
  //}}}

  //{{{
  bool getMask (int x, int y, ULONG& mask) {

    float row = rowPixels;

    mask = 0x00000001;
    while ((mask != 0x10000) && (row < getClientF().height)) {
     float  nextRow = row + ((mask & 0x80808080) ? groupRowPixels : rowPixels);
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

    std::wstringstream stringStream;

    if ((sample > 0) && (sample < samplesLoaded)) {
      int back = sample;
      while ((back > 0) && !((samples[back] ^ samples[back - 1]) & mask) && ((sample - back) < 100000000))
        back--;

      int forward = sample;
      while ((forward < (samplesLoaded - 1)) &&
             !((samples[forward] ^ samples[forward + 1]) & mask) && ((forward - sample) < 100000000))
        forward++;

      if (((sample - back) != 100000000) && ((forward - sample) != 100000000)) {
        double width = (forward - back) / samplesPerSecond;

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

    measureStr = stringStream.str();
    }
  //}}}
  //{{{
  void count (ULONG mask, int firstSample, int lastSample) {

    int count = 0;
    int sample = firstSample;
    if ((sample > 0) && (sample < samplesLoaded)) {
      while ((sample < lastSample) && (sample < (samplesLoaded-2))) {
        if ((samples[sample] ^ samples[sample + 1]) & mask)
          count++;
        sample++;
        }

      if (count > 0) {
        std::wstringstream stringStream;
        stringStream << count;
        measureStr = stringStream.str();
        changed();
        }
      }
    }
  //}}}

  //{{{  vars
  // display
  float xWindow = 1600.0f;
  float leftPixels = 50.0f;
  float rightPixels = 50.0f;
  float barPixels = 16.0f;
  float rowPixels = 22.0f;
  float groupRowPixels = 28.0f;
  float yWindow = 18.0f * rowPixels;

  std::wstring graticuleStr;
  std::wstring measureStr;

  ULONG active = 0;
  int transitionCount [32];
  int hiCount [32];
  int loCount [32];

  // samples
  int timeout = 2000;
  bool restart = false;

  ULONG* samples = NULL;
  ULONG* maxSamplePtr = NULL;
  int samplesLoaded = 0;
  #ifdef _WIN64
    size_t maxSamples = 0x40000000;
  #else
    size_t maxSamples = 0x10000000;
  #endif

  double midSample = 0;
  double samplesPerSecond = 100000000;
  double samplesPerGraticule = samplesPerSecond;  // graticule every secoond
  double minSamplesPerPixel = 1.0/16.0;
  double maxSamplesPerPixel = (double)maxSamples / (xWindow - leftPixels - rightPixels);
  double samplesPerPixel = 0;

  // pll
  int pllm = 16;
  int plln = 1;
  int pllp = 1;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  cLogicWindow logicWindow;
  logicWindow.run (L"logicWindow", 1600, 18 * 22);
  }
//}}}
