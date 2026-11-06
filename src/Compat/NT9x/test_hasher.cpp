/* Test-only external module. It is never included in the release payload.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../../CPP/Common/MyInitGuid.h"
#include "../../CPP/7zip/ICoder.h"
#include <new>
#include <string.h>
static bool equal(REFIID a,REFIID b){return !memcmp(&a,&b,sizeof(GUID));}
class TestHasher:public IHasher{
 LONG refs;UInt32 sum;
public:
 TestHasher():refs(1),sum(2166136261UL){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out) noexcept override{if(!out)return E_POINTER;*out=NULL;if(!equal(id,IID_IUnknown)&&!equal(id,IID_IHasher))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() noexcept override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() noexcept override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
 void STDMETHODCALLTYPE Init() noexcept override{sum=2166136261UL;}
 void STDMETHODCALLTYPE Update(const void *data,UInt32 size) noexcept override{const Byte *p=(const Byte*)data;while(size--)sum=(sum^*p++)*16777619UL;}
 void STDMETHODCALLTYPE Final(Byte *digest) noexcept override{for(unsigned i=0;i<4;i++)digest[i]=(Byte)(sum>>(i*8));}
 UInt32 STDMETHODCALLTYPE GetDigestSize() noexcept override{return 4;}
};
class TestHashers:public IHashers{
 LONG refs;
public:
 TestHashers():refs(1){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out) noexcept override{if(!out)return E_POINTER;*out=NULL;if(!equal(id,IID_IUnknown)&&!equal(id,IID_IHashers))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() noexcept override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() noexcept override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
 UInt32 STDMETHODCALLTYPE GetNumHashers() noexcept override{return 1;}
 HRESULT STDMETHODCALLTYPE GetHasherProp(UInt32 index,PROPID prop,PROPVARIANT *value) noexcept override{
  if(!value)return E_POINTER;ZeroMemory(value,sizeof(*value));if(index)return E_INVALIDARG;
  switch(prop){case NMethodPropID::kID:value->vt=VT_UI8;value->uhVal.QuadPart=0x7f2605;break;case NMethodPropID::kName:value->bstrVal=SysAllocString(L"NT9xTest");if(!value->bstrVal)return E_OUTOFMEMORY;value->vt=VT_BSTR;break;case NMethodPropID::kDigestSize:value->vt=VT_UI4;value->ulVal=4;break;}
  return S_OK;
 }
 HRESULT STDMETHODCALLTYPE CreateHasher(UInt32 index,IHasher **out) noexcept override{if(!out)return E_POINTER;*out=NULL;if(index)return E_INVALIDARG;*out=new(std::nothrow) TestHasher;return *out?S_OK:E_OUTOFMEMORY;}
};
extern "C" HRESULT WINAPI GetHashers(IHashers **out){if(!out)return E_POINTER;*out=new(std::nothrow) TestHashers;return *out?S_OK:E_OUTOFMEMORY;}
extern "C" HRESULT WINAPI GetModuleProp(PROPID prop,PROPVARIANT *value){if(!value)return E_POINTER;ZeroMemory(value,sizeof(*value));if(prop==NModulePropID::kInterfaceType){value->vt=VT_UI4;value->ulVal=0;}return S_OK;}
