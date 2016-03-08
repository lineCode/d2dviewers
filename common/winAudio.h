// winAudio.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void winAudioOpen (int sampleFreq, int bitsPerSample, int channels);
void winAudioPlay (const void* buff, int len, float pitch, float volume);
void winAudioClose();

#ifdef __cplusplus
}
#endif
