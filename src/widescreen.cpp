#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <vector>
#include <utility>
#include "MinHook.h"
static HMODULE self;
static FILE* journal;
static std::atomic<int> width{0},height{0};
static std::atomic<unsigned> frames{0},uiChanges{0},worldChanges{0},inputs{0};
static IDirect3DDevice9* targetDevice;
static bool interfaceView=false;
static bool widenWorld=true,centerUi=true,fillBackgrounds=true;
static bool borderless=false;
static HWND gameWindow;
using Transform=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
using Present=HRESULT(WINAPI*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
static Transform originalTransform;
static Present originalPresent;
static Reset originalReset;
using DrawIndexed=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
static DrawIndexed originalDrawIndexed;
using DrawUP=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
static DrawUP originalDrawUP;
using Draw=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
static Draw originalDraw;
using DrawIndexedUP=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
static DrawIndexedUP originalDrawIndexedUP;
#include "antialiasing.h"
#include "diagnostics.h"
#include "performance.h"
#include "sailing.h"
#ifdef PIRATES_TIMING_DIAGNOSTIC
#include "timing-diagnostic.h"
#endif
#include "town-layout.h"
#include "tavern-light.h"
#include "sailing-observation.h"
static bool closeTo(float a,float b);
#include "cinematic-bars.h"
#include "map-expansion.h"
#include "map-decoration.h"
#include "texture-filtering.h"
// Stock full-screen NiScreenElements backgrounds occupy 640 by 480 units.
// Widen only these quads; buttons, text and maps retain the 4:3 camera.
enum class ScreenQuad { Other, Background, DialogueGradient, MapBacking, CinematicBar };
static ScreenQuad screenQuadKind(IDirect3DDevice9*d,INT base,UINT minimum,UINT count,UINT primitives){
 if(count!=4||primitives!=2)return ScreenQuad::Other;
 DWORD fvf=0;d->GetFVF(&fvf);if(fvf!=0x142)return ScreenQuad::Other;
 IDirect3DVertexBuffer9*vb=nullptr;UINT offset=0,stride=0;
 if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||!vb)return ScreenQuad::Other;
 void*p=nullptr;bool full=false,gradient=false,map=false,bar=false;const int first=base+minimum;
 const auto readStart=profileEnabled?performanceTick():0;
 if(first>=0&&stride>=12&&SUCCEEDED(vb->Lock(offset+first*stride,count*stride,&p,D3DLOCK_READONLY))){
  full=true;gradient=true;map=true;unsigned corners=0;
  const float planeY=reinterpret_cast<float*>(p)[1];
  for(unsigned i=0;i<4;++i){auto v=reinterpret_cast<float*>(static_cast<unsigned char*>(p)+i*stride);
   // Merchant backdrops use -0.52; the map's decorative backing uses -1.14.
   // Require the complete 640x480 coplanar rectangle, not a fixed depth.
   if(!closeTo(std::fabs(v[0]),320)||std::fabs(v[1])>2.f||!closeTo(v[1],planeY)||!closeTo(std::fabs(v[2]),240))full=false;
   // The stock dialogue backing is a separate untextured 642x242 quad.
   // Its text uses other draws and must keep the centered UI projection.
   if(!closeTo(std::fabs(v[0]),321)||!closeTo(v[1],0)||!closeTo(std::fabs(v[2]),121))gradient=false;
   if(!closeTo(std::fabs(v[0]),320)||std::fabs(v[1])>1.f||!closeTo(v[1],planeY)||(!closeTo(v[2],-200)&&!closeTo(v[2],240)))map=false;
   corners|=1u<<((v[0]>0?1:0)+(v[2]>0?2:0));
  }
  full=full&&corners==15;gradient=gradient&&corners==15;map=map&&corners==15;
  if(stride==sizeof(CinematicBarVertex)&&closeTo(std::fabs(reinterpret_cast<float*>(p)[0]),321)&&closeTo(std::fabs(reinterpret_cast<float*>(p)[2]),20)){
   D3DMATRIX world{};
   if(SUCCEEDED(d->GetTransform(D3DTS_WORLD,&world)))bar=cinematicBarGeometry(static_cast<const CinematicBarVertex*>(p),world);
  }
  vb->Unlock();
 }
 if(profileEnabled){quadReadTicks+=performanceTick()-readStart;++quadReads;}
 vb->Release();
 if(full)return ScreenQuad::Background;
 if(map)return ScreenQuad::MapBacking;
 if(gradient||bar){IDirect3DBaseTexture9*texture=nullptr;const HRESULT result=d->GetTexture(0,&texture);
  const bool untextured=SUCCEEDED(result)&&!texture;if(texture)texture->Release();
  if(untextured)return bar?ScreenQuad::CinematicBar:ScreenQuad::DialogueGradient;
 }return ScreenQuad::Other;
}
// Add decorative parchment margins. The actual map,
// labels, icons and picking keep the original centered coordinate system.
static void extendMapBacking(IDirect3DDevice9*d,INT base,UINT minimum){
 const int w=width.load(),h=height.load();
 if(!centerUi||!fillBackgrounds||h<=0||3*w<=4*h)return;
 IDirect3DBaseTexture9*texture=nullptr;D3DSURFACE_DESC desc{};
 if(FAILED(d->GetTexture(0,&texture))||!texture)return;
 const bool validTexture=texture->GetType()==D3DRTYPE_TEXTURE&&SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0,&desc))&&desc.Width==1024&&desc.Height==1024;
 texture->Release();if(!validTexture)return;
 static IDirect3DTexture9*paper=nullptr;
 if(!paper){
  IDirect3DTexture9*created=nullptr;
  if(FAILED(d->CreateTexture(128,128,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&created,nullptr)))return;
  D3DLOCKED_RECT pixels{};
  if(FAILED(created->LockRect(0,&pixels,nullptr,0))){created->Release();return;}
  unsigned seed=0x50415045;
  for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x){
   seed=1664525u*seed+1013904223u;const int grain=int(seed>>28)-8;
   static_cast<DWORD*>(static_cast<void*>(static_cast<unsigned char*>(pixels.pBits)+y*pixels.Pitch))[x]=D3DCOLOR_ARGB(255,239+grain,211+grain,161+grain);
  }
  created->UnlockRect(0);paper=created;
 }
 IDirect3DVertexBuffer9*vb=nullptr;UINT offset=0,stride=0;
 if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||!vb)return;
 struct Vertex {float x,y,z;DWORD color;float u,v;};Vertex original[4]{};void*data=nullptr;
 const int first=base+minimum;
 bool valid=first>=0&&stride==sizeof(Vertex)&&SUCCEEDED(vb->Lock(offset+first*stride,sizeof(original),&data,D3DLOCK_READONLY));
 if(valid){std::memcpy(original,data,sizeof(original));vb->Unlock();}vb->Release();if(!valid)return;
 IDirect3DStateBlock9*block=nullptr;if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&block)))return;
 if(FAILED(block->Capture())){block->Release();return;}
 d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
 d->SetTexture(0,paper);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
 d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
 const WORD indices[]={0,1,2,2,1,3};
 const float outer=320.f*(3.f*w/(4.f*h));
 for(int side=-1;side<=1;side+=2){
  Vertex edge[2]{};unsigned found=0;for(const auto&v:original)if(v.x*side>0&&found<2)edge[found++]=v;
  if(found!=2)continue;
  Vertex strip[4]={edge[0],edge[1],edge[0],edge[1]};
  for(unsigned i=0;i<4;++i){strip[i].u=i<2?0:std::max(1.f,(outer-320.f)/128.f);strip[i].v=(strip[i].z+200.f)/128.f;}
  strip[2].x=strip[3].x=side*outer;
  originalDrawIndexedUP(d,D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,strip,sizeof(Vertex));
 }
 block->Apply();block->Release();
}
// The tavern's additive scene-copy layer sits at y=-0.01 rather than zero.
// It contains already-rendered world pixels, so it must cover the same output
// as the main scene regardless of the optional menu-background setting.
static bool sceneComposite(IDirect3DDevice9*d){
 DWORD blend=0,destination=0;d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);d->GetRenderState(D3DRS_DESTBLEND,&destination);
 if(!blend||destination!=D3DBLEND_ONE)return false;
 IDirect3DBaseTexture9*texture=nullptr;bool composite=false;
 if(SUCCEEDED(d->GetTexture(0,&texture))&&texture){
  if(texture->GetType()==D3DRTYPE_TEXTURE){D3DSURFACE_DESC desc{};if(SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0,&desc)))composite=(desc.Usage&D3DUSAGE_RENDERTARGET)!=0;}
  texture->Release();
 }return composite;
}
static RECT uiClipRect(int w,int h,const D3DVIEWPORT9&viewport){
 // Offscreen scene captures can be square while storing the widescreen image.
 // Apply the output's normalized safe area inside the current viewport.
 const LONG safeWidth=static_cast<LONG>((static_cast<unsigned long long>(viewport.Width)*4*h)/(3*w));
 const LONG left=viewport.X+(static_cast<LONG>(viewport.Width)-safeWidth)/2;
 return RECT{left,static_cast<LONG>(viewport.Y),left+safeWidth,static_cast<LONG>(viewport.Y+viewport.Height)};
}
struct UiDrawState{
 IDirect3DDevice9*d;DWORD enabled=0;RECT old{};D3DMATRIX projection{};bool active=false,background=false;
 UiDrawState(IDirect3DDevice9*device,bool full):d(device){
  const int w=width.load(),h=height.load();
  if(!centerUi||d!=targetDevice||!interfaceView||3*w<=4*h||h<=0)return;
  if(FAILED(d->GetTransform(D3DTS_PROJECTION,&projection))||!closeTo(projection._33,2.5f)||!closeTo(projection._34,1))return;
  active=true;background=full;d->GetRenderState(D3DRS_SCISSORTESTENABLE,&enabled);d->GetScissorRect(&old);
  if(full){auto wide=projection;wide._11=2;originalTransform(d,D3DTS_PROJECTION,&wide);d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);}
  else{
   if(mapExpansionEnabled&&mapExpandedFrame==frames.load()){d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);return;}
   D3DVIEWPORT9 viewport{};d->GetViewport(&viewport);RECT safe=uiClipRect(w,h,viewport);
   if(enabled){safe.left=std::max(safe.left,old.left);safe.top=std::max(safe.top,old.top);safe.right=std::min(safe.right,old.right);safe.bottom=std::min(safe.bottom,old.bottom);}
   d->SetScissorRect(&safe);d->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);
  }
 }
 ~UiDrawState(){if(active){if(background)originalTransform(d,D3DTS_PROJECTION,&projection);d->SetScissorRect(&old);d->SetRenderState(D3DRS_SCISSORTESTENABLE,enabled);}}
};
static HRESULT WINAPI onDrawIndexed(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,INT base,UINT minimum,UINT count,UINT start,UINT primitives){
 if(sailingExtra&&sailingUiDisplay(d))return S_OK;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 timingTextDraw(d,"indexed",primitives,__builtin_return_address(0));
#endif
 if(traceEnabled){IDirect3DSurface9*s=nullptr;D3DSURFACE_DESC desc{};if(SUCCEEDED(d->GetRenderTarget(0,&s))){s->GetDesc(&desc);s->Release();}forceTrace=desc.Width==1024&&desc.Height==1024;
  if(forceTrace)std::fprintf(journal,"CAPTURE d=%p target=%p townActive=%d interface=%d\n",d,targetDevice,townLayoutActive,interfaceView);
 }
 traceDraw(d,"indexed",primitives,base,minimum,count);
 forceTrace=false;
 const auto quad=d==targetDevice&&interfaceView?screenQuadKind(d,base,minimum,count,primitives):ScreenQuad::Other;
 // Omit only the verified cinematic masks, before any draw-state changes.
 // Geometry, camera, subtitle draws and native cinematic timing stay intact.
 if(quad==ScreenQuad::CinematicBar&&t==D3DPT_TRIANGLESTRIP&&centerUi&&widenWorld&&height.load()>0&&3ll*width.load()>4ll*height.load()){
  D3DMATRIX view{},projection{};
  if(SUCCEEDED(d->GetTransform(D3DTS_VIEW,&view))&&SUCCEEDED(d->GetTransform(D3DTS_PROJECTION,&projection))&&closeTo(view._43,640)&&closeTo(projection._33,2.5f)&&closeTo(projection._34,1)){
   static unsigned notices=0;if(journal&&notices<4){++notices;std::fprintf(journal,"CINEMATIC mask omitted frame=%u\n",frames.load());std::fflush(journal);}
   return S_OK;
  }
 }
 if(quad==ScreenQuad::MapBacking){if(drawExpandedMap(d,base,minimum))return S_OK;extendMapBacking(d,base,minimum);}
 if(drawMapDecoration(d,t,base,minimum,count,primitives))return S_OK;
 bool expand=quad==ScreenQuad::DialogueGradient||(quad==ScreenQuad::Background&&(fillBackgrounds||sceneComposite(d)));
 UiDrawState state(d,expand);
 TownDrawState town(d,base,minimum,count,primitives);
 TavernLightState light(d,base,minimum,count,primitives);
 sailingUiIndexed(d,t,base,minimum,count,start,primitives);
 const int observation=obsDraw(d,primitives);
 TextureFilteringState filtering(d);
 return obsResult(observation,originalDrawIndexed(d,t,base,minimum,count,start,primitives));
}
static void fixExistingWorldCamera();
extern "C" { void* inputTrampoline=nullptr; }
extern "C" { void* frustumTrampoline=nullptr; }
struct Frustum {float left,right,top,bottom,nearPlane,farPlane;unsigned char orthographic;};
static thread_local Frustum temporaryFrustum;
static bool isWorldFrustum(const Frustum& f){
 const float dx=f.right-f.left,dy=f.top-f.bottom;
 return !f.orthographic&&dx>0&&dy>0&&f.nearPlane>0&&f.farPlane>f.nearPlane&&std::fabs(dx/dy-4.f/3.f)<0.0002f&&std::fabs(f.farPlane/f.nearPlane-5.f/3.f)>0.001f;
}
extern "C" const Frustum* fixFrustum(const Frustum* source){
 const int w=width.load(),h=height.load();if(w<=0||h<=0||3*w<=4*h)return source;
 // NiScreenElements' camera is constructed at 503CA0, then resized at 501040.
 // Its original widescreen vertical frustum culls small top/bottom UI pieces
 // before they reach D3D. Correct the engine bounds as well as the projection.
 if(centerUi&&!source->orthographic&&source->nearPlane>0&&closeTo(source->farPlane/source->nearPlane,5.f/3.f)&&closeTo(source->left,-0.5f)&&closeTo(source->right,0.5f)&&source->top>0&&source->top<=0.3751f&&closeTo(source->top,-source->bottom)){
  temporaryFrustum=*source;temporaryFrustum.left=-3.f*float(w)/(8.f*float(h));temporaryFrustum.right=-temporaryFrustum.left;
  temporaryFrustum.top=0.375f;temporaryFrustum.bottom=-0.375f;return &temporaryFrustum;
 }
 if(!widenWorld||!isWorldFrustum(*source))return source;
 temporaryFrustum=*source;const float scale=(3.f*w)/(4.f*h);temporaryFrustum.left*=scale;temporaryFrustum.right*=scale;return &temporaryFrustum;
}
extern "C" __attribute__((naked)) void frustumHook(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tpushl 20(%ebp)\n\tcall _fixFrustum\n\taddl $4,%esp\n\tmovl %eax,20(%ebp)\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tjmp *_frustumTrampoline\n\t");
}
extern "C" int remapUiX(int x){
 const int w=width.load(),h=height.load();
 if(!centerUi||w<=0||h<=0||3*w<=4*h)return x;
 ++inputs;
 // The stock UI picker divides x by the full render width. Convert the
 // centered 4:3 rectangle back to that stock range. No OS mouse manipulation.
 return static_cast<int>((static_cast<long long>(2*x-w)*3*w)/(8*h)+w/2);
}
static unsigned scaleMousePacket(unsigned packed,int w,int h,int clientW,int clientH){
 if(w<=0||h<=0||clientW<=0||clientH<=0)return packed;
 const int x=static_cast<int>((static_cast<long long>(static_cast<short>(packed))*w)/clientW);
 const int y=static_cast<int>((static_cast<long long>(static_cast<short>(packed>>16))*h)/clientH);
 return (static_cast<unsigned>(y)<<16)|(static_cast<unsigned>(x)&0xffffu);
}
static HRESULT WINAPI onDrawIndexedUP(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,UINT minimum,UINT count,UINT primitives,const void*indices,D3DFORMAT format,const void*data,UINT stride){
 if(sailingUiOther(d))return S_OK;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 timingTextDraw(d,"indexedUP",primitives,__builtin_return_address(0));
#endif
 traceDraw(d,"indexedUP",primitives,0,minimum,0);
 const int observation=obsDraw(d,primitives);
 TextureFilteringState filtering(d);
 return obsResult(observation,originalDrawIndexedUP(d,t,minimum,count,primitives,indices,format,data,stride));
}
static HRESULT WINAPI onDrawUP(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,UINT primitives,const void* data,UINT stride){
 if(sailingUiOther(d))return S_OK;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 timingTextDraw(d,"up",primitives,__builtin_return_address(0));
#endif
 if(d==targetDevice&&traceEnabled&&frames.load()%30==0){
  traceDraw(d,"up",primitives);
  if(primitives==2&&stride>=12){std::fprintf(journal,"UP type=%u stride=%u",unsigned(t),stride);for(unsigned i=0;i<4;++i){auto v=reinterpret_cast<const float*>(static_cast<const unsigned char*>(data)+i*stride);std::fprintf(journal," v%u=%g,%g,%g",i,v[0],v[1],v[2]);}std::fputc('\n',journal);}
 }
 const int observation=obsDraw(d,primitives);
 TextureFilteringState filtering(d);
 return obsResult(observation,originalDrawUP(d,t,primitives,data,stride));
}
static HRESULT WINAPI onDraw(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,UINT first,UINT primitives){
 if(sailingUiPrimitive(d,t,first,primitives))return S_OK;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 timingTextDraw(d,"primitive",primitives,__builtin_return_address(0));
#endif
 const UINT count=t==D3DPT_TRIANGLESTRIP||t==D3DPT_TRIANGLEFAN?primitives+2:primitives*3;
 traceDraw(d,"primitive",primitives,first,0,count);
 const int observation=obsDraw(d,primitives);
 TextureFilteringState filtering(d);
 return obsResult(observation,originalDraw(d,t,first,primitives));
}
extern "C" unsigned remapUiPacked(unsigned packed,unsigned message){
 if(message<0x200||message>0x209)return packed;
 if(borderless&&gameWindow){RECT client{};
  if(GetClientRect(gameWindow,&client)&&client.right>0&&client.bottom>0){
   packed=scaleMousePacket(packed,width.load(),height.load(),client.right,client.bottom);
  }
 }
 return (packed&0xffff0000u)|(static_cast<unsigned>(remapUiX(static_cast<short>(packed)))&0xffffu);
}
extern "C" __attribute__((naked)) void uiInputHook(){
 __asm__ __volatile__(
  "pushfl\n\tpushal\n\t"
  "movl 40(%esp),%eax\n\tmovl 28(%esp),%ecx\n\tpushl %ecx\n\tpushl %eax\n\t"
  "call _remapUiPacked\n\taddl $8,%esp\n\t"
  "movl %eax,40(%esp)\n\tpopal\n\tpopfl\n\tjmp *_inputTrampoline\n\t");
}
static bool closeTo(float a,float b){return std::fabs(a-b)<0.0002f;}
static HRESULT WINAPI onTransform(IDirect3DDevice9*d,D3DTRANSFORMSTATETYPE t,const D3DMATRIX*m){
 obsTransformPayload(d,t,m);
 if(d!=targetDevice||!m)return originalTransform(d,t,m);
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(t>=D3DTS_WORLD)sailingVisualMatrix(m);
 timingCameraMatrix(t,m);
#endif
 if(t==D3DTS_VIEW){
  interfaceView=closeTo(m->_11,1)&&closeTo(m->_12,0)&&closeTo(m->_13,0)&&closeTo(m->_21,0)&&closeTo(m->_22,0)&&closeTo(m->_23,1)&&closeTo(m->_31,0)&&closeTo(m->_32,1)&&closeTo(m->_33,0)&&closeTo(m->_41,0)&&closeTo(m->_42,0)&&m->_43>0;
  return originalTransform(d,t,m);
 }
 if(t!=D3DTS_PROJECTION)return originalTransform(d,t,m);
 const int w=width.load(),h=height.load();if(w<=0||h<=0||3*w<=4*h)return originalTransform(d,t,m);
 D3DMATRIX fixed=*m;
 const float ratio=(4.f*h)/(3.f*w);
 // The observed interface camera has near=960, far=1600, horizontal extent=1,
 // and vertical extent=H/W. Other perspective cameras are world/cutscene views.
 const bool ui=interfaceView&&closeTo(m->_11,2.f)&&closeTo(m->_33,2.5f)&&m->_43<0&&closeTo(m->_34,1.f)&&closeTo(m->_44,0.f);
 if(ui&&centerUi){fixed._11*=ratio;fixed._22=8.f/3.f;++uiChanges;}
 else if(!ui&&widenWorld&&closeTo(m->_34,1.f)&&closeTo(m->_44,0.f)&&closeTo(m->_31,0.f)&&closeTo(m->_32,0.f)&&m->_11>0&&m->_22>0&&closeTo(m->_22/m->_11,4.f/3.f)){
  // Preserve the vertical field of view, exposing additional world at the sides.
  fixed._11*=ratio;++worldChanges;
 }
 return originalTransform(d,t,&fixed);
}
static void updateSize(IDirect3DDevice9*d){IDirect3DSurface9*bb=nullptr;if(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb))){D3DSURFACE_DESC desc{};bb->GetDesc(&desc);width=desc.Width;height=desc.Height;bb->Release();}}
static HRESULT WINAPI onReset(IDirect3DDevice9*d,D3DPRESENT_PARAMETERS*p){if(d==targetDevice){obsDeviceReset();clearMapExpansion();clearMapDecoration();clearTownAssets();sailingUiClear();sailingDropBuffers();aaMain.release(d);}auto hr=originalReset(d,p);if(d==targetDevice&&SUCCEEDED(hr)){updateSize(d);interfaceView=false;}return hr;}
static HRESULT WINAPI onPresent(IDirect3DDevice9*d,const RECT*a,const RECT*b,HWND w,const RGNDATA*r){
 if(d==targetDevice)obsPresent();
 if(d==targetDevice)aaMain.resolve(d);
 if(d==targetDevice){updateTownMenu();recordPresent();fixExistingWorldCamera();const auto f=++frames;if(f%300==1){char toggle[MAX_PATH];std::snprintf(toggle,sizeof(toggle),"%strace.enabled",traceFolder);traceEnabled=GetFileAttributesA(toggle)!=INVALID_FILE_ATTRIBUTES;std::snprintf(toggle,sizeof(toggle),"%sprofile.enabled",traceFolder);profileEnabled=GetFileAttributesA(toggle)!=INVALID_FILE_ATTRIBUTES;updateSize(d);std::fprintf(journal,"frame=%u size=%dx%d UI=%u world=%u inputs=%u\n",f,width.load(),height.load(),uiChanges.load(),worldChanges.load(),inputs.load());std::fflush(journal);}}
#ifdef PIRATES_TIMING_DIAGNOSTIC
 const auto timingBegin=d==targetDevice?beginTimingPresent():0;
 timingHudPixels(d);
#endif
 if(d==targetDevice)beginSailingPresent();
 auto result=originalPresent(d,a,b,w,r);
 if(d==targetDevice)endSailingPresent(result);
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(d==targetDevice)endTimingPresent(timingBegin,result);
#endif
 return result;
}
static bool readable(const void*p,size_t bytes){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;return reinterpret_cast<uintptr_t>(p)+bytes<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;}
static void fixExistingWorldCamera(){
 auto camera=*reinterpret_cast<unsigned char**>(0x8e9fd8);
 if(!readable(camera,0x168))return;
 auto f=reinterpret_cast<Frustum*>(camera+0x128);const auto fixed=fixFrustum(f);
 if(fixed!=f){*f=*fixed;using Update=void(__attribute__((thiscall))*)(void*);reinterpret_cast<Update>(0x536f80)(camera);}
}
static DWORD WINAPI init(void*){
 QueryPerformanceFrequency(&performanceFrequency);
 char settingsPath[MAX_PATH];GetModuleFileNameA(self,settingsPath,MAX_PATH);strcpy(strrchr(settingsPath,'\\')+1,"PiratesWide.ini");
 widenWorld=GetPrivateProfileIntA("Widescreen","World",1,settingsPath)!=0;
 centerUi=GetPrivateProfileIntA("Widescreen","CenterUI",1,settingsPath)!=0;
 fillBackgrounds=GetPrivateProfileIntA("Widescreen","FillBackgrounds",1,settingsPath)!=0;
 borderless=GetPrivateProfileIntA("Widescreen","Borderless",0,settingsPath)!=0;
 traceEnabled=GetPrivateProfileIntA("Widescreen","Trace",0,settingsPath)!=0;
 mapExpansionEnabled=GetPrivateProfileIntA("Widescreen","MapExpansionTrial",0,settingsPath)!=0;
 sailingSeparate=GetPrivateProfileIntA("Widescreen","ExperimentalSailing120",0,settingsPath)!=0;
 aaRequested=GetPrivateProfileIntA("Widescreen","MSAA",0,settingsPath);
 textureFiltering=GetPrivateProfileIntA("Widescreen","TextureFiltering",0,settingsPath);
 if(textureFiltering<0||textureFiltering>2)textureFiltering=0;
 anisotropyRequested=GetPrivateProfileIntA("Widescreen","Anisotropy",0,settingsPath);
 if(anisotropyRequested!=2&&anisotropyRequested!=4&&anisotropyRequested!=8&&anisotropyRequested!=16)anisotropyRequested=0;
 if(aaRequested!=2&&aaRequested!=4&&aaRequested!=8)aaRequested=0;
#ifdef PIRATES_LAYOUT_ONLY
 // Town release does not install the unfinished sailing redraw hooks.
 sailingSeparate=false;
#endif
 GetModuleFileNameA(self,traceFolder,MAX_PATH);*(strrchr(traceFolder,'\\')+1)=0;
 char mapTrialPath[MAX_PATH];std::snprintf(mapTrialPath,sizeof(mapTrialPath),"%smap-expansion.enabled",traceFolder);
 mapExpansionEnabled=mapExpansionEnabled||GetFileAttributesA(mapTrialPath)!=INVALID_FILE_ATTRIBUTES;
 char path[MAX_PATH];GetModuleFileNameA(self,path,MAX_PATH);strcpy(strrchr(path,'\\')+1,"PiratesWide.log");journal=std::fopen(path,"w");if(!journal)return 1;
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
 const unsigned char camera[]={0x8b,0x02,0x89,0x81,0x28,0x01,0,0};
 const unsigned char input[]={0x55,0x56,0x8b,0xf0,0xa0,0x3c,0x6b,0x72,0x00};
 if(reinterpret_cast<uintptr_t>(base)!=0x400000){std::fprintf(journal,"Unexpected image base\n");std::fflush(journal);return 2;}
 for(unsigned attempts=0;attempts<600;++attempts){auto r=*reinterpret_cast<unsigned char**>(base+0x4ea08c);if(!std::memcmp(base+0xcce74,camera,sizeof(camera))&&readable(r,0x64)&&readable(*reinterpret_cast<void**>(r+0x60),4))break;Sleep(100);}
 if(std::memcmp(base+0xcce74,camera,sizeof(camera))||std::memcmp(base+0x2c170,input,sizeof(input))){std::fprintf(journal,"Target signature mismatch; no changes.\n");std::fflush(journal);return 2;}
 // NiDX9Renderer::device is confirmed by the game's SetViewport call at 5c6b92.
 auto renderer=*reinterpret_cast<unsigned char**>(base+0x4ea08c);
 if(!readable(renderer,0x64)){std::fprintf(journal,"Renderer unavailable\n");std::fflush(journal);return 3;}
 targetDevice=*reinterpret_cast<IDirect3DDevice9**>(renderer+0x60);
 if(!readable(targetDevice,4)){std::fprintf(journal,"Device unavailable\n");std::fflush(journal);return 4;}
 D3DDEVICE_CREATION_PARAMETERS creation{};if(SUCCEEDED(targetDevice->GetCreationParameters(&creation)))gameWindow=creation.hFocusWindow;
 auto table=*reinterpret_cast<void***>(targetDevice);if(!readable(table,95*4))return 5;
 IDirect3DSurface9*bb=nullptr;if(SUCCEEDED(targetDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb))){D3DSURFACE_DESC desc{};bb->GetDesc(&desc);width=desc.Width;height=desc.Height;bb->Release();}
 auto result=MH_Initialize();bool ok=result==MH_OK;
 if(ok)ok=MH_CreateHook(table[44],reinterpret_cast<void*>(onTransform),reinterpret_cast<void**>(&originalTransform))==MH_OK;
 if(ok)ok=MH_CreateHook(table[17],reinterpret_cast<void*>(onPresent),reinterpret_cast<void**>(&originalPresent))==MH_OK;
 if(ok)ok=MH_CreateHook(table[16],reinterpret_cast<void*>(onReset),reinterpret_cast<void**>(&originalReset))==MH_OK;
 if(ok)ok=MH_CreateHook(table[82],reinterpret_cast<void*>(onDrawIndexed),reinterpret_cast<void**>(&originalDrawIndexed))==MH_OK;
 if(ok)ok=MH_CreateHook(table[83],reinterpret_cast<void*>(onDrawUP),reinterpret_cast<void**>(&originalDrawUP))==MH_OK;
 if(ok)ok=MH_CreateHook(table[81],reinterpret_cast<void*>(onDraw),reinterpret_cast<void**>(&originalDraw))==MH_OK;
 if(ok)ok=MH_CreateHook(table[84],reinterpret_cast<void*>(onDrawIndexedUP),reinterpret_cast<void**>(&originalDrawIndexedUP))==MH_OK;
 if(ok&&aaRequested){
  ok=MH_CreateHook(table[37],reinterpret_cast<void*>(onAaSetTarget),reinterpret_cast<void**>(&aaSetTarget))==MH_OK;
  if(ok)ok=MH_CreateHook(table[38],reinterpret_cast<void*>(onAaGetTarget),reinterpret_cast<void**>(&aaGetTarget))==MH_OK;
  if(ok)ok=MH_CreateHook(table[39],reinterpret_cast<void*>(onAaSetDepth),reinterpret_cast<void**>(&aaSetDepth))==MH_OK;
  if(ok)ok=MH_CreateHook(table[40],reinterpret_cast<void*>(onAaGetDepth),reinterpret_cast<void**>(&aaGetDepth))==MH_OK;
  if(ok)ok=MH_CreateHook(table[41],reinterpret_cast<void*>(onAaBegin),reinterpret_cast<void**>(&aaBegin))==MH_OK;
  if(ok)ok=MH_CreateHook(table[43],reinterpret_cast<void*>(onAaClear),reinterpret_cast<void**>(&aaClear))==MH_OK;
  if(ok)ok=MH_CreateHook(table[57],reinterpret_cast<void*>(onAaSetState),reinterpret_cast<void**>(&aaSetState))==MH_OK;
 }
 if(ok)ok=MH_CreateHook(base+0x2c170,reinterpret_cast<void*>(uiInputHook),&inputTrampoline)==MH_OK;
 if(ok)ok=MH_CreateHook(base+0xcce74,reinterpret_cast<void*>(frustumHook),&frustumTrampoline)==MH_OK;
 if(ok&&mapExpansionEnabled)ok=installMapExpansion(base);
#ifdef PIRATES_SAILING_OBSERVATION
 if(ok)ok=obsInstall(table);
#endif
 if(ok)ok=MH_EnableHook(MH_ALL_HOOKS)==MH_OK;
 if(!ok)MH_Uninitialize();
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(ok)initTimingDiagnostic();
#else
 if(ok&&sailingSeparate)initSailing();
#endif
 char profilePath[MAX_PATH];std::snprintf(profilePath,sizeof(profilePath),"%sprofile.enabled",traceFolder);
 if(ok&&GetFileAttributesA(profilePath)!=INVALID_FILE_ATTRIBUTES)std::fprintf(journal,"Sleep measurements=%s\n",installSleepMeasurements()?"available":"unavailable");
 std::fprintf(journal,"Pirates! Unbound 0.1.0-beta.2 init=%s size=%dx%d device=%p world=%d centerUI=%d backgrounds=%d\n",ok?"OK":"FAILED",width.load(),height.load(),targetDevice,widenWorld,centerUi,fillBackgrounds);std::fflush(journal);return ok?0:6;
}
BOOL WINAPI DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){self=h;DisableThreadLibraryCalls(h);HANDLE t=CreateThread(nullptr,0,init,nullptr,0,nullptr);if(t)CloseHandle(t);}return TRUE;}
