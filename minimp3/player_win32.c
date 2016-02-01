//{{{  includes
// minimp3 example player application for Win32
// this file is public domain -- do with it whatever you want!
#define MAIN_PROGRAM

#include "stdio.h"
#include "libc.h"
#include "minimp3.h"
//}}}
#define BUFFER_COUNT 8
//{{{
static WAVEFORMATEX wf = {
  1,  // wFormatTag
  0,  // nChannels
  0,  // nSamplesPerSec
  0,  // nAvgBytesPerSec
  4,  // nBlockAlign
  16, // wBitsPerSample
  sizeof(WAVEFORMATEX) // cbSize
  };
//}}}
//{{{
static const WAVEHDR wh_template = {
  NULL, // lpData
  0, // dwBufferLength
  0, // dwBytesRecorded
  0, // dwUser
  0, // dwFlags
  1, // dwLoops
  NULL, // lpNext
  0 // reserved
  };
//}}}

static mp3_decoder_t mp3;
static mp3_info_t info;
static unsigned char* stream_pos;
static int bytes_left;
static int byte_count;
static WAVEHDR wh[BUFFER_COUNT];
static signed short sample_buffer[MP3_MAX_SAMPLES_PER_FRAME * BUFFER_COUNT];

//{{{
void CALLBACK AudioCallback (HWAVEOUT hwo, UINT uMsg,
                             DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2 ) {

  LPWAVEHDR wh = (LPWAVEHDR) dwParam1;
  if (!wh)
    return;

  if (byte_count) {
    stream_pos += byte_count;
    bytes_left -= byte_count;
    waveOutUnprepareHeader(hwo, wh, sizeof(WAVEHDR));
    waveOutPrepareHeader(hwo, wh, sizeof(WAVEHDR));
    waveOutWrite(hwo, wh, sizeof(WAVEHDR));
    }

  byte_count = mp3_decode(mp3, stream_pos, bytes_left, (signed short *) wh->lpData, &info);
  }
//}}}
//{{{
void ShowTag (const char *caption, const unsigned char *src, int max_length) {

  static char tagbuf[32];
  char *tagpos = tagbuf;

  tagbuf[max_length] = '\0';
  __asm {
    cld
    mov esi, src
    mov edi, tagpos
    mov ecx, max_length
    rep movsb
    }

  if (!*tagbuf)
    return;

  printf ("caption %s tagbuf:%s\n", caption, tagbuf);
  }
//}}}

int main (int argc, char* argv[]) {

  printf ("Minimp3 - small MPEG Audio Layer III player based on ffmpeg\n\n");

  // open and mmap() the file
  HANDLE hFile = CreateFile (argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
  bytes_left = GetFileSize (hFile, NULL) - 128;
  HANDLE hMap = CreateFileMapping (hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  stream_pos = (unsigned char*)MapViewOfFile (hMap, FILE_MAP_READ, 0, 0, 0);
  if (!stream_pos) {
    printf ("Error: cannot open %s\n", argv[1]);
    return 1;
    }
  else
    printf ("Now Playing: %s\n", argv[1]);

  // check for a ID3 tag
  char* inptr = stream_pos + bytes_left;
  if (((*(unsigned long *)inptr) & 0xFFFFFF) == 0x474154) {
    ShowTag ("\nTitle: ",   inptr +  3, 30);
    ShowTag ("\nArtist: ",  inptr + 33, 30);
    ShowTag ("\nAlbum: ",   inptr + 63, 30);
    ShowTag ("\nYear: ",    inptr + 93,  4);
    ShowTag ("\nComment: ", inptr + 97, 30);
    }

  // set up minimp3 and decode the first frame
  mp3 = mp3_create();
  byte_count = mp3_decode (mp3, stream_pos, bytes_left, sample_buffer, &info);
  if (!byte_count) {
    printf ("\nError: not a valid MP2 audio file!\n");
    return 1;
    }

  // set up wave output
  HWAVEOUT hwo;
  wf.nSamplesPerSec = info.sample_rate;
  wf.nChannels = info.channels;
  if (waveOutOpen (&hwo, WAVE_MAPPER, &wf,
                   (INT_PTR)AudioCallback, (INT_PTR)NULL, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
    printf ("\nError: cannot open wave output!\n");
    return 1;
    }

  // allocate buffers
  inptr = (char*)sample_buffer;
  for (int i = 0;  i < BUFFER_COUNT;  ++i) {
    wh[i] = wh_template;
    wh[i].lpData = inptr;
    wh[i].dwBufferLength = info.audio_bytes;
    AudioCallback (hwo, 0, 0, (DWORD) &wh[i], 0);
    inptr += MP3_MAX_SAMPLES_PER_FRAME * 2;
    }

  // endless loop
  while (1)
    Sleep(10);

  return 0;
  }
