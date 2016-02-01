#pragma once

#define KJMP2_MAX_FRAME_SIZE    1440  // the maximum size of a frame
#define KJMP2_SAMPLES_PER_FRAME 1152  // the number of samples per frame

// kjmp2_context_t: A kjmp2 context record. You don't need to use or modify
// any of the values inside this structure; just pass the whole structure
// to the kjmp2_* functions
typedef struct _kjmp2_context {
  int id;
  int V[2][1024];
  int Voffs;
  } kjmp2_context_t;


// kjmp2_init: This function must be called once to initialize each kjmp2  decoder instance.
extern void kjmp2_init (kjmp2_context_t *mp2);

// kjmp2_get_sample_rate: Returns the sample rate of a MP2 stream.
// frame: Points to at least the first three bytes of a frame from the
//        stream.
// return value: The sample rate of the stream in Hz, or zero if the stream
//               isn't valid.
extern int kjmp2_get_sample_rate (const unsigned char *frame);


// kjmp2_decode_frame: Decode one frame of audio.
// mp2: A pointer to a context record that has been initialized with
//      kjmp2_init before.
// frame: A pointer to the frame to decode. It *must* be a complete frame,
//        because no error checking is done!
// pcm: A pointer to the output PCM data. kjmp2_decode_frame() will always
//      return 1152 (=KJMP2_SAMPLES_PER_FRAME) interleaved stereo samples
//      in a native-endian 16-bit signed format. Even for mono streams,
//      stereo output will be produced.
// return value: The number of bytes in the current frame. In a valid stream,
//               frame + kjmp2_decode_frame(..., frame, ...) will point to
//               the next frame, if frames are consecutive in memory.
// Note: pcm may be NULL. In this case, kjmp2_decode_frame() will return the
//       size of the frame without actually decoding it.
extern unsigned long kjmp2_decode_frame (kjmp2_context_t *mp2,
                                         const unsigned char *frame, signed short *pcm);
