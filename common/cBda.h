// cBda.h
#pragma once
//{{{  includes
#include <initguid.h>
#include <DShow.h>
#include <tuner.h>
#include <bdaiface.h>
#include <ks.h>
#include <ksmedia.h>
#include <bdamedia.h>
#include <bdatif.h>

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

#pragma comment(lib,"strmiids")

using namespace Microsoft::WRL;

// guids
DEFINE_GUID (CLSID_DVBTLocator,
             0x9CD64701, 0xBDF3, 0x4d14, 0x8E,0x03, 0xF1,0x29,0x83,0xD8,0x66,0x64);
DEFINE_GUID (CLSID_BDAtif,
             0xFC772ab0, 0x0c7f, 0x11d3, 0x8F,0xf2, 0x00,0xa0,0xc9,0x22,0x4c,0xf4);

//}}}
//#define BDA_COMMENTS

uint8_t* bdaEndPtr = nullptr;

//{{{
class cSampleGrabCB : public ISampleGrabberCB {
public:
  cSampleGrabCB() {};
  virtual ~cSampleGrabCB() {};

private:
  // ISampleGrabberCB methods
  STDMETHODIMP_(ULONG) AddRef() { return ++ul_cbrc; }
  STDMETHODIMP_(ULONG) Release() { return --ul_cbrc; }
  STDMETHODIMP QueryInterface (REFIID /*riid*/, void** /*p_p_object*/) { return E_NOTIMPL; }

  //{{{

  STDMETHODIMP cSampleGrabCB::BufferCB (double sampleTime, BYTE* samples, long sampleLen ) {

    printf ("BufferCB\n");
    return S_OK;
    }
  //}}}
  //{{{
  STDMETHODIMP cSampleGrabCB::SampleCB (double sampleTime, IMediaSample* mediaSample) {

    if (mediaSample->IsDiscontinuity() == S_OK)
      printf ("cSampleGrabCB::SampleCB sample Discontinuity\n");

    BYTE* samples;
    mediaSample->GetPointer (&samples);

    memcpy (bdaEndPtr, samples, mediaSample->GetActualDataLength());
    bdaEndPtr += mediaSample->GetActualDataLength();

    return S_OK;
    }
  //}}}

  // vars
  ULONG ul_cbrc;
  };
//}}}

class cBda {
public:
  cBda() {}
  ~cBda() {}
  //{{{
  bool  connectPins (ComPtr<IGraphBuilder> graphBuilder,
                     ComPtr<IBaseFilter> fromFilter, ComPtr<IBaseFilter> toFilter,
                     wchar_t* fromPinName = NULL, wchar_t* toPinName = NULL) {
  // connect pins, if name == NULL use first correct direction unconnected pin, else use pin matching name

    ComPtr<IEnumPins> fromPins;
    fromFilter->EnumPins (&fromPins);

    ComPtr<IPin> fromPin;
    while (fromPins->Next (1, &fromPin, NULL) == S_OK) {
      ComPtr<IPin> connectedPin;
      fromPin->ConnectedTo (&connectedPin);
      if (!connectedPin) {
        // match fromPin info
        PIN_INFO fromPinInfo;
        fromPin->QueryPinInfo (&fromPinInfo);
        if ((fromPinInfo.dir == PINDIR_OUTPUT) && (!fromPinName || !wcscmp (fromPinInfo.achName, fromPinName))) {
          // found fromPin, look for toPin
          ComPtr<IEnumPins> toPins;
          toFilter->EnumPins (&toPins);

          ComPtr<IPin> toPin;
          while (toPins->Next (1, &toPin, NULL) == S_OK) {
            ComPtr<IPin> connectedPin;
            toPin->ConnectedTo (&connectedPin);
            if (!connectedPin) {
              // match toPin info
              PIN_INFO toPinInfo;
              toPin->QueryPinInfo (&toPinInfo);
              if ((toPinInfo.dir == PINDIR_INPUT) && (!toPinName || !wcscmp (toPinInfo.achName, toPinName))) {
                // found toPin
                if (graphBuilder->Connect(fromPin.Get(), toPin.Get()) == S_OK) {
                  #ifdef BDA_COMMENTS
                  wprintf(L"- connecting pin %s to %s\n", fromPinInfo.achName, toPinInfo.achName);
                  #endif
                  return true;
                  }
                else {
                  #ifdef BDA_COMMENTS
                  printf ("- connectPins failed\n");
                  #endif
                  return false;
                  }
                }
              }
            }
          printf ("connectPins - no toPin\n");
          return false;
          }
        }
      }

    printf ("connectPins - no fromPin\n");
    return false;
    }
  //}}}
  //{{{
  ComPtr<IBaseFilter> findFilter (ComPtr<IGraphBuilder> graphBuilder, const CLSID& clsid, wchar_t* name,
                                         ComPtr<IBaseFilter> fromFilter) {
  // Find instance of filter of type CLSID by name, add to graphBuilder, connect fromFilter

    ComPtr<IBaseFilter> filter;

    ComPtr<ICreateDevEnum> systemDevEnum;
    CoCreateInstance (CLSID_SystemDeviceEnum, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&systemDevEnum));

    ComPtr<IEnumMoniker> classEnumerator;
    systemDevEnum->CreateClassEnumerator (clsid, &classEnumerator, 0);

    if (classEnumerator) {
      int i = 1;
      IMoniker* moniker = NULL;
      ULONG fetched;
      while (classEnumerator->Next (1, &moniker, &fetched) == S_OK) {
        IPropertyBag* propertyBag;
        if (moniker->BindToStorage (NULL, NULL, IID_IPropertyBag, (void**)&propertyBag) == S_OK) {
          VARIANT varName;
          VariantInit (&varName);
          propertyBag->Read (L"FriendlyName", &varName, 0);
          VariantClear (&varName);

          #ifdef BDA_COMMENTS
          wprintf (L"FindFilter - %s:%d\n", varName.bstrVal, instance);
          #endif

          // bind the filter
          moniker->BindToObject (NULL, NULL, IID_IBaseFilter, (void**)(&filter));

          graphBuilder->AddFilter (filter.Get(), name);
          propertyBag->Release();

          if (connectPins (graphBuilder, fromFilter, filter)) {
            propertyBag->Release();
            break;
            }
          else
            graphBuilder->RemoveFilter (filter.Get());
          }
        propertyBag->Release();
        moniker->Release();
        }

      moniker->Release();
      }
    return filter;
    }
  //}}}
  //{{{
  ComPtr<IBaseFilter> createFilter (ComPtr<IGraphBuilder> graphBuilder, const CLSID& clsid,
                                           wchar_t* title, ComPtr<IBaseFilter> fromFilter) {
  // createFilter of type clsid, add to graphBuilder, connect fromFilter

    ComPtr<IBaseFilter> filter;
    CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&filter));

    graphBuilder->AddFilter (filter.Get(), title);
    connectPins (graphBuilder, fromFilter, filter);

    #ifdef BDA_COMMENTS
    wprintf (L"CreateFilter %s\n", title);
    #endif

    return filter;
    }
  //}}}

  // private interface to render
  //{{{
  int getSignalStrength() {

    long strength = 0;

    if (mScanningTuner)
      mScanningTuner->get_SignalStrength (&strength);

    return strength / 100000;
    }
  //}}}

  //{{{
  void renderBDAGraph (ID2D1DeviceContext* d2dContext, D2D1_SIZE_F client,
                       IDWriteTextFormat* textFormat,
                       ID2D1SolidColorBrush* whiteBrush,
                       ID2D1SolidColorBrush* blueBrush,
                       ID2D1SolidColorBrush* blackBrush,
                       ID2D1SolidColorBrush* greyBrush) {

    if (mGraphBuilder) {
      wchar_t wStr[200];
      swprintf (wStr, 200, L"%2d", getSignalStrength());

      D2D1_RECT_F textRect = D2D1::RectF((float)client.width-20, 0.0f, (float)client.width, 20.0f);
      d2dContext->DrawText (wStr, (UINT32)wcslen(wStr), textFormat, textRect, whiteBrush);
      }
    }
  //}}}

  //{{{
  uint8_t* getBda (int len) {

    while (mBdaPtr + len > bdaEndPtr)
      Sleep (2);

    uint8_t* ptr = mBdaPtr;
    mBdaPtr += len;
    return ptr;
    }
  //}}}
  //{{{
  uint8_t* createBDAGraph (int freq, int bufSize) {

    printf ("BDAcreate\n");
    CoCreateInstance (CLSID_FilterGraph, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (mGraphBuilder.GetAddressOf()));

    ComPtr<IBaseFilter> dvbtNetworkProvider;
    CoCreateInstance (CLSID_DVBTNetworkProvider, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (dvbtNetworkProvider.GetAddressOf()));
    mGraphBuilder->AddFilter (dvbtNetworkProvider.Get(), L"dvbtNetworkProvider");

    // scanningTuner interface from dvbtNetworkProvider
    dvbtNetworkProvider.As (&mScanningTuner);

    ComPtr<ITuningSpace> tuningSpace;
    mScanningTuner->get_TuningSpace (tuningSpace.GetAddressOf());

    // setup dvbTuningSpace2 interface
    ComPtr<IDVBTuningSpace2> dvbTuningSpace2;
    tuningSpace.As (&dvbTuningSpace2);
    dvbTuningSpace2->put__NetworkType (CLSID_DVBTNetworkProvider);
    dvbTuningSpace2->put_SystemType (DVB_Terrestrial);
    dvbTuningSpace2->put_NetworkID (9018);
    dvbTuningSpace2->put_FrequencyMapping (L"");
    dvbTuningSpace2->put_UniqueName (L"DTV DVB-T");
    dvbTuningSpace2->put_FriendlyName (L"DTV DVB-T");

    // create dvbtLocator and setup in dvbTuningSpace2 interface
    ComPtr<IDVBTLocator> dvbtLocator;
    CoCreateInstance (CLSID_DVBTLocator, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (dvbtLocator.GetAddressOf()));
    dvbtLocator->put_CarrierFrequency (freq);
    dvbtLocator->put_Bandwidth (8);
    dvbTuningSpace2->put_DefaultLocator (dvbtLocator.Get());

    // tuneRequest from scanningTuner
    ComPtr<ITuneRequest> tuneRequest;
    HRESULT hr = mScanningTuner->get_TuneRequest (tuneRequest.GetAddressOf());
    if (hr != S_OK)
      hr = tuningSpace->CreateTuneRequest (tuneRequest.GetAddressOf());

    tuneRequest->put_Locator (dvbtLocator.Get());
    mScanningTuner->Validate (tuneRequest.Get());
    mScanningTuner->put_TuneRequest (tuneRequest.Get());

    // dvbtNetworkProvider -> dvbtTuner -> dvbtCapture -> sampleGrabberFilter -> mpeg2Demux -> bdaTif
    ComPtr<IBaseFilter> dvbtTuner  =
      findFilter (mGraphBuilder, KSCATEGORY_BDA_NETWORK_TUNER, L"DVBTtuner", dvbtNetworkProvider);
    if (!dvbtTuner)
      return nullptr;

    ComPtr<IBaseFilter> dvbtCapture =
      findFilter (mGraphBuilder, KSCATEGORY_BDA_RECEIVER_COMPONENT, L"DVBTcapture", dvbtTuner);

    ComPtr<IBaseFilter> sampleGrabberFilter =
      createFilter (mGraphBuilder, CLSID_SampleGrabber, L"grabber", dvbtCapture);
    ComPtr<ISampleGrabber> sampleGrabber;
    sampleGrabberFilter.As (&sampleGrabber);
    sampleGrabber->SetOneShot (false);
    sampleGrabber->SetBufferSamples (true);
    sampleGrabber->SetCallback (new cSampleGrabCB(), 0);

    ComPtr<IBaseFilter> mpeg2Demux =
      createFilter (mGraphBuilder, CLSID_MPEG2Demultiplexer, L"MPEG2demux", sampleGrabberFilter);
    ComPtr<IBaseFilter> bdaTif =
      createFilter (mGraphBuilder, CLSID_BDAtif, L"BDAtif", mpeg2Demux);

    mBdaBuf = (uint8_t*) malloc (bufSize);
    mBdaPtr = mBdaBuf;
    bdaEndPtr = mBdaBuf;

    ComPtr<IMediaControl> mediaControl;
    mGraphBuilder.As (&mediaControl);
    mediaControl->Run();

    printf ("- running\n");
    return mBdaBuf;
    }
  //}}}

  // vars
  ComPtr<IGraphBuilder> mGraphBuilder;   // ensure graph persists
  ComPtr<IScanningTuner> mScanningTuner; // for signalStrength

  uint8_t* mBdaBuf = nullptr;
  uint8_t* mBdaPtr = nullptr;
  };
