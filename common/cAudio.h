// cAudio.h
#pragma once
//{{{  includes
#include <stdio.h>
#include <stdint.h>
#include <xaudio2.h>

#pragma comment(lib,"Xaudio2.lib")
//}}}
#define NUM_BUFFERS 8

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

class cAudio {
public:
  //{{{
  cAudio() {
    mSilence = (int16_t*)malloc (2048*4);
    memset (mSilence, 0, 2048*4);
    }
  //}}}
  //{{{
  virtual ~cAudio() {
    free (mSilence);
    }
  //}}}

  //{{{
  float getVolume() {
    return mVolume;
    }
  //}}}
  //{{{
  void setVolume (float volume) {
    if (volume < 0)
      mVolume = 0;
    else if (volume > 100.0f)
      mVolume = 100.0f;
    else
      mVolume = volume;
    }
  //}}}

  //{{{
  void audOpen (int sampleFreq, int bitsPerSample, int channels) {

    // create the XAudio2 engine.
    HRESULT hr = XAudio2Create (&mxAudio2);
    if (hr != S_OK) {
      printf ("XAudio2Create failed\n");
      return;
      }

    hr = mxAudio2->CreateMasteringVoice (&mxAudio2MasteringVoice, 2, 48000);
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

    hr = mxAudio2->CreateSourceVoice (&mxAudio2SourceVoice, &waveformatex,
                                      0, XAUDIO2_DEFAULT_FREQ_RATIO, &mAudio2VoiceCallback, nullptr, nullptr);
    if (hr != S_OK) {
      printf ("CreateSourceVoice failed\n");
      return;
      }

    mxAudio2SourceVoice->Start();
    }
  //}}}
  //{{{
  void audPlay (int16_t* src, int len, float pitch) {

    if (!src)
      src = mSilence;

    // copy data, it can be reused before we play it
    // - can reverse if needed
    mBuffers[mBufferIndex] = (uint8_t*)realloc (mBuffers[mBufferIndex], len);
    if (mVolume == 1.0f)
      memcpy (mBuffers[mBufferIndex], src, len);
    else {
      auto dst = (int16_t*)mBuffers[mBufferIndex];
      for (auto i = 0; i < len/ 2; i++)
        *dst++ = (int16_t)(*src++ * mVolume);
      }

    // queue buffer
    XAUDIO2_BUFFER xAudio2_buffer;
    memset ((void*)&xAudio2_buffer, 0, sizeof (XAUDIO2_BUFFER));
    xAudio2_buffer.AudioBytes = len;
    xAudio2_buffer.pAudioData = mBuffers[mBufferIndex];
    HRESULT hr = mxAudio2SourceVoice->SubmitSourceBuffer (&xAudio2_buffer);
    if (hr != S_OK) {
      printf ("XAudio2 - SubmitSourceBufferCreate failed\n");
      return;
      }

    // printf ("winAudioPlay %3.1f\n", pitch);
    if ((pitch > 0.005f) && (pitch < 4.0f))
      mxAudio2SourceVoice->SetFrequencyRatio (pitch, XAUDIO2_COMMIT_NOW);

    // cycle round buffers
    mBufferIndex = (mBufferIndex+1) % NUM_BUFFERS;

    // wait for buffer free if none left
    XAUDIO2_VOICE_STATE xAudio_voice_state;
    mxAudio2SourceVoice->GetState (&xAudio_voice_state);
    if (xAudio_voice_state.BuffersQueued >= NUM_BUFFERS)
      WaitForSingleObject (mAudio2VoiceCallback.hBufferEndEvent, INFINITE);
    }
  //}}}
  //{{{
  void audSilence() {

    audPlay (mSilence, 4096, 1.0f);
    }
  //}}}
  //{{{
  void audClose() {

    HRESULT hr = mxAudio2SourceVoice->Stop();
    hr = mxAudio2->Release();
    }
  //}}}

  float mVolume = 1.0f;

private:
  int16_t* mSilence;

  IXAudio2* mxAudio2;
  IXAudio2MasteringVoice* mxAudio2MasteringVoice;
  IXAudio2SourceVoice* mxAudio2SourceVoice;

  int mBufferIndex = 0;
  BYTE* mBuffers [NUM_BUFFERS] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

  cAudio2VoiceCallback mAudio2VoiceCallback;
  };
