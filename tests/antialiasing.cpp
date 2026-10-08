#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cassert>
#include "MinHook.h"
static IDirect3DDevice9* targetDevice=nullptr;
static FILE* journal=stdout;
#include "../src/antialiasing.h"
static std::vector<unsigned char> pixels(IDirect3DSurface9* gpu){
 D3DSURFACE_DESC desc{};assert(SUCCEEDED(gpu->GetDesc(&desc)));IDirect3DSurface9* cpu=nullptr;
 assert(SUCCEEDED(targetDevice->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr)));
 assert(SUCCEEDED(targetDevice->GetRenderTargetData(gpu,cpu)));D3DLOCKED_RECT lock{};assert(SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)));
 std::vector<unsigned char> result(desc.Width*desc.Height*4);
 for(unsigned y=0;y<desc.Height;++y)std::memcpy(result.data()+y*desc.Width*4,static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,desc.Width*4);
 cpu->UnlockRect();cpu->Release();return result;
}
struct Vertex{float x,y,z,w;DWORD color;};
static std::vector<unsigned char> draw(){
 assert(SUCCEEDED(targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0)));
 assert(SUCCEEDED(targetDevice->BeginScene()));
 Vertex tri[]={{4.2f,3.7f,0,1,0xffffffff},{59.3f,14.1f,0,1,0xffffffff},{19.7f,60.4f,0,1,0xffffffff}};
 assert(SUCCEEDED(targetDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,tri,sizeof(Vertex))));
 // A pixel-aligned solid rectangle stands in for crisp UI interiors.
 Vertex rect[]={{40,40,0,1,0xff00ff00},{55,40,0,1,0xff00ff00},{40,55,0,1,0xff00ff00},{55,55,0,1,0xff00ff00}};
 assert(SUCCEEDED(targetDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,rect,sizeof(Vertex))));
 assert(SUCCEEDED(targetDevice->EndScene()));assert(aaMain.resolve(targetDevice));
 IDirect3DSurface9* back=nullptr;assert(SUCCEEDED(targetDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)));
 auto result=pixels(back);back->Release();return result;
}
int main(){
 HWND window=CreateWindowA("STATIC","PiratesWide MSAA test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);assert(window);
 IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);assert(api);
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.hDeviceWindow=window;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
 assert(SUCCEEDED(api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&targetDevice)));
 auto table=*reinterpret_cast<void***>(targetDevice);assert(MH_Initialize()==MH_OK);
 assert(MH_CreateHook(table[37],reinterpret_cast<void*>(onAaSetTarget),reinterpret_cast<void**>(&aaSetTarget))==MH_OK);
 assert(MH_CreateHook(table[38],reinterpret_cast<void*>(onAaGetTarget),reinterpret_cast<void**>(&aaGetTarget))==MH_OK);
 assert(MH_CreateHook(table[39],reinterpret_cast<void*>(onAaSetDepth),reinterpret_cast<void**>(&aaSetDepth))==MH_OK);
 assert(MH_CreateHook(table[40],reinterpret_cast<void*>(onAaGetDepth),reinterpret_cast<void**>(&aaGetDepth))==MH_OK);
 assert(MH_CreateHook(table[41],reinterpret_cast<void*>(onAaBegin),reinterpret_cast<void**>(&aaBegin))==MH_OK);
 assert(MH_CreateHook(table[43],reinterpret_cast<void*>(onAaClear),reinterpret_cast<void**>(&aaClear))==MH_OK);
 assert(MH_CreateHook(table[57],reinterpret_cast<void*>(onAaSetState),reinterpret_cast<void**>(&aaSetState))==MH_OK);
 assert(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
 targetDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);targetDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);targetDevice->SetRenderState(D3DRS_LIGHTING,FALSE);targetDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
 const auto native=draw();assert(!aaMain.color);aaRequested=4;const auto smooth=draw();assert(aaMain.samples==4||aaMain.samples==2);
 unsigned nativePartial=0,smoothPartial=0;
 for(unsigned i=0;i<native.size();i+=4){nativePartial+=native[i]>0&&native[i]<255;smoothPartial+=smooth[i]>0&&smooth[i]<255;}
 assert(nativePartial==0&&smoothPartial>50);assert(native!=smooth);
 for(unsigned y=42;y<53;++y)for(unsigned x=42;x<53;++x)assert(!std::memcmp(&native[(y*64+x)*4],&smooth[(y*64+x)*4],3));
 IDirect3DSurface9 *logical=nullptr,*logicalDepth=nullptr;
 assert(SUCCEEDED(targetDevice->GetRenderTarget(0,&logical))&&logical==aaMain.back);
 assert(SUCCEEDED(targetDevice->GetDepthStencilSurface(&logicalDepth))&&logicalDepth==aaMain.nativeDepth);logicalDepth->Release();
 IDirect3DSurface9* offscreen=nullptr;assert(SUCCEEDED(targetDevice->CreateRenderTarget(32,32,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&offscreen,nullptr)));
 assert(SUCCEEDED(targetDevice->SetRenderTarget(0,offscreen)));
 assert(!aaMain.selected(targetDevice));assert(SUCCEEDED(targetDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0)));
 auto other=pixels(offscreen);assert(other[0]==0x56&&other[1]==0x34&&other[2]==0x12);
 assert(SUCCEEDED(targetDevice->SetRenderTarget(0,logical))&&aaMain.selected(targetDevice));logical->Release();offscreen->Release();
 // Resource release before reset must remove our default-pool references.
 aaMain.release(targetDevice);assert(SUCCEEDED(targetDevice->Reset(&pp)));
 targetDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);targetDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);targetDevice->SetRenderState(D3DRS_LIGHTING,FALSE);targetDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
 assert(draw()==smooth);aaMain.release(targetDevice);
 assert(MH_Uninitialize()==MH_OK);targetDevice->Release();api->Release();DestroyWindow(window);
 std::printf("PASS: hardware MSAA edge coverage (%u partial pixels), crisp solid UI interiors, logical surfaces, offscreen targets and reset/recreation.\n",smoothPartial);
}
