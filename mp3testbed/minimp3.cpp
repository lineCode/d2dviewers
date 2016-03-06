// minimp3.c
//{{{  includes
#include <windows.h>
#include <stdio.h>

#define _USE_MATH_DEFINES
#include "../common/cMp3Decoder.h"

#include "../common/winAudio.h"
//}}}

int main (int argc, char* argv[]) {

  CoInitialize (NULL);  // for winAudio

  printf ("Small MPEG Audio Layer III player based on ffmpeg %s\n", argv[1]);

  HANDLE hFile = CreateFile (argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
  HANDLE hMap = CreateFileMapping (hFile, NULL, PAGE_READONLY, 0, 0, NULL);

  uint8_t* buffer = (uint8_t*)MapViewOfFile (hMap, FILE_MAP_READ, 0, 0, 0);
  int bufferBytes = GetFileSize (hFile, NULL) - 128;


  // check for ID3 tag
  auto ptr = buffer;
  uint32_t tag = ((*ptr)<<24) | (*(ptr+1)<<16) | (*(ptr+2)<<8) | *(ptr+3);
  if (tag == 0x49443303)  {
    //{{{  got ID3 tag
    uint32_t tagSize = (*(ptr+6)<<20) | (*(ptr+7)<<13) | (*(ptr+8)<<7) | *(ptr+9);
    printf ("%c%c%c ver:%d %02x flags:%02x tagSize:%d\n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), tagSize);
    ptr += 10;

    while (ptr < buffer + tagSize) {
      uint32_t frameSize = (*(ptr+4)<<20) | (*(ptr+5)<<13) | (*(ptr+6)<<7) | *(ptr+7);
      if (!frameSize)
        break;
      uint8_t frameFlags1 = *(ptr+8);
      uint8_t frameFlags2 = *(ptr+9);
      printf ("%c%c%c%c frameSize:%04d flags1:%02x flags2:%02x - ",
              *ptr, *(ptr+1), *(ptr+2), *(ptr+3), frameSize, frameFlags1, frameFlags2);
      if (frameSize < 60) {
        for (uint32_t i = 0; i < frameSize; i++)
          printf ("%c", *(ptr+10+i));
        }
      printf ("\n");
      ptr += frameSize + 10;
      };
    }
    //}}}
  else {
    //{{{  print header
    for (auto i = 0; i < 32; i++)
      printf ("%02x ", *(ptr+i));
    printf ("\n");
    }
    //}}}

  // set up minimp3 and decode the first frame
  winAudioOpen (44100, 16, 2);

  cMp3Decoder mMp3Decoder;
  while (true) {
    int16_t samples[1152*2];
    int bytesUsed = mMp3Decoder.decodeFrame (buffer, bufferBytes, samples);
    //printf ("%d\n", byte_count);
    if (bytesUsed) {
      buffer += bytesUsed;
      bufferBytes -= bytesUsed;
      winAudioPlay (samples, 1152*2*2, 1.0f);
      }
    }

  CoUninitialize();
  return 0;
  }
