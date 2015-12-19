// hacked qedit.h for sampleGrabber
#pragma once

#ifdef __cplusplus
  extern "C"{
#endif

// interface ISampleGrabberCB
EXTERN_C const IID IID_ISampleGrabberCB;
#if defined(__cplusplus) && !defined(CINTERFACE)
  MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
  //{{{
  ISampleGrabberCB : public IUnknown
  {
  public:
      virtual HRESULT STDMETHODCALLTYPE SampleCB(
          double SampleTime,
          IMediaSample *pSample) = 0;

      virtual HRESULT STDMETHODCALLTYPE BufferCB(
          double SampleTime,
          BYTE *pBuffer,
          long BufferLen) = 0;

  };
  //}}}
#else /* C style interface */
  //{{{
      typedef struct ISampleGrabberCBVtbl
      {
          BEGIN_INTERFACE

          HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
              ISampleGrabberCB * This,
              /* [in] */ REFIID riid,
              /* [iid_is][out] */ void **ppvObject);

          ULONG ( STDMETHODCALLTYPE *AddRef )(
              ISampleGrabberCB * This);

          ULONG ( STDMETHODCALLTYPE *Release )(
              ISampleGrabberCB * This);

          HRESULT ( STDMETHODCALLTYPE *SampleCB )(
              ISampleGrabberCB * This,
              double SampleTime,
              IMediaSample *pSample);

          HRESULT ( STDMETHODCALLTYPE *BufferCB )(
              ISampleGrabberCB * This,
              double SampleTime,
              BYTE *pBuffer,
              long BufferLen);

          END_INTERFACE
      } ISampleGrabberCBVtbl;

      interface ISampleGrabberCB
      {
          CONST_VTBL struct ISampleGrabberCBVtbl *lpVtbl;
      };

  #ifdef COBJMACROS
  #define ISampleGrabberCB_QueryInterface(This,riid,ppvObject)  \
      (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

  #define ISampleGrabberCB_AddRef(This) \
      (This)->lpVtbl -> AddRef(This)

  #define ISampleGrabberCB_Release(This)  \
      (This)->lpVtbl -> Release(This)


  #define ISampleGrabberCB_SampleCB(This,SampleTime,pSample)  \
      (This)->lpVtbl -> SampleCB(This,SampleTime,pSample)

  #define ISampleGrabberCB_BufferCB(This,SampleTime,pBuffer,BufferLen)  \
      (This)->lpVtbl -> BufferCB(This,SampleTime,pBuffer,BufferLen)
  #endif /* COBJMACROS */
  #endif  /* C style interface */
  //}}}

// interface ISampleGrabber
EXTERN_C const IID IID_ISampleGrabber;
#if defined(__cplusplus) && !defined(CINTERFACE)
  MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
  //{{{
  ISampleGrabber : public IUnknown
  {
  public:
      virtual HRESULT STDMETHODCALLTYPE SetOneShot(
          BOOL OneShot) = 0;

      virtual HRESULT STDMETHODCALLTYPE SetMediaType(
          const AM_MEDIA_TYPE *pType) = 0;

      virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
          AM_MEDIA_TYPE *pType) = 0;

      virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
          BOOL BufferThem) = 0;

      virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
          /* [out][in] */ long *pBufferSize,
          /* [out] */ long *pBuffer) = 0;

      virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
          /* [retval][out] */ IMediaSample **ppSample) = 0;

      virtual HRESULT STDMETHODCALLTYPE SetCallback(
          ISampleGrabberCB *pCallback,
          long WhichMethodToCallback) = 0;

  };
  //}}}
#else /* C style interface */
  //{{{
      typedef struct ISampleGrabberVtbl
      {
          BEGIN_INTERFACE
          HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
              ISampleGrabber * This,
              /* [in] */ REFIID riid,
              /* [iid_is][out] */ void **ppvObject);

          ULONG ( STDMETHODCALLTYPE *AddRef )(
              ISampleGrabber * This);

          ULONG ( STDMETHODCALLTYPE *Release )(
              ISampleGrabber * This);

          HRESULT ( STDMETHODCALLTYPE *SetOneShot )(
              ISampleGrabber * This,
              BOOL OneShot);

          HRESULT ( STDMETHODCALLTYPE *SetMediaType )(
              ISampleGrabber * This,
              const AM_MEDIA_TYPE *pType);

          HRESULT ( STDMETHODCALLTYPE *GetConnectedMediaType )(
              ISampleGrabber * This,
              AM_MEDIA_TYPE *pType);

          HRESULT ( STDMETHODCALLTYPE *SetBufferSamples )(
              ISampleGrabber * This,
              BOOL BufferThem);

          HRESULT ( STDMETHODCALLTYPE *GetCurrentBuffer )(
              ISampleGrabber * This,
              /* [out][in] */ long *pBufferSize,
              /* [out] */ long *pBuffer);

          HRESULT ( STDMETHODCALLTYPE *GetCurrentSample )(
              ISampleGrabber * This,
              /* [retval][out] */ IMediaSample **ppSample);

          HRESULT ( STDMETHODCALLTYPE *SetCallback )(
              ISampleGrabber * This,
              ISampleGrabberCB *pCallback,
              long WhichMethodToCallback);

          END_INTERFACE
      } ISampleGrabberVtbl;

      interface ISampleGrabber
      {
          CONST_VTBL struct ISampleGrabberVtbl *lpVtbl;
      };

  #ifdef COBJMACROS

  #define ISampleGrabber_QueryInterface(This,riid,ppvObject)  \
      (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

  #define ISampleGrabber_AddRef(This) \
      (This)->lpVtbl -> AddRef(This)

  #define ISampleGrabber_Release(This)  \
      (This)->lpVtbl -> Release(This)


  #define ISampleGrabber_SetOneShot(This,OneShot) \
      (This)->lpVtbl -> SetOneShot(This,OneShot)

  #define ISampleGrabber_SetMediaType(This,pType) \
      (This)->lpVtbl -> SetMediaType(This,pType)

  #define ISampleGrabber_GetConnectedMediaType(This,pType)  \
      (This)->lpVtbl -> GetConnectedMediaType(This,pType)

  #define ISampleGrabber_SetBufferSamples(This,BufferThem)  \
      (This)->lpVtbl -> SetBufferSamples(This,BufferThem)

  #define ISampleGrabber_GetCurrentBuffer(This,pBufferSize,pBuffer) \
      (This)->lpVtbl -> GetCurrentBuffer(This,pBufferSize,pBuffer)

  #define ISampleGrabber_GetCurrentSample(This,ppSample)  \
      (This)->lpVtbl -> GetCurrentSample(This,ppSample)

  #define ISampleGrabber_SetCallback(This,pCallback,WhichMethodToCallback)  \
      (This)->lpVtbl -> SetCallback(This,pCallback,WhichMethodToCallback)
  #endif /* COBJMACROS */
  #endif  /* C style interface */
  //}}}

EXTERN_C const CLSID CLSID_SampleGrabber;

#ifdef __cplusplus
  }
#endif
