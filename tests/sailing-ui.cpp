#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <utility>
#include <cassert>
#include "MinHook.h"
static IDirect3DDevice9* targetDevice=nullptr;
static bool interfaceView=true,sailingFrameActive=true,sailingExtra=false,sailingHealthy=true;
static bool sailingLabelPass=false,sailingEffectPass=false;
static FILE* journal=stdout;
using DrawIndexedUP=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
static DrawIndexedUP originalDrawIndexedUP;
using Transform=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
static Transform originalTransform;
#include "../src/sailing-ui.h"
static std::vector<unsigned char> pixels(IDirect3DDevice9* device){
 IDirect3DSurface9 *gpu=nullptr,*cpu=nullptr;assert(SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&gpu)));
 D3DSURFACE_DESC desc{};gpu->GetDesc(&desc);assert(SUCCEEDED(device->CreateOffscreenPlainSurface(64,64,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr)));
 assert(SUCCEEDED(device->GetRenderTargetData(gpu,cpu)));D3DLOCKED_RECT lock{};assert(SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)));
 std::vector<unsigned char> result(64*64*4);for(unsigned y=0;y<64;++y)std::memcpy(result.data()+y*64*4,static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,64*4);
 cpu->UnlockRect();cpu->Release();gpu->Release();return result;
}
int main(){
 assert(!sailingUiReady());
 const HWND window=CreateWindowA("STATIC","PiratesWide rendering test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);assert(window);
 IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);assert(api);
 D3DPRESENT_PARAMETERS parameters{};parameters.Windowed=TRUE;parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;parameters.BackBufferWidth=64;parameters.BackBufferHeight=64;parameters.BackBufferFormat=D3DFMT_A8R8G8B8;parameters.hDeviceWindow=window;
 assert(SUCCEEDED(api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&parameters,&targetDevice)));
 originalDrawIndexedUP=reinterpret_cast<DrawIndexedUP>((*reinterpret_cast<void***>(targetDevice))[84]);
 originalTransform=reinterpret_cast<Transform>((*reinterpret_cast<void***>(targetDevice))[44]);
 struct Vertex{float x,y,z,w;DWORD color;};
 Vertex vertices[7]{};vertices[3]={8,8,0,1,0xff3399cc};vertices[4]={56,8,0,1,0xff3399cc};vertices[5]={56,56,0,1,0xff3399cc};vertices[6]={8,56,0,1,0xff3399cc};
 unsigned short indices[]={0,0,0,2,3,4,2,4,5};
 IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;void* mapped=nullptr;
 assert(SUCCEEDED(targetDevice->CreateVertexBuffer(sizeof(vertices),0,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,D3DPOOL_MANAGED,&vb,nullptr)));
 assert(SUCCEEDED(targetDevice->CreateIndexBuffer(sizeof(indices),0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr)));
 vb->Lock(0,0,&mapped,0);std::memcpy(mapped,vertices,sizeof(vertices));vb->Unlock();ib->Lock(0,0,&mapped,0);std::memcpy(mapped,indices,sizeof(indices));ib->Unlock();
 targetDevice->SetStreamSource(0,vb,0,sizeof(Vertex));targetDevice->SetIndices(ib);targetDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);targetDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);targetDevice->SetRenderState(D3DRS_LIGHTING,FALSE);targetDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLELIST,1,2,4,3,2));assert(sailingUiDraws.size()==1&&sailingHealthy);
 assert(sailingUiReady());sailingUiPending=true;assert(!sailingUiReady());sailingUiPending=false;
 assert(SUCCEEDED(targetDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,1,2,4,3,2)));targetDevice->EndScene();const auto native=pixels(targetDevice);
 // Destroy the source geometry's contents to prove replay owns its data.
 vb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(vertices));vb->Unlock();ib->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(indices));ib->Unlock();
 sailingFrameActive=false;sailingExtra=true;
 for(unsigned repeat=0;repeat<3;++repeat){
  targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
  assert(sailingUiIndexed(targetDevice,D3DPT_TRIANGLELIST,1,2,4,3,2));assert(sailingUiReplay(targetDevice));const auto replay=pixels(targetDevice);
  if(native!=replay){unsigned differing=0;for(unsigned i=0;i<native.size();++i)if(native[i]!=replay[i])++differing;std::printf("Different bytes=%u center native=%02x%02x%02x%02x replay=%02x%02x%02x%02x\n",differing,native[8320],native[8321],native[8322],native[8323],replay[8320],replay[8321],replay[8322],replay[8323]);}
  std::fflush(stdout);assert(native==replay);
  IDirect3DVertexBuffer9* restored=nullptr;UINT offset=0,stride=0;targetDevice->GetStreamSource(0,&restored,&offset,&stride);assert(restored==vb&&stride==sizeof(Vertex));restored->Release();
 }
 assert(native[(32*64+32)*4]==0xcc);assert(sailingUiReplays==3);
 sailingUiClear();assert(sailingUiDraws.empty());
 assert(!sailingUiReady());
 // The game's screen quads also use indexed triangle strips.
 sailingFrameActive=true;sailingExtra=false;
 vb->Lock(0,0,&mapped,0);std::memcpy(mapped,vertices,sizeof(vertices));vb->Unlock();
 const unsigned short strip[]={0,0,0,2,3,5,4};
 ib->Lock(0,0,&mapped,0);std::memcpy(mapped,strip,sizeof(strip));ib->Unlock();
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLESTRIP,1,2,4,3,2));assert(sailingHealthy&&sailingUiDraws.size()==1);
 assert(SUCCEEDED(targetDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,1,2,4,3,2)));targetDevice->EndScene();
 const auto nativeStrip=pixels(targetDevice);
 vb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(vertices));vb->Unlock();
 ib->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(indices));ib->Unlock();
 sailingFrameActive=false;sailingExtra=true;targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
 assert(sailingUiReplay(targetDevice));assert(nativeStrip==pixels(targetDevice));assert(nativeStrip==native);
 sailingUiClear();
 // World labels use their own camera. Capture only inside the verified label
 // routine, rather than treating all world geometry as interface geometry.
 sailingFrameActive=true;sailingExtra=false;interfaceView=false;
 vb->Lock(0,0,&mapped,0);std::memcpy(mapped,vertices,sizeof(vertices));vb->Unlock();
 ib->Lock(0,0,&mapped,0);std::memcpy(mapped,strip,sizeof(strip));ib->Unlock();
 assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLESTRIP,1,2,4,3,2));assert(sailingUiDraws.empty());
 sailingLabelPass=true;
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLESTRIP,1,2,4,3,2));assert(sailingUiDraws.size()==1);
 assert(!sailingUiReady()); // World-label geometry alone cannot replace a HUD.
 assert(SUCCEEDED(targetDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,1,2,4,3,2)));targetDevice->EndScene();
 const auto nativeLabel=pixels(targetDevice);
 sailingLabelPass=false;sailingFrameActive=false;sailingExtra=true;
 vb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(vertices));vb->Unlock();
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
 assert(sailingUiReplay(targetDevice,true));assert(nativeLabel==pixels(targetDevice));
 sailingUiClear();
 // Transient world effects belong at their native pass, not in the HUD replay.
 sailingFrameActive=true;sailingExtra=false;sailingEffectPass=true;
 vb->Lock(0,0,&mapped,0);std::memcpy(mapped,vertices,sizeof(vertices));vb->Unlock();
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLESTRIP,1,2,4,3,2));
 assert(sailingUiDraws.size()==1&&sailingUiDraws[0].effect&&!sailingUiReady());
 targetDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,1,2,4,3,2);targetDevice->EndScene();
 const auto nativeEffect=pixels(targetDevice);
 sailingEffectPass=false;sailingExtra=true;sailingFrameActive=false;
 vb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(vertices));vb->Unlock();
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
 const auto effectBlank=pixels(targetDevice);assert(sailingUiReplay(targetDevice));assert(effectBlank==pixels(targetDevice));
 targetDevice->BeginScene();assert(sailingUiReplay(targetDevice,false,true,true));targetDevice->EndScene();
 assert(nativeEffect==pixels(targetDevice));sailingUiClear();
 // Native transient text uses DrawPrimitive, not indexed HUD geometry.
 assert(MH_Initialize()==MH_OK);
 assert(sailingObserveBuffer(vb,sizeof(vertices)));
 sailingFrameActive=true;sailingExtra=false;sailingLabelPass=true;
 vb->Lock(0,0,&mapped,0);std::memcpy(mapped,vertices,sizeof(vertices));vb->Unlock();
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(!sailingUiPrimitive(targetDevice,D3DPT_TRIANGLESTRIP,3,2));assert(sailingUiDraws.size()==1&&sailingHealthy);
 assert(SUCCEEDED(targetDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,3,2)));targetDevice->EndScene();
 const auto primitiveLabel=pixels(targetDevice);
 sailingLabelPass=false;sailingFrameActive=false;sailingExtra=true;
 vb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(vertices));vb->Unlock();
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
 assert(sailingUiReplay(targetDevice,true));assert(primitiveLabel==pixels(targetDevice));
 sailingUiClear();
 // WRITEONLY dynamic buffers must be copied while the engine writes them.
 // Reading them later is unsupported and can return invalid vertex contents.
 IDirect3DVertexBuffer9* dynamic=nullptr;
 assert(SUCCEEDED(targetDevice->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,D3DPOOL_DEFAULT,&dynamic,nullptr)));
 targetDevice->SetStreamSource(0,dynamic,0,sizeof(Vertex));
 sailingFrameActive=true;sailingExtra=false;sailingLabelPass=true;
 assert(!sailingUiPrimitive(targetDevice,D3DPT_TRIANGLESTRIP,3,2));assert(sailingUiPending&&sailingUiDraws.empty()&&sailingHealthy);
 sailingUiClear();
 assert(SUCCEEDED(dynamic->Lock(0,0,&mapped,D3DLOCK_DISCARD)));std::memcpy(mapped,vertices,sizeof(vertices));assert(SUCCEEDED(dynamic->Unlock()));
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(!sailingUiPrimitive(targetDevice,D3DPT_TRIANGLESTRIP,3,2));assert(!sailingUiPending&&sailingUiDraws.size()==1);
 assert(SUCCEEDED(targetDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,3,2)));targetDevice->EndScene();
 const auto dynamicNative=pixels(targetDevice);assert(dynamicNative==primitiveLabel);
 assert(SUCCEEDED(dynamic->Lock(0,0,&mapped,D3DLOCK_DISCARD)));std::memset(mapped,0,sizeof(vertices));assert(SUCCEEDED(dynamic->Unlock()));
 sailingLabelPass=false;sailingFrameActive=false;sailingExtra=true;
 targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);targetDevice->BeginScene();
 assert(sailingUiReplay(targetDevice,true,true));targetDevice->EndScene();assert(dynamicNative==pixels(targetDevice));
 sailingUiClear();sailingDropBuffers();dynamic->Release();MH_Uninitialize();interfaceView=true;
 // Live sailing HUD uses XYZ + diffuse + texture coordinates, not XYZRHW.
 // Exercise its transformed camera, alpha test, depth and centered scissor.
 {
  struct HudVertex{float x,y,z;DWORD color;float u,v;};
  const HudVertex quad[]={{-.7f,.7f,.5f,0xffffffff,0,0},{.7f,.7f,.5f,0xffffffff,1,0},{.7f,-.7f,.5f,0xffffffff,1,1},{-.7f,-.7f,.5f,0xffffffff,0,1}};
  const unsigned short order[]={0,1,2,0,2,3};
  IDirect3DVertexBuffer9* hudVb=nullptr;IDirect3DIndexBuffer9* hudIb=nullptr;IDirect3DTexture9* texture=nullptr;IDirect3DSurface9* hudDepth=nullptr;
  assert(SUCCEEDED(targetDevice->CreateVertexBuffer(sizeof(quad),0,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1,D3DPOOL_MANAGED,&hudVb,nullptr)));
  assert(SUCCEEDED(targetDevice->CreateIndexBuffer(sizeof(order),0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&hudIb,nullptr)));
  assert(SUCCEEDED(targetDevice->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr)));
  assert(SUCCEEDED(targetDevice->CreateDepthStencilSurface(64,64,D3DFMT_D16,D3DMULTISAMPLE_NONE,0,TRUE,&hudDepth,nullptr)));
  hudVb->Lock(0,0,&mapped,0);std::memcpy(mapped,quad,sizeof(quad));hudVb->Unlock();
  hudIb->Lock(0,0,&mapped,0);std::memcpy(mapped,order,sizeof(order));hudIb->Unlock();
  D3DLOCKED_RECT texLock{};texture->LockRect(0,&texLock,nullptr,0);
  for(unsigned y=0;y<2;++y){auto row=reinterpret_cast<DWORD*>(static_cast<unsigned char*>(texLock.pBits)+y*texLock.Pitch);row[0]=0xff3366cc;row[1]=y?0xffcc6633:0x003366cc;}texture->UnlockRect(0);
  D3DMATRIX identity{};for(unsigned i=0;i<4;++i)identity.m[i][i]=1;
  D3DMATRIX world=identity;world._41=.1f;world._42=-.1f;
  targetDevice->SetTransform(D3DTS_WORLD,&world);targetDevice->SetTransform(D3DTS_VIEW,&identity);targetDevice->SetTransform(D3DTS_PROJECTION,&identity);
  targetDevice->SetDepthStencilSurface(hudDepth);targetDevice->SetRenderState(D3DRS_ZENABLE,TRUE);targetDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);targetDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
  targetDevice->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);targetDevice->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);targetDevice->SetRenderState(D3DRS_ALPHAREF,128);
  RECT clip{14,10,54,55};targetDevice->SetScissorRect(&clip);targetDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);
  targetDevice->SetTexture(0,texture);targetDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);targetDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);targetDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
  targetDevice->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);targetDevice->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
  targetDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);targetDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
  targetDevice->SetStreamSource(0,hudVb,0,sizeof(HudVertex));targetDevice->SetIndices(hudIb);targetDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
  sailingFrameActive=true;sailingExtra=false;
  targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0);targetDevice->BeginScene();
  assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLELIST,0,0,4,0,2));assert(sailingUiDraws.size()==1);
  assert(SUCCEEDED(targetDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2)));targetDevice->EndScene();const auto nativeHud=pixels(targetDevice);
  assert(nativeHud[(32*64+20)*4]==0xcc);assert(nativeHud[(20*64+48)*4]==0);
  hudVb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(quad));hudVb->Unlock();hudIb->Lock(0,0,&mapped,0);std::memset(mapped,0,sizeof(order));hudIb->Unlock();
  D3DMATRIX displaced=identity;displaced._41=3;targetDevice->SetTransform(D3DTS_WORLD,&displaced);
  RECT changed{0,0,1,1};targetDevice->SetScissorRect(&changed);targetDevice->SetRenderState(D3DRS_ZENABLE,FALSE);targetDevice->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);targetDevice->SetTexture(0,nullptr);
  sailingFrameActive=false;sailingExtra=true;targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0);
  assert(sailingUiReplay(targetDevice));assert(nativeHud==pixels(targetDevice));
  D3DMATRIX restoredWorld{};DWORD restoredZ=1;RECT restoredClip{};targetDevice->GetTransform(D3DTS_WORLD,&restoredWorld);targetDevice->GetRenderState(D3DRS_ZENABLE,&restoredZ);targetDevice->GetScissorRect(&restoredClip);
  assert(!std::memcmp(&restoredWorld,&displaced,sizeof(displaced))&&restoredZ==FALSE&&!std::memcmp(&restoredClip,&changed,sizeof(changed)));
  sailingUiClear();targetDevice->SetDepthStencilSurface(nullptr);targetDevice->SetIndices(ib);
  hudVb->Release();hudIb->Release();texture->Release();hudDepth->Release();
 }
 targetDevice->SetStreamSource(0,vb,0,sizeof(Vertex));
 sailingFrameActive=true;sailingExtra=false;
 assert(!sailingUiIndexed(targetDevice,D3DPT_TRIANGLELIST,1,2,4,3,2));assert(!sailingHealthy&&sailingUiFailed);
 sailingUiClear();vb->Release();ib->Release();targetDevice->Release();api->Release();DestroyWindow(window);
 std::puts("PASS: exact D3D overlay pixels, owned buffers, index rebasing, repeated redraws, state restoration and invalid-geometry fallback.");
}


