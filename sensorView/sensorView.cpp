// sensorViewer.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2DWindow.h"
#include "../common/timer.h"
#include "../inc/jpeglib.h"

#pragma comment(lib,"turbojpeg-static")

using namespace D2D1;
//}}}
//{{{
class cAppWindow : public cD2dWindow {
public:
  cAppWindow() {}
  ~cAppWindow() {}
  //{{{
  static cAppWindow* create (wchar_t* title, int width, int height) {

    cAppWindow* appWindow = new cAppWindow();
    appWindow->initialise (title, width, height);
    return appWindow;
    }
  //}}}

  void setSamplesPerPixel (double newSamplesPerPixel);
  void preview();
  void pll (int m, int n, int p);

  void loader();
  void onDraw (ID2D1DeviceContext* deviceContext);

  bool onKey (int key);
  void onMouseWheel (int delta);
  void onMouseProx (int x, int y);
  void onMouseDown (bool right, int x, int y);
  void onMouseMove (bool right, int x, int y, int xInc, int yInc);
  void onMouseUp (bool right, bool mouseMoved, int x, int y);

private:
  void setMidSample (double newMidSample);
  void incSamplesLoaded (int packetLen);
  BYTE* nextPacket (BYTE* samplePtr, int packetLen);

  uint8_t limit (double v);
  void makeVidFrame (BYTE* frame, int frameBytes);
  void setFrameSize (int x, int y, bool bayer, bool jpeg);

  void capture();
  void bayer();
  void moreSamples();
  void setFocus (int value);

  bool getMask (int x, int y, ULONG& mask);
  void measure (ULONG mask, int sample);
  void count (ULONG mask, int firstSample, int lastSample);

  void drawBackground (ID2D1DeviceContext* deviceContext, BYTE* framePtr);
  void drawSamplesFramesTitle (ID2D1DeviceContext* deviceContext);
  void drawSensor (ID2D1DeviceContext* deviceContext, int sensorid);
  void drawLeftLabels (ID2D1DeviceContext* deviceContext, int rows);
  void drawSamples (ID2D1DeviceContext* deviceContext, ULONG maxMask);
  void drawGraticule (ID2D1DeviceContext* deviceContext, int rows);
  void drawMidLine (ID2D1DeviceContext* deviceContext, int rows);
  void drawHistogram (ID2D1DeviceContext* deviceContext);
  void drawVector (ID2D1DeviceContext* deviceContext);

  // private vars
  //{{{  layout
  float barPixels = 16.0f;
  float rowPixels = 22.0f;
  float groupRowPixels = 28.0f;
  std::wstring graticuleStr;
  std::wstring measureStr;
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
  //{{{  frames
  bool waveform = true;
  bool histogram = true;
  bool vector = true;
  int focus = 0;

  BYTE bucket = 1;
  int histogramValues [0x100];
  bool vectorValues [0x4000];

  bool bayer10 = false;
  bool jpeg422 = false;
  //}}}
  //{{{  jpeg
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  BYTE jpegHeader[1000];
  int jpegHeaderBytes = 0;
  //}}}
  };
//}}}
//{{{  vars
int sensorid = 0;

float xWindow = 800.0f;
float yWindow = 600.0f;

float leftPixels = 50.0f;
float rightPixels = 50.0f;

// samples
#define QUEUESIZE 64
int timeout = 2000;

bool restart = false;

BYTE* samples = NULL;
BYTE* maxSamplePtr = NULL;
int samplesLoaded = 0;
size_t maxSamples = (16384-16) * 0x10000;

double midSample = 0;
double samplesPerSecond = 80000000;
double samplesPerGraticule = samplesPerSecond;  // graticule every secoond
double minSamplesPerPixel = 1.0/16.0;
double maxSamplesPerPixel = (double)maxSamples / (xWindow - leftPixels - rightPixels);
double samplesPerPixel = maxSamplesPerPixel;

// frames
int frames = 0;
BYTE* framePtr = NULL;
BYTE* lastFramePtr = NULL;
int frameBytes = 0;
int grabbedFrameBytes = 0;

int width = 800;
int height = 600;

IWICBitmap* vidFrame;
ID2D1Bitmap* mD2D1Bitmap = NULL;
//}}}

//{{{
void cAppWindow::setMidSample (double newMidSample) {
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
void cAppWindow::setSamplesPerPixel (double newSamplesPerPixel) {
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
void cAppWindow::incSamplesLoaded (int packetLen) {

  bool tailing = (midSample == samplesLoaded);
  samplesLoaded += packetLen;
  if (tailing)
    setMidSample (samplesLoaded);
  }
//}}}
//{{{
BYTE* cAppWindow::nextPacket (BYTE* samplePtr, int packetLen) {
// return nextPacket start address, wraparound if no room for another packetLen packet

  if (samplePtr + packetLen + packetLen <= maxSamplePtr)
    return samplePtr + packetLen;
  else
    return samples;
  }
//}}}
//{{{
void cAppWindow::loader() {

  auto samplePtr = samples;
  auto packetLen = getBulkEndPoint()->MaxPktSize - 16;

  BYTE* bufferPtr[QUEUESIZE];
  BYTE* contexts[QUEUESIZE];
  OVERLAPPED overLapped[QUEUESIZE];
  for (auto xferIndex = 0; xferIndex < QUEUESIZE; xferIndex++) {
    //{{{  setup QUEUESIZE transfers
    bufferPtr[xferIndex] = samplePtr;
    samplePtr = nextPacket (samplePtr, packetLen);
    overLapped[xferIndex].hEvent = CreateEvent (NULL, false, false, NULL);

    contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
    if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
      // error
      std::wcout << L"BeginDataXfer init failed " << getBulkEndPoint()->NtStatus
                 << L" " << getBulkEndPoint()->UsbdStatus
                 << std::endl;
      return;
      }
    }
    //}}}

  startStreaming();
  startTimer();

  auto frameLoadingPtr = samples;
  auto xferIndex = 0;
  while (true) {
    if (!getBulkEndPoint()->WaitForXfer (&overLapped[xferIndex], timeout)) {
      std::wcout << L"- timeOut" << getBulkEndPoint()->LastError
                 << std::endl;

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
        framePtr = frameLoadingPtr;
        frameBytes = (bufferPtr[xferIndex] + rxLen) - frameLoadingPtr;
        frameLoadingPtr = nextPacket (bufferPtr[xferIndex], packetLen);
        frames++;
        }

      bufferPtr[xferIndex] = samplePtr;
      samplePtr = nextPacket (samplePtr, packetLen);
      contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
      if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
        //{{{  error
        std::wcout << L"BeginDataXfer init failed " << getBulkEndPoint()->NtStatus
                   << L" " << getBulkEndPoint()->UsbdStatus
                   << std::endl;
        return;
        }
        //}}}
      }
    else {
      //{{{  error
      std::wcout << L"FinishDataXfer init failed" << std::endl;
      return;
      }
      //}}}

    xferIndex = xferIndex & (QUEUESIZE-1);
    }
  }
//}}}

//{{{
uint8_t cAppWindow::limit (double v) {

  if (v <= 0.0)
    return 0;

  if (v >= 255.0)
    return 255;

  return (uint8_t)v;
  }
//}}}
//{{{
void cAppWindow::makeVidFrame (BYTE* frame, int frameBytes) {

  // create vidFrame wicBitmap 24bit BGR
  int pitch = width;
  if (width % 32)
    pitch = ((width + 31) / 32) * 32;
  int pad = pitch - width;

  if (!vidFrame)
    getWicImagingFactory()->CreateBitmap (pitch, height, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand, &vidFrame);

  // lock vidFrame wicBitmap
  WICRect wicRect = { 0, 0, pitch, height };
  IWICBitmapLock* wicBitmapLock = NULL;
  vidFrame->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);

  // get vidFrame wicBitmap buffer
  UINT bufferLen = 0;
  BYTE* buffer = NULL;
  wicBitmapLock->GetDataPointer (&bufferLen, &buffer);

  if (jpeg422) {
    if ((frameBytes != 800*600*2) && (frameBytes != 1600*800*2) && (frameBytes != 1600*800)) {
      //{{{  decode jpeg
      BYTE* endPtr = frame + frameBytes - 4;
      int jpegBytes = *endPtr++;
      jpegBytes += (*endPtr++) << 8;
      jpegBytes += (*endPtr++) << 16;
      auto status = *endPtr;

      if ((status & 0x0f) == 0x01) {
        cinfo.err = jpeg_std_error (&jerr);
        jpeg_create_decompress (&cinfo);
        jpegHeaderBytes = setJpegHeader (jpegHeader, width, height, 0, 6);

        jpeg_mem_src (&cinfo, jpegHeader, jpegHeaderBytes);
        jpeg_read_header (&cinfo, TRUE);
        cinfo.out_color_space = JCS_EXT_BGR;

        //wraparound problem if (frame >= maxSamplePtr)
        //  frame = samples;
        frame[jpegBytes] = 0xff;
        frame[jpegBytes+1] = 0xd9;
        jpeg_mem_src (&cinfo, frame, jpegBytes+2);
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
  else if (bayer10) {
    //{{{  bayer
    //  g r    right to left
    //  b g
    int x = 0;
    int y = 0;

    auto bufferEnd = buffer + bufferLen;
    while (buffer < bufferEnd) {
      if (frame >= maxSamplePtr)
        frame = samples;

      if ((y & 0x01) == 0) {
        // even lines
        *buffer++ = 0;
        *buffer++ = *frame++;; // g1
        *buffer++ = 0;

        *buffer++ = 0;
        *buffer++ = 0;
        *buffer++ = *frame++;  // r
        }

      else {
        // odd lines
        *buffer++ = *frame++; // b
        *buffer++ = 0;
        *buffer++ = 0;
        *buffer++ = 0;
        *buffer++ = *frame++; // g2
        *buffer++ = 0;
        }
      x += 2;

      if (x >= width) {
        for (int i = 0; i < pad*3; i++)
          *buffer++ = 0;
        y++;
        x = 0;
        }
      }
    }
    //}}}
  else {
    //{{{  yuv
    if (histogram)
      for (auto i = 0; i < 0x100; i++)
        histogramValues[i] = 0;

    if (vector)
      for (auto i = 0; i < 0x4000; i++)
        vectorValues[i] = false;

    auto bufferEnd = buffer + bufferLen;
    while (buffer < bufferEnd) {
      if (frame >= maxSamplePtr)
        frame = samples;

      int y1 = *frame++;
      int u = *frame++;
      int y2 = *frame++;
      int v = *frame++;

      if (histogram) {
        histogramValues [y1 >> bucket]++;
        histogramValues [y2 >> bucket]++;
        }

      if (vector)
        vectorValues [((v << 6) & 0x3F80) | (u >> 1)] = true;

      u -= 128;
      v -= 128;

      *buffer++ = limit (y1 + (1.8556 * u));
      *buffer++ = limit (y1 - (0.1873 * u) - (0.4681 * v));
      *buffer++ = limit (y1 + (1.5748 * v));

      *buffer++ = limit (y2 + (1.8556 * u));
      *buffer++ = limit (y2 - (0.1873 * u) - (0.4681 * v));
      *buffer++ = limit (y2 + (1.5748 * v));
      }

    if (histogram) {
      int maxHistogram = 0;
      for (auto i = 0; i < (0x100 >> bucket) - 1; i++)
        if (histogramValues[i] > maxHistogram)
          maxHistogram = histogramValues[i];
      for (auto i = 0; i < 0x100 >> bucket; i++)
        histogramValues[i] = (histogramValues[i] << 8) / maxHistogram;
      }
    }
    //}}}

  // release vidFrame wicBitmap buffer
  wicBitmapLock->Release();

  // convert to mD2D1Bitmap 32bit BGRA
  IWICFormatConverter* wicFormatConverter;
  getWicImagingFactory()->CreateFormatConverter (&wicFormatConverter);
  wicFormatConverter->Initialize (vidFrame,
                                  GUID_WICPixelFormat32bppPBGRA,
                                  WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
  if (mD2D1Bitmap)
    mD2D1Bitmap->Release();
  if (getDeviceContext())
    getDeviceContext()->CreateBitmapFromWicBitmap (wicFormatConverter, NULL, &mD2D1Bitmap);
  }
//}}}
//{{{
void cAppWindow::setFrameSize (int x, int y, bool bayer, bool jpeg) {

  width = x;
  height = y;
  bayer10 = bayer;
  jpeg422 = jpeg;

  if (vidFrame)
    vidFrame->Release();

  vidFrame = NULL;
  }
//}}}

//{{{
void cAppWindow::pll (int m, int n, int p) {

  pllm = m;
  plln = n;
  pllp = p;

  if (sensorid == 0x1519) {
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
void cAppWindow::preview() {

  std::wcout << L"preview" << std::endl;

  if (sensorid == 0x1519) {
    writeReg (0xF0, 1);
    writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x00); // Sequencer.params.mode - none
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x01); // Sequencer goto preview A
    setFrameSize (800, 600, false, false);
    }
  else {
    writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0000); // sequencer.params.mode - none
    writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0001); // sequencer.cmd - goto preview mode A
    setFrameSize (640, 480, false, false);
    }
  framePtr = NULL;
  }
//}}}
//{{{
void cAppWindow::capture() {

  std::wcout << L"capture" << std::endl;

  if (sensorid == 0x1519) {
    writeReg (0xF0, 1);
    writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
    setFrameSize (1600, 1200, false, false);
    }
  else {
    writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0002); // sequencer.params.mode - capture video
    writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0002); // sequencer.cmd - goto capture mode B
    setFrameSize (1600, 1200, false, false);
    }
  framePtr = NULL;
  }
//}}}
//{{{
void cAppWindow::bayer() {

  if (sensorid == 0x1519) {
    std::wcout << L"bayer" << std::endl;
    bayer10 = true;

    setFrameSize (1608, 1200, true, false);

    writeReg (0xF0, 1);
    writeReg (0x09, 0x0008); // factory bypass 10 bit sensor
    writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
    writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
    }

  framePtr = NULL;
  }
//}}}
//{{{
void cAppWindow::moreSamples() {

  std::wcout << L"moreSamples" << std::endl;

  restart = true;
  setMidSample (samplesLoaded);
  setSamplesPerPixel (maxSamplesPerPixel);
  }
//}}}
//{{{
void cAppWindow::setFocus (int value) {

  // printf ("focus %d\r", value);

  if (value < 0)
    focus = 0;
  else if (focus > 255)
    focus = 255;
  else
    focus = value;

  sendFocus (focus);
  }
//}}}

//{{{
bool cAppWindow::getMask (int x, int y, ULONG& mask) {

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
void cAppWindow::measure (ULONG mask, int sample) {

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
void cAppWindow::count (ULONG mask, int firstSample, int lastSample) {

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

//{{{
void cAppWindow::drawBackground (ID2D1DeviceContext* deviceContext, BYTE* framePtr) {

  if (framePtr && (framePtr != lastFramePtr)) {
    grabbedFrameBytes = frameBytes;
    makeVidFrame (framePtr, frameBytes);
    framePtr = lastFramePtr;
    }

  if (mD2D1Bitmap)
    deviceContext->DrawBitmap (mD2D1Bitmap, RectF(0,0, getClientF().width, getClientF().height));
  else
    deviceContext->Clear (ColorF(ColorF::Black));
  }
//}}}
//{{{
void cAppWindow::drawSamplesFramesTitle (ID2D1DeviceContext* deviceContext) {

  std::wstringstream stringStream;
  stringStream << L"samples" << midSample / samplesPerSecond
               << L" " << samplesLoaded / samplesPerSecond
               << L" " << samplesLoaded
               << L" " << frames
               << L" " << frames / getTimer()
               << L" " << grabbedFrameBytes
               << L" " << width
               << L" " << height
               << L" " << focus;
  deviceContext->DrawText (stringStream.str().c_str(), (UINT32)stringStream.str().size(), getTextFormat(),
                           RectF(leftPixels, 0, getClientF().width, getClientF().height), getWhiteBrush());
  }
//}}}
//{{{
void cAppWindow::drawSensor (ID2D1DeviceContext* deviceContext, int sensorid) {

  if (sensorid == 0x1519) {
    std::wstringstream stringStream;
    stringStream << L"mt9d111";
    deviceContext->DrawText (stringStream.str().c_str(), (UINT32)stringStream.str().size(), getTextFormat(),
                             RectF(getClientF().width - 2*rightPixels, 0, getClientF().width, rowPixels), getWhiteBrush());
    }
  }
//}}}
//{{{
void cAppWindow::drawLeftLabels (ID2D1DeviceContext* deviceContext, int rows) {

  auto rl = RectF(0.0f, rowPixels, getClientF().width, getClientF().height);
  for (auto dq = 0; dq < rows; dq++) {
    std::wstringstream stringStream;
    stringStream << L"dq" << dq;
    deviceContext->DrawText (stringStream.str().c_str(), (UINT32)stringStream.str().size(), getTextFormat(),
                             rl, getWhiteBrush());
    rl.top += rowPixels;
    }
  }
//}}}
//{{{
void cAppWindow::drawSamples (ID2D1DeviceContext* deviceContext, ULONG maxMask) {

  auto r = RectF (leftPixels, rowPixels, 0, 0);
  auto lastSampleIndex = 0;
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
        auto nextTop = r.top + ((mask & 0x80808080) ? groupRowPixels : rowPixels);

        if (transition & mask)
          r.bottom = r.top + barPixels;
        else {
          if (!(samples[sampleIndex % maxSamples] & mask)) // lo
            r.top += barPixels - 1.0f;
          r.bottom = r.top + 1.0f;
          }

        deviceContext->FillRectangle (r, getBlueBrush());

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
void cAppWindow::drawGraticule (ID2D1DeviceContext* deviceContext, int rows) {

  deviceContext->DrawText (graticuleStr.c_str(), (UINT32)graticuleStr.size(), getTextFormat(),
                           RectF (0, 0, leftPixels, rowPixels), getWhiteBrush());

  auto rg = RectF (0, rowPixels, 0, (rows+1)*rowPixels);

  auto leftSample = midSample - ((getClientF().width/2.0f - leftPixels) * samplesPerPixel);
  auto graticule = int(leftSample + (samplesPerGraticule - 1.0)) / (int)samplesPerGraticule;
  auto graticuleSample = graticule * samplesPerGraticule;

  auto more = true;
  while (more) {
    more = graticuleSample <= maxSamples;
    if (!more)
      graticuleSample = (double)maxSamples;

    rg.left = float((graticuleSample - midSample) / samplesPerPixel) + getClientF().width/2.0f;
    rg.right = rg.left+1;
    if (graticule > 0)
      deviceContext->FillRectangle (rg, getGreyBrush());

    graticule++;
    graticuleSample += samplesPerGraticule;

    rg.left = rg.right;
    more &= rg.left < getClientF().width;
    }
  }
//}}}
//{{{
void cAppWindow::drawMidLine (ID2D1DeviceContext* deviceContext, int rows) {

  deviceContext->FillRectangle (
    RectF(getClientF().width/2.0f, rowPixels, getClientF().width/2.0f + 1.0f, (rows+1)*rowPixels),
    getBlackBrush());
  }
//}}}
//{{{
void cAppWindow::drawHistogram (ID2D1DeviceContext* deviceContext) {

  auto r = RectF (10.0f, 0, 0, getClientF().height - 10.0f);

  auto inc = 1.0f;
  for (auto i = 0; i < bucket; i++)
    inc *= 2.0f;

  for (auto i = 0; i < 0x100 >> bucket; i++) {
    r.right = r.left + inc;
    r.top = r.bottom - histogramValues[i] - 1;
    deviceContext->FillRectangle (r, getWhiteBrush());
    r.left = r.right;
    }
  }
//}}}
//{{{
void cAppWindow::drawVector (ID2D1DeviceContext* deviceContext) {

  deviceContext->FillRectangle (
    RectF (getClientF().width - 128.0f, getClientF().height - 256.0f,
           getClientF().width - 128.0f + 1.0f, getClientF().height), getBlackBrush());

  deviceContext->FillRectangle (
    RectF (getClientF().width - 256.0f, getClientF().height - 128.0f,
           getClientF().width, getClientF().height - 128.0f - 1.0f), getBlackBrush());

  auto r = RectF (getClientF().width - 256.0f, getClientF().height - 256.0f, 0, 0);

  auto index = 0;
  for (auto j = 0; j < 0x80; j++) {
    r.bottom = r.top + 2.0f;
    for (int i = 0; i < 0x80; i++) {
      r.right = r.left + 2.0f;
      if (vectorValues[index++])
        deviceContext->FillRectangle (r, getWhiteBrush());
      r.left = r.right;
      }
    r.left = getClientF().width - 256.0f;
    r.top = r.bottom;
    }
  }
//}}}
//{{{
void cAppWindow::onDraw (ID2D1DeviceContext* deviceContext) {

  drawBackground (deviceContext, framePtr);
  drawSamplesFramesTitle (deviceContext);
  drawSensor (deviceContext, sensorid);
  if (waveform) {
    drawLeftLabels (deviceContext, 8);
    drawSamples (deviceContext, 0x100);
    drawGraticule (deviceContext, 8);
    drawMidLine (deviceContext, 8);
    }
  if (!bayer10 && !jpeg422 && histogram)
    drawHistogram (deviceContext);
  if (!bayer10 && !jpeg422 && vector)
    drawVector (deviceContext);
  }
//}}}

//{{{
bool cAppWindow::onKey (int key) {

  switch (key) {
    case 0x00 : return false;

    case 0x10: shiftKeyDown = true; return false; // shift key
    case 0x11: controlKeyDown = true; return false; // control key

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
        setMidSample (midSample - (controlKeyDown ? 4.0 : shiftKeyDown ? 2.0 : 1.0) * samplesPerPixel);
      else if (floor (midSample) != midSample)
        setMidSample (floor (midSample));
      else
        setMidSample (midSample - 1.0);
      break;
      //}}}
    case 0x28:
      //{{{  down arrow
      if (samplesPerPixel > 1.0)
        setMidSample (midSample + (controlKeyDown ? 4.0 : shiftKeyDown ? 2.0 : 1.0) * samplesPerPixel);
      else if (ceil (midSample) != midSample)
        setMidSample (midSample);
      else
        setMidSample (midSample + 1.0);
      break;
      //}}}

    case 'O': waveform = !waveform; changed();; break;
    case 'H': histogram = !histogram; changed();; break;
    case 'V': vector = !vector; changed();; break;

    case 'B': setFocus (--focus); break;
    case 'N': setFocus (++focus); break;

    case 'Q': preview(); break;
    case 'W': capture(); break;
    case 'E': bayer(); break;
    case 'J': jpeg (1600, 1200); setFrameSize (1600, 1200, false, true);  framePtr = NULL; break;

    case 'A': pll (++pllm, plln, pllp); break;
    case 'Z': pll (--pllm, plln, pllp); break;
    case 'S': pll (pllm, ++plln, pllp); break;
    case 'X': pll (pllm, --plln, pllp); break;
    case 'D': pll (pllm, plln, ++pllp); break;;
    case 'C': pll (pllm, plln, --pllp); break;
    case 'R': pll (80, 11, 0); break;  // 80 Mhz
    case 'T': pll (16, 1, 1); break;  // 48 Mhz

    default: std::wcout << L"key:" << key << std::endl;
    }

  return false;
  }
//}}}
//{{{
void cAppWindow::onMouseWheel (int delta) {

  double ratio = controlKeyDown ? 1.5 : shiftKeyDown ? 1.25 : 2.0;
  if (delta > 0)
    ratio = 1.0/ratio;

  setSamplesPerPixel (samplesPerPixel * ratio);
  }
//}}}
//{{{
void cAppWindow::onMouseProx (int x, int y) {

  ULONG mask;
  if (getMask (x, y, mask))
    measure (mask, int(midSample + (x - getClientF().width/2.0f) * samplesPerPixel));
  }
//}}}
//{{{
void cAppWindow::onMouseDown (bool right, int x, int y) {
  }
//}}}
//{{{
void cAppWindow::onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  if (controlKeyDown)
    setFocus (focus + xInc);

  else if (right) {
    ULONG mask = 0;
    if (getMask (downMousex, downMousey, mask))
      count (mask,
             int(midSample + (downMousex - getClientF().width/2.0f) * samplesPerPixel),
             int(midSample + (x - getClientF().width/2.0f) * samplesPerPixel));
    }
  else
    setMidSample (midSample - (xInc * samplesPerPixel));
  }
//}}}
//{{{
void cAppWindow::onMouseUp (bool right, bool mouseMoved, int x, int y) {

  if (!right)
    if (!mouseMoved)
      setMidSample (midSample + (x - getClientF().width/2.0) * samplesPerPixel);
  }
//}}}

//{{{
int wmain (int argc, wchar_t* argv[]) {

  auto appWindow = cAppWindow::create (L"sensorView", int(xWindow), int(yWindow));

  // samples
  samples = (BYTE*)malloc (maxSamples);
  maxSamplePtr = samples + maxSamples;
  appWindow->setSamplesPerPixel (maxSamplesPerPixel);

  initUSB();

  sensorid = readReg (0x3000);
  if (sensorid != 0x1519) {
    writeReg (0xF0, 0);
    sensorid = readReg (0x00);
    std::wcout << L"sensorid " << sensorid << std::endl;
    }
  if (sensorid == 0x1519)
    appWindow->pll (16, 1, 1);

  appWindow->preview();

  auto loaderThread = std::thread([=]() { appWindow->loader(); });
  SetThreadPriority (loaderThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
  loaderThread.detach();

  appWindow->messagePump();
  }
//}}}
