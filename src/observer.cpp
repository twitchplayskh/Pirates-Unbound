#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <atomic>
#include <mutex>
#include "MinHook.h"
static HMODULE self;
static FILE* logFile;
static std::mutex logMutex;
static std::atomic<unsigned> frame{0},rows{0};
static bool sample(){return frame.load()%300==0 && rows.fetch_add(1)<50000;}
static void log(const char* fmt,...){std::lock_guard<std::mutex> guard(logMutex);if(!logFile)return;va_list a;va_start(a,fmt);std::vfprintf(logFile,fmt,a);va_end(a);std::fputc('\n',logFile);std::fflush(logFile);}
using Present=HRESULT(WINAPI*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
using Transform=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
using Viewport=HRESULT(WINAPI*)(IDirect3DDevice9*,const D3DVIEWPORT9*);
using Constants=HRESULT(WINAPI*)(IDirect3DDevice9*,UINT,const float*,UINT);
using DrawIndexed=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
using Draw=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
using DrawUP=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
static Present present;static Transform transform;static Viewport viewport;static Constants constants;static DrawIndexed drawIndexed;static Draw draw;static DrawUP drawUP;
static HRESULT WINAPI onPresent(IDirect3DDevice9*d,const RECT*a,const RECT*b,HWND w,const RGNDATA*r){auto hr=present(d,a,b,w,r);auto f=++frame;if(f%300==1){IDirect3DSurface9*bb=nullptr;if(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb))){D3DSURFACE_DESC desc{};bb->GetDesc(&desc);log("frame %u backbuffer %ux%u present %lx",f,desc.Width,desc.Height,static_cast<unsigned long>(hr));bb->Release();}}return hr;}
static HRESULT WINAPI onTransform(IDirect3DDevice9*d,D3DTRANSFORMSTATETYPE t,const D3DMATRIX*m){if(m&&sample()&&(t==D3DTS_PROJECTION||t==D3DTS_VIEW))log("T frame=%u type=%u caller=%p matrix=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g",frame.load(),t,__builtin_return_address(0),m->_11,m->_12,m->_13,m->_14,m->_21,m->_22,m->_23,m->_24,m->_31,m->_32,m->_33,m->_34,m->_41,m->_42,m->_43,m->_44);return transform(d,t,m);}
static HRESULT WINAPI onViewport(IDirect3DDevice9*d,const D3DVIEWPORT9*v){if(v&&sample())log("V frame=%u caller=%p x=%lu y=%lu w=%lu h=%lu z=%g,%g",frame.load(),__builtin_return_address(0),v->X,v->Y,v->Width,v->Height,v->MinZ,v->MaxZ);return viewport(d,v);}
static HRESULT WINAPI onConstants(IDirect3DDevice9*d,UINT start,const float*v,UINT count){if(v&&sample()){log("C frame=%u start=%u count=%u caller=%p",frame.load(),start,count,__builtin_return_address(0));for(UINT i=0;i<count&&i<16;++i)log("  c%u=%g,%g,%g,%g",start+i,v[i*4],v[i*4+1],v[i*4+2],v[i*4+3]);}return constants(d,start,v,count);}
static void drawState(IDirect3DDevice9*d,const char*name,void*caller,unsigned count){if(!sample())return;DWORD fvf=0;D3DVIEWPORT9 v{};D3DMATRIX m{};IDirect3DVertexShader9*vs=nullptr;IDirect3DPixelShader9*ps=nullptr;d->GetFVF(&fvf);d->GetViewport(&v);d->GetTransform(D3DTS_PROJECTION,&m);d->GetVertexShader(&vs);d->GetPixelShader(&ps);log("D %s frame=%u caller=%p count=%u fvf=%lx vs=%p ps=%p viewport=%lu,%lu,%lu,%lu proj=%g,%g,%g,%g",name,frame.load(),caller,count,fvf,vs,ps,v.X,v.Y,v.Width,v.Height,m._11,m._22,m._34,m._44);if(vs)vs->Release();if(ps)ps->Release();}
static HRESULT WINAPI onDrawIndexed(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,INT b,UINT mn,UINT n,UINT s,UINT c){drawState(d,"indexed",__builtin_return_address(0),c);return drawIndexed(d,t,b,mn,n,s,c);}
static HRESULT WINAPI onDraw(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,UINT s,UINT c){drawState(d,"draw",__builtin_return_address(0),c);return draw(d,t,s,c);}
static HRESULT WINAPI onDrawUP(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,UINT c,const void*v,UINT stride){drawState(d,"up",__builtin_return_address(0),c);return drawUP(d,t,c,v,stride);}
template<class T>static bool hook(void*target,void*replacement,T&original){auto r=MH_CreateHook(target,replacement,reinterpret_cast<void**>(&original));log("hook %p status=%d",target,r);return r==MH_OK;}
static DWORD WINAPI init(void*){
 char path[MAX_PATH];GetModuleFileNameA(self,path,MAX_PATH);char*slash=strrchr(path,'\\');strcpy(slash+1,"observer.log");logFile=std::fopen(path,"w");
 log("Pirates D3D9 observer 0.1; pid=%lu; all graphics arguments pass through",GetCurrentProcessId());
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
 const unsigned char expected[]={0x8b,0x02,0x89,0x81,0x28,0x01,0x00,0x00};
 if(std::memcmp(base+0xcce74,expected,sizeof(expected))){log("Target code gate failed; no hooks installed");return 1;}
 IDirect3D9*d3d=Direct3DCreate9(D3D_SDK_VERSION);if(!d3d){log("Direct3DCreate9 failed");return 2;}
 WNDCLASSA wc{};wc.lpfnWndProc=DefWindowProcA;wc.hInstance=self;wc.lpszClassName="PiratesObserverProbe";RegisterClassA(&wc);HWND w=CreateWindowA(wc.lpszClassName,"",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,self,nullptr);
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=w;pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_UNKNOWN;
 IDirect3DDevice9*d=nullptr;HRESULT hr=d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,w,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,&d);
 if(FAILED(hr)){log("Probe device failed %lx",static_cast<unsigned long>(hr));DestroyWindow(w);d3d->Release();return 3;}
 auto table=*reinterpret_cast<void***>(d);auto r=MH_Initialize();log("MH_Initialize=%d",r);bool ok=r==MH_OK;
 ok=ok&&hook(table[17],reinterpret_cast<void*>(onPresent),present);
 ok=ok&&hook(table[44],reinterpret_cast<void*>(onTransform),transform);
 ok=ok&&hook(table[47],reinterpret_cast<void*>(onViewport),viewport);
 ok=ok&&hook(table[94],reinterpret_cast<void*>(onConstants),constants);
 ok=ok&&hook(table[81],reinterpret_cast<void*>(onDraw),draw);
 ok=ok&&hook(table[82],reinterpret_cast<void*>(onDrawIndexed),drawIndexed);
 ok=ok&&hook(table[83],reinterpret_cast<void*>(onDrawUP),drawUP);
 if(ok){r=MH_EnableHook(MH_ALL_HOOKS);log("MH_EnableHook=%d",r);}else{MH_Uninitialize();log("Incomplete hook setup rolled back");}
 d->Release();d3d->Release();DestroyWindow(w);log("Observer initialization finished");return 0;
}
BOOL WINAPI DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){self=h;DisableThreadLibraryCalls(h);HANDLE t=CreateThread(nullptr,0,init,nullptr,0,nullptr);if(t)CloseHandle(t);}return TRUE;}
