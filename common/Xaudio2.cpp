// Xaudio2.cpp
//{{{  includes
#include "pch.h"

#include "winAudio.h"

#include <stdio.h>
#include <xaudio2.h>

#pragma comment(lib,"Xaudio2.lib")
//}}}
#define NUM_BUFFERS 4

//{{{
class cAudio2VoiceCallback : public IXAudio2VoiceCallback {
public:
  cAudio2VoiceCallback() : hBufferEndEvent (CreateEvent (NULL, FALSE, FALSE, NULL)) {}
  ~cAudio2VoiceCallback() { CloseHandle (hBufferEndEvent); }

  STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override {}
  STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}
  STDMETHOD_(void, OnStreamEnd)() override {}
  STDMETHOD_(void, OnBufferStart)(void*) override {}
  STDMETHOD_(void, OnBufferEnd)(void*) override { SetEvent (hBufferEndEvent); }
  STDMETHOD_(void, OnLoopEnd)(void*) override {}
  STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override {}

  HANDLE hBufferEndEvent;
  };
//}}}

IXAudio2* xAudio2;
IXAudio2MasteringVoice* xAudio2MasteringVoice;
IXAudio2SourceVoice* xAudio2SourceVoice;

int bufferIndex = 0;
BYTE* buffers [NUM_BUFFERS] = { NULL, NULL, NULL, NULL };
cAudio2VoiceCallback audio2VoiceCallback;

//{{{
void winAudioOpen (int sampleFreq, int bitsPerSample, int channels) {

  // create the XAudio2 engine.
  HRESULT hr = XAudio2Create (&xAudio2);
  if (hr != S_OK) {
    printf ("XAudio2Create failed\n");
    return;
    }

  hr = xAudio2->CreateMasteringVoice (&xAudio2MasteringVoice, 2, 48000);
  if (hr != S_OK) {
    printf ("CreateMasteringVoice failed\n");
    return;
    }

  WAVEFORMATEX waveformatex;
  memset ((void*)&waveformatex, 0, sizeof (WAVEFORMATEX));
  waveformatex.wFormatTag      = WAVE_FORMAT_PCM;
  waveformatex.wBitsPerSample  = bitsPerSample;
  waveformatex.nChannels       = channels;
  waveformatex.nSamplesPerSec  = (unsigned long)sampleFreq;
  waveformatex.nBlockAlign     = waveformatex.nChannels * waveformatex.wBitsPerSample/8;
  waveformatex.nAvgBytesPerSec = waveformatex.nSamplesPerSec * waveformatex.nChannels * waveformatex.wBitsPerSample/8;

  hr = xAudio2->CreateSourceVoice (&xAudio2SourceVoice, &waveformatex,
                                   0, XAUDIO2_DEFAULT_FREQ_RATIO,
                                   &audio2VoiceCallback, nullptr, nullptr);
  if (hr != S_OK) {
    printf ("CreateSourceVoice failed\n");
    return;
    }

  xAudio2SourceVoice->Start();
  }
//}}}
//{{{
void winAudioPlay (const void* data, int len, float pitch) {

  // copy data, it can be reused before we play it
  // - can reverse if needed
  buffers[bufferIndex] = (BYTE*)realloc (buffers[bufferIndex], len);
  memcpy (buffers[bufferIndex], data, len);

  // queue buffer
  XAUDIO2_BUFFER xAudio2_buffer;
  memset ((void*)&xAudio2_buffer, 0, sizeof (XAUDIO2_BUFFER));
  xAudio2_buffer.AudioBytes = len;
  xAudio2_buffer.pAudioData = buffers[bufferIndex];
  HRESULT hr = xAudio2SourceVoice->SubmitSourceBuffer (&xAudio2_buffer);
  if (hr != S_OK) {
    printf ("XAudio2 - SubmitSourceBufferCreate failed\n");
    return;
    }

  //printf ("winAudioPlay %3.1f\n", pitch);
  if ((pitch > 0.005f) && (pitch < 4.0f))
    xAudio2SourceVoice->SetFrequencyRatio (pitch, XAUDIO2_COMMIT_NOW);

  // cycle round buffers
  bufferIndex = (bufferIndex+1) % NUM_BUFFERS;

  // wait for buffer free if none left
  XAUDIO2_VOICE_STATE xAudio_voice_state;
  xAudio2SourceVoice->GetState (&xAudio_voice_state);
  if (xAudio_voice_state.BuffersQueued >= NUM_BUFFERS)
    WaitForSingleObject (audio2VoiceCallback.hBufferEndEvent, INFINITE);
  }
//}}}
//{{{
void winAudioClose() {

  HRESULT hr = xAudio2SourceVoice->Stop();
  hr = xAudio2->Release();
  }
//}}}
