// aacPlay.c
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <objbase.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/winAudio.h"

#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#include "../mp4ff/mp4ff.h"
#pragma comment(lib,"mp4ff.lib")
//}}}
//{{{  const
#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#define MAX_CHANNELS 6 /* make this higher to support files with more channels */

/* MicroSoft channel definitions */
#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

static int adts_sample_rates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350,0,0,0};

static const unsigned long srates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };

static const char *ot[6] = { "NULL", "MAIN AAC", "LC AAC", "SSR AAC", "LTP AAC", "HE AAC" };
//}}}
#include "manonthemoon.c"

typedef struct {
  FILE* infile;
  unsigned char* buffer;
  long file_offset;
  long bytes_into_buffer;
  long bytes_consumed;
  int at_eof;
  } aacBuffer;

//{{{
static void advance_buffer (aacBuffer* b, int bytes) {

  b->file_offset += bytes;
  b->bytes_consumed = bytes;
  b->bytes_into_buffer -= bytes;

  if (b->bytes_into_buffer < 0)
    b->bytes_into_buffer = 0;
  }
//}}}
//{{{
static int fill_buffer (aacBuffer* b) {

  int bytesRead;

  if (b->bytes_consumed > 0) {
    if (b->bytes_into_buffer)
      memmove ((void*)b->buffer, (void*)(b->buffer + b->bytes_consumed), b->bytes_into_buffer*sizeof(unsigned char));

    if (!b->at_eof) {
      bytesRead = fread ((void*)(b->buffer + b->bytes_into_buffer), 1, b->bytes_consumed, b->infile);

      if (bytesRead != b->bytes_consumed)
        b->at_eof = 1;

      b->bytes_into_buffer += bytesRead;
      }

    b->bytes_consumed = 0;

    if (b->bytes_into_buffer > 3)
      if (memcmp(b->buffer, "TAG", 3) == 0)
        b->bytes_into_buffer = 0;

    if (b->bytes_into_buffer > 11)
      if (memcmp(b->buffer, "LYRICSBEGIN", 11) == 0)
         b->bytes_into_buffer = 0;

    if (b->bytes_into_buffer > 8)
      if (memcmp(b->buffer, "APETAGEX", 8) == 0)
        b->bytes_into_buffer = 0;
    }

  return 1;
  }
//}}}

//{{{
static int aacChannelConfig2wavexChannelMask (NeAACDecFrameInfo* hInfo) {

  if (hInfo->channels == 6 && hInfo->num_lfe_channels)
    return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT +
           SPEAKER_FRONT_CENTER + SPEAKER_LOW_FREQUENCY + SPEAKER_BACK_LEFT + SPEAKER_BACK_RIGHT;
  else
    return 0;
  }
//}}}
//{{{
static char* position2string (int position) {

  switch (position) {
    case FRONT_CHANNEL_CENTER: return "Center front";
    case FRONT_CHANNEL_LEFT:   return "Left front";
    case FRONT_CHANNEL_RIGHT:  return "Right front";
    case SIDE_CHANNEL_LEFT:    return "Left side";
    case SIDE_CHANNEL_RIGHT:   return "Right side";
    case BACK_CHANNEL_LEFT:    return "Left back";
    case BACK_CHANNEL_RIGHT:   return "Right back";
    case BACK_CHANNEL_CENTER:  return "Center back";
    case LFE_CHANNEL:          return "LFE";
    case UNKNOWN_CHANNEL:      return "Unknown";
    default: return "";
    }

  return "";
  }
//}}}
//{{{
static void print_channel_info (NeAACDecFrameInfo* frameInfo) {

  int channelMask = aacChannelConfig2wavexChannelMask (frameInfo);

  if (frameInfo->num_lfe_channels > 0)
    printf ("Channels %2d.%d - ", frameInfo->channels-frameInfo->num_lfe_channels, frameInfo->num_lfe_channels);
  else
    printf ("Channels %2d - ", frameInfo->channels);

  if (channelMask)
    printf (" WARNING: channels are reordered according to\n");

  for (int i = 0; i < frameInfo->channels; i++)
    printf (" %.2d:%-14s ", i, position2string ((int)frameInfo->channel_position[i]));

  printf ("\n");
  }
//}}}

//{{{
static int decodeADTSfile (char* aacFileName, int outputFormat) {

  NeAACDecFrameInfo frameInfo;

  aacBuffer aacBuf;
  memset (&aacBuf, 0, sizeof(aacBuffer));
  aacBuf.infile = fopen (aacFileName, "rb");
  aacBuf.buffer = (unsigned char*)malloc (FAAD_MIN_STREAMSIZE * 2);
  aacBuf.bytes_into_buffer = fread (aacBuf.buffer, 1, FAAD_MIN_STREAMSIZE * 2, aacBuf.infile);;
  aacBuf.at_eof = (aacBuf.bytes_into_buffer != FAAD_MIN_STREAMSIZE * 2);
  aacBuf.bytes_consumed = 0;
  aacBuf.file_offset = 0;

  if ((aacBuf.buffer[0] == 0xFF) && ((aacBuf.buffer[1] & 0xF6) == 0xF0)) {
    NeAACDecHandle decoder = NeAACDecOpen();
    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (decoder);
    config->defSampleRate = 0;
    config->defObjectType = 0;
    config->outputFormat = outputFormat;
    config->downMatrix = 0;
    config->useOldADTSFormat = 0;
    NeAACDecSetConfiguration (decoder, config);

    unsigned long sampleRate;
    unsigned char channels;
    NeAACDecInit (decoder, aacBuf.buffer, aacBuf.bytes_into_buffer, &sampleRate, &channels);

    int sample = 0;
    void* sampleBuffer;
    do {
      sampleBuffer = NeAACDecDecode (decoder, &frameInfo, aacBuf.buffer, aacBuf.bytes_into_buffer);

      if (frameInfo.error > 0) {
        printf ("Error: %s\n", NeAACDecGetErrorMessage (frameInfo.error));
        break;
        }

      if (sample == 0) {
        print_channel_info (&frameInfo);
        winAudioOpen (frameInfo.samplerate, 16, frameInfo.channels);
        }

      winAudioPlay ((short*)sampleBuffer, frameInfo.channels * frameInfo.samples, 1.0f);

      sample++;
      printf ("ADTS - %d into:%d consumed:%d samples:%d sampleRate:%d ch:%d\r",
              sample, aacBuf.bytes_into_buffer, frameInfo.bytesconsumed, frameInfo.samples, sampleRate, channels);

      advance_buffer (&aacBuf, frameInfo.bytesconsumed);
      fill_buffer (&aacBuf);
      } while (aacBuf.bytes_into_buffer != 0);

    winAudioClose();
    NeAACDecClose (decoder);
    fclose (aacBuf.infile);
    free (aacBuf.buffer);
    }

  return frameInfo.error;
  }
//}}}
//{{{
static int decodeADTSbuf (unsigned char* buf, int bufLen, int outputFormat) {

  NeAACDecFrameInfo frameInfo;

  if ((buf[0] == 0xFF) && ((buf[1] & 0xF6) == 0xF0)) {
    NeAACDecHandle decoder = NeAACDecOpen();
    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (decoder);
    config->defSampleRate = 0;
    config->defObjectType = 0;
    config->outputFormat = outputFormat;
    config->downMatrix = 0;
    config->useOldADTSFormat = 0;
    NeAACDecSetConfiguration (decoder, config);

    unsigned long sampleRate;
    unsigned char channels;
    NeAACDecInit (decoder, buf, bufLen, &sampleRate, &channels);

    int sample = 0;
    int bufIndex = 0;
    void* sampleBuffer;
    do {
      int len = ((buf[3] & 0x03) << 11) | (buf[4] << 3) | ((buf[5] & 0xE0) >> 5);
      sampleBuffer = NeAACDecDecode (decoder, &frameInfo, buf+bufIndex, bufLen-bufIndex);
      //printf ("decode %d %d %d\n", bufIndex, len, frameInfo.bytesconsumed);
      bufIndex += frameInfo.bytesconsumed;

      if (frameInfo.error > 0) {
        printf ("Error: %s\n", NeAACDecGetErrorMessage (frameInfo.error));
        break;
        }

      if (sample == 0) {
        print_channel_info (&frameInfo);
        winAudioOpen (frameInfo.samplerate, 16, frameInfo.channels);
        }

      winAudioPlay ((short*)sampleBuffer, frameInfo.channels * frameInfo.samples, 1.0f);

      sample++;
      //printf ("ADTS - %d into:%d consumed:%d samples:%d sampleRate:%d ch:%d\n",
      //        sample, len, frameInfo.bytesconsumed, frameInfo.samples, sampleRate, channels);
      } while (bufIndex < bufLen);

    winAudioClose();
    NeAACDecClose (decoder);
    }

  return frameInfo.error;
  }
//}}}

//{{{
uint32_t seek_callback (void* user_data, uint64_t position) {

  return fseek ((FILE*)user_data, position, SEEK_SET);
  }
//}}}
//{{{
uint32_t read_callback (void* user_data, void* buffer, uint32_t length) {

  return fread (buffer, 1, length, (FILE*)user_data);
  }
//}}}
//{{{
static void decodeMP4file (char* mp4FileName, int outputFormat) {

  //  initialise the callback structure
  mp4ff_callback_t* mp4ff_callback = (mp4ff_callback_t*)malloc (sizeof(mp4ff_callback_t));
  FILE* mp4File = fopen (mp4FileName, "rb");
  mp4ff_callback->user_data = mp4File;
  mp4ff_callback->read = read_callback;
  mp4ff_callback->seek = seek_callback;
  mp4ff_t* mp4ff = mp4ff_open_read (mp4ff_callback);

  //{{{  init decoder
  NeAACDecHandle decoder = NeAACDecOpen();
  NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (decoder);
  config->outputFormat = outputFormat;
  config->downMatrix = 0;
  NeAACDecSetConfiguration (decoder, config);
  //}}}
  //{{{  find audTrack
  int audTrack;
  int numTracks = mp4ff_total_tracks (mp4ff);
  printf ("numtracks:%d\n", numTracks);
  for (audTrack = 0; audTrack < numTracks; audTrack++) {
    unsigned char* buff = NULL;
    unsigned int buff_size = 0;
    mp4ff_get_decoder_config (mp4ff, audTrack, &buff, &buff_size);
    printf (" - track:%d %x\n", audTrack, buff);
    if (buff) {
      mp4AudioSpecificConfig mp4ASC;
      int rc = NeAACDecAudioSpecificConfig (buff, buff_size, &mp4ASC);
      free (buff);
      if (rc >= 0)
        break;
      }
    }
  //}}}

  unsigned int buffer_size = 0;
  unsigned char* buffer = NULL;
  mp4ff_get_decoder_config (mp4ff, audTrack, &buffer, &buffer_size);

  unsigned char channels;
  unsigned long samplerate;
  NeAACDecInit2 (decoder, buffer, buffer_size, &samplerate, &channels);
  if (buffer)
    free (buffer);
  buffer = NULL;

  winAudioOpen (samplerate, 16, channels);

  long samples = mp4ff_num_samples (mp4ff, audTrack);
  //long vidSamples = mp4ff_num_samples (mp4ff, audTrack ? 0 : 1);

  for (int sample = 0; sample < samples; sample++) {
    buffer_size = 0;
    mp4ff_read_sample (mp4ff, audTrack, sample, &buffer, &buffer_size);

    NeAACDecFrameInfo frameInfo;
    short* sampleBuffer = (short*)NeAACDecDecode (decoder, &frameInfo, buffer, buffer_size);

    if (buffer)
      free (buffer);
    buffer = NULL;

    if (frameInfo.samples)
      winAudioPlay ((short*)sampleBuffer, frameInfo.channels*frameInfo.samples, 1.0f);
    }

  NeAACDecClose (decoder);
  winAudioClose();

  mp4ff_close (mp4ff);
  free (mp4ff_callback);
  fclose (mp4File);
  }
//}}}

//{{{
int main (int argc, char* argv[]) {

  CoInitialize (NULL);
  printf ("MPEG-4 AAC Decoder Faad v%s %s", FAAD2_VERSION, __DATE__);

  if (NeAACDecGetCapabilities() & FIXED_POINT_CAP)
    printf (" Fixed point version\n");
  else
    printf (" Floating point version\n");

  if (argc == 1)
    return decodeADTSbuf (aacBuf, aacBufLen, FAAD_FMT_16BIT);
  else {
    FILE* mp4File = fopen (argv[1], "rb");
    if (mp4File) {
      unsigned char header[8];
      fread (header, 1, 8, mp4File);
      fclose (mp4File);

      if (header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p')
        return decodeMP4file (argv[1], FAAD_FMT_16BIT);
      else
        return decodeADTSfile (argv[1], FAAD_FMT_16BIT);
      }
    }

  CoUninitialize();
  }
//}}}
