// wave_out.c
#include <string.h>
#include <errno.h>

#include "wave_out.h"

#define MAX_WAVEBLOCKS 8

static CRITICAL_SECTION cs;
static HWAVEOUT dev = NULL;
static int ScheduledBlocks = 0;
static int PlayedWaveHeadersCount = 0;
static WAVEHDR* PlayedWaveHeaders [MAX_WAVEBLOCKS];

//{{{
static void CALLBACK wave_callback (HWAVE hWave, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {

  if (uMsg == WOM_DONE) {
    EnterCriticalSection (&cs);

    PlayedWaveHeaders[PlayedWaveHeadersCount++] = (WAVEHDR*)dwParam1;

    LeaveCriticalSection (&cs);
    }

  }
//}}}
//{{{
static void free_memory() {

  EnterCriticalSection (&cs);

  WAVEHDR* wh = PlayedWaveHeaders[--PlayedWaveHeadersCount];
  ScheduledBlocks--;                        // decrease the number of USED blocks

  LeaveCriticalSection (&cs);

  waveOutUnprepareHeader (dev, wh, sizeof (WAVEHDR));

  HGLOBAL hg = GlobalHandle (wh -> lpData);       // Deallocate the buffer memory
  GlobalUnlock (hg);
  GlobalFree (hg);

  hg = GlobalHandle (wh);                 // Deallocate the header memory
  GlobalUnlock (hg);
  GlobalFree (hg);
  }
//}}}

//{{{
void winAudioOpen (Ldouble SampleFreq, Uint BitsPerSample, Uint Channels) {

  WAVEFORMATEX outFormat;
  outFormat.wFormatTag      = WAVE_FORMAT_PCM;
  outFormat.wBitsPerSample  = BitsPerSample;
  outFormat.nChannels       = Channels;
  outFormat.nSamplesPerSec  = (unsigned long)(SampleFreq);
  outFormat.nBlockAlign     = outFormat.nChannels * outFormat.wBitsPerSample/8;
  outFormat.nAvgBytesPerSec = outFormat.nSamplesPerSec * outFormat.nChannels * outFormat.wBitsPerSample/8;

  switch (waveOutOpen (&dev, WAVE_MAPPER, &outFormat, (DWORD_PTR)wave_callback, (DWORD_PTR)0, CALLBACK_FUNCTION)) {
    case MMSYSERR_ALLOCATED:   printf ( "Device is already open\n" );
    case MMSYSERR_BADDEVICEID: printf ( "The specified device is out of range.\n" );
    case MMSYSERR_NODRIVER:    printf ( "There is no audio driver in this system.\n" );
    case MMSYSERR_NOMEM:       printf ( "Unable to allocate sound memory.\n" );
    case WAVERR_BADFORMAT:     printf ( "This audio format is not supported.\n" );
    case WAVERR_SYNC:          printf ( "The device is synchronous.\n" );
    default:                   printf ( "Unknown media error\n." );
    case MMSYSERR_NOERROR:     break;
    }

  waveOutReset (dev);

  InitializeCriticalSection (&cs);

  //SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  }
//}}}
//{{{
void winAudioPlay (const void* data, size_t len) {

  do {
    while (PlayedWaveHeadersCount > 0) // free used
      free_memory ();

    if (ScheduledBlocks < sizeof(PlayedWaveHeaders) / sizeof(*PlayedWaveHeaders)) // wait for free block
      break;

    Sleep (26);

    } while (1);

  // allocate some memory for a copy of the buffer
  HGLOBAL hg2 = GlobalAlloc (GMEM_MOVEABLE, len);
  if (hg2 == NULL) {
    printf ("GlobalAlloc failed.\n");
    return;
  }

  // Here we can call any modification output functions we want....
  void* allocptr = GlobalLock (hg2);
  CopyMemory (allocptr, data, len);

  // now make a header and WRITE IT!
  HGLOBAL hg = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof (WAVEHDR));
  if (hg == NULL)
    return;

  LPWAVEHDR  wh = GlobalLock (hg);
  wh->dwBufferLength = (DWORD)len;
  wh->lpData = allocptr;

  if (waveOutPrepareHeader (dev, wh, sizeof (WAVEHDR)) != MMSYSERR_NOERROR ) {
    GlobalUnlock (hg);
    GlobalFree (hg);
    return;
    }

  if ( waveOutWrite (dev, wh, sizeof (WAVEHDR)) != MMSYSERR_NOERROR ) {
    GlobalUnlock (hg);
    GlobalFree (hg);
    return;
    }

  EnterCriticalSection (&cs);
  ScheduledBlocks++;
  LeaveCriticalSection (&cs);
  }
//}}}
//{{{
void winAudioClose() {

  if (dev != NULL) {
    while (ScheduledBlocks > 0) {
      Sleep (ScheduledBlocks);
      while (PlayedWaveHeadersCount > 0) // free used blocks
        free_memory();
      }

    waveOutReset (dev);
    waveOutClose (dev);
    dev = NULL;
    }

  DeleteCriticalSection (&cs);

  ScheduledBlocks = 0;
  }
//}}}
