// yuv2rgbsse2.h
#ifdef __cplusplus
  extern "C" {
#endif

void yuv420_rgba32_sse2 (void* fromYPtr, void* fromUPtr, void* fromVPtr, void* toPtr, _int64 width);

#ifdef __cplusplus
  }
#endif
