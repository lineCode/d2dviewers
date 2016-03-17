// cyUSB.cpp : Defines the entry point for the console application.
//{{{  includes
#include "pch.h"
#include <wtypes.h>

#include "../inc/CyAPI.h"
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
