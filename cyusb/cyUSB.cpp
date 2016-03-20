// cyUSB.cpp : Defines the entry point for the console application.
//{{{  includes
#include "pch.h"
#include <wtypes.h>

#include "../common/usbUtils.h"
#include "../common/jpegHeader.h"
//}}}
#define NUM_FRAMES 99
//{{{  vars
#define QUEUESIZE 64
int timeout = 2000;

BYTE* samples = NULL;
BYTE* maxSamplePtr = NULL;
int samplesLoaded = 0;
size_t maxSamples = (16384-16) * 0x1000;

int frames = 0;
int frameBytesArray [NUM_FRAMES];
BYTE* framePtrArray [NUM_FRAMES];
//}}}

//{{{
void incSamplesLoaded (int packetLen) {

  samplesLoaded += packetLen;
  }
//}}}
//{{{
BYTE* nextPacket (BYTE* samplePtr, int packetLen) {
// return nextPacket start address, wraparound if no room for another packetLen packet

  if (samplePtr + packetLen + packetLen <= maxSamplePtr)
    return samplePtr + packetLen;
  else
    return samples;
  }
//}}}
//{{{
DWORD WINAPI loaderThread (LPVOID arg) {

  int frameBytes = 0;
  BYTE* samplePtr = samples;
  int packetLen = getBulkEndPoint()->MaxPktSize - 16;

  BYTE* bufferPtr[QUEUESIZE];
  BYTE* contexts[QUEUESIZE];
  OVERLAPPED overLapped[QUEUESIZE];
  for (int xferIndex = 0; xferIndex < QUEUESIZE; xferIndex++) {
    //{{{  setup QUEUESIZE transfers
    bufferPtr[xferIndex] = samplePtr;
    samplePtr = nextPacket (samplePtr, packetLen);
    overLapped[xferIndex].hEvent = CreateEvent (NULL, false, false, NULL);
    contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
    if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
      //{{{  error
      printf ("BeginDataXfer init failed %d %d\n", getBulkEndPoint()->NtStatus, getBulkEndPoint()->UsbdStatus);
      return 0;
      }
      //}}}
    }
    //}}}

  startStreaming();

  BYTE* frameLoadingPtr = samples;
  int xferIndex = 0;
  while (true) {
    if (!getBulkEndPoint()->WaitForXfer (&overLapped[xferIndex], timeout)) {
      printf (" - timeOut %d\n", getBulkEndPoint()->LastError);
      getBulkEndPoint()->Abort();
      if (getBulkEndPoint()->LastError == ERROR_IO_PENDING)
        WaitForSingleObject (overLapped[xferIndex].hEvent, 2000);
      }

    long rxLen = packetLen;
    if (getBulkEndPoint()->FinishDataXfer (bufferPtr[xferIndex], rxLen, &overLapped[xferIndex], contexts[xferIndex])) {
      frameBytes += rxLen;
      incSamplesLoaded (packetLen);
      if (rxLen < packetLen) {
        // short packet =  endOfFrame
        if (frames < NUM_FRAMES) {
          framePtrArray [frames] = frameLoadingPtr;
          frameBytesArray [frames] = frameBytes;
          //printf ("%d %d\n", frames, frameBytesArray [frames]);
          }

        frameLoadingPtr = nextPacket (bufferPtr[xferIndex], packetLen);
        frameBytes = 0;
        frames++;
        }

      bufferPtr[xferIndex] = samplePtr;
      samplePtr = nextPacket (samplePtr, packetLen);
      contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
      if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
        //{{{  error
        printf ("BeginDataXfer init failed %d %d\n", getBulkEndPoint()->NtStatus, getBulkEndPoint()->UsbdStatus);
        return 0;
        }
        //}}}
      }
    else {
      //{{{  error
      printf ("FinishDataXfer init failed\n");
      return 0;
      }
      //}}}

    xferIndex = xferIndex & (QUEUESIZE-1);
    }

  return 0;
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

int main() {

  initUSB();
  writeReg (0xF0, 0);
  printf ("sensor %x\n", readReg (0x00));

  samples = (BYTE*)malloc (maxSamples);
  maxSamplePtr = samples + maxSamples;
  HANDLE hLoaderThread = CreateThread (NULL, 0, loaderThread, NULL, 0, NULL);
  SetThreadPriority (hLoaderThread, THREAD_PRIORITY_ABOVE_NORMAL);

  int width = 800;
  int height = 600;
  jpeg (width, height);

  //int qsel = (status & 0xC0) >> 6;
  writeReg (0xc6, 0xa90a); printf ("JPEG_QSCALE_1 %d\n",readReg (0xc8));
  int qscale1 = readReg (0xc8);
  BYTE jpegHeader [1000];
  int jpegHeaderBytes  = setJpegHeader (jpegHeader, width, height, 0, qscale1);

  int count = 0;
  while (count < NUM_FRAMES) {
    if (count < frames) {
      //printf ("w %2d %6d", count, frameBytesArray [count]);
      //for (int i = 0; i < 8; i++) printf(" %02x", *(framePtrArray[count] + i));
      //printf(" : ");
      //for (int i = -4 ; i < 0; i++) printf (" %02x",  *(framePtrArray [count] + frameBytesArray [count] + i));
      //printf ("\n");

      BYTE* endPtr = framePtrArray [count] + frameBytesArray [count] - 4;
      int jpegBytes = *endPtr++;
      jpegBytes += (*endPtr++) << 8;
      jpegBytes += (*endPtr++) << 16;
      int status = *endPtr;

      char filename[200];
      sprintf (filename, "C:\\Users\\nnn\\Pictures\\Camera Roll\\cam%2d.jpg", count);

      if ((status & 0x0f) == 0x01) {
        FILE* imfile = fopen (filename,"wb");
        fwrite (jpegHeader, 1, jpegHeaderBytes, imfile);     // write JPEG header
        fwrite (framePtrArray[count], 1, jpegBytes, imfile); // write JPEG data
        fwrite ("\xff\xd9", 1, 2, imfile);                   // write JPEG EOI marker
        fclose (imfile);
        }
      else
        printf ("err: ");

      printf ("%s %x %d\n", filename, status, jpegBytes);

      count++;
      }
    else
      Sleep (2);
    }

  closeUSB();

  return 0;
  }
