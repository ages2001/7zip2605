/* Isolated in-process COM server used only by ole_tests.c. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
static const CLSID testClass={0x72bf97c1,0x8d52,0x469a,{0xa4,0x31,0x79,0x73,0x62,0x16,0xef,0x20}};
static BOOL equal(REFIID a,REFIID b){const DWORD *x=(const DWORD*)a,*y=(const DWORD*)b;return x[0]==y[0]&&x[1]==y[1]&&x[2]==y[2]&&x[3]==y[3];}
typedef struct Object{IUnknown iface;LONG refs;} Object;
static ULONG STDMETHODCALLTYPE add(IUnknown *s){return InterlockedIncrement(&((Object*)s)->refs);}
static ULONG STDMETHODCALLTYPE drop(IUnknown *s){ULONG n=InterlockedDecrement(&((Object*)s)->refs);if(!n)HeapFree(GetProcessHeap(),0,s);return n;}
static HRESULT STDMETHODCALLTYPE query(IUnknown *s,REFIID id,void **out){if(!out)return E_POINTER;*out=NULL;if(!equal(id,&IID_IUnknown))return E_NOINTERFACE;*out=s;add(s);return S_OK;}
static IUnknownVtbl objectTable={query,add,drop};
static ULONG STDMETHODCALLTYPE factoryAdd(IClassFactory *s){(void)s;return 2;}
static ULONG STDMETHODCALLTYPE factoryDrop(IClassFactory *s){(void)s;return 1;}
static HRESULT STDMETHODCALLTYPE factoryQuery(IClassFactory *s,REFIID id,void **out){if(!out)return E_POINTER;*out=NULL;if(!equal(id,&IID_IUnknown)&&!equal(id,&IID_IClassFactory))return E_NOINTERFACE;*out=s;return S_OK;}
static HRESULT STDMETHODCALLTYPE create(IClassFactory *s,IUnknown *outer,REFIID id,void **out){Object *p;HRESULT hr;(void)s;if(!out)return E_POINTER;*out=NULL;if(outer)return CLASS_E_NOAGGREGATION;p=HeapAlloc(GetProcessHeap(),0,sizeof(*p));if(!p)return E_OUTOFMEMORY;p->iface.lpVtbl=&objectTable;p->refs=1;hr=query(&p->iface,id,out);drop(&p->iface);return hr;}
static HRESULT STDMETHODCALLTYPE lock(IClassFactory *s,BOOL value){(void)s;(void)value;return S_OK;}
static IClassFactoryVtbl factoryTable={factoryQuery,factoryAdd,factoryDrop,create,lock};
static IClassFactory factory={&factoryTable};
HRESULT WINAPI __declspec(dllexport) DllGetClassObject(REFCLSID clsid,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(!equal(clsid,&testClass))return CLASS_E_CLASSNOTAVAILABLE;return factoryQuery(&factory,iid,out);}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){(void)instance;(void)reason;(void)reserved;return TRUE;}
