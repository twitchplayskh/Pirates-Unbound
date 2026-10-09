#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cassert>
static IDirect3DDevice9*targetDevice=nullptr;
static bool interfaceView=false;
static FILE*journal=stdout;
#include "../src/texture-filtering.h"
static DWORD sampler(unsigned stage,D3DSAMPLERSTATETYPE type){DWORD value=0;assert(SUCCEEDED(targetDevice->GetSamplerState(stage,type,&value)));return value;}
int main(){
 auto window=CreateWindowA("STATIC","filtering fixture",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
 auto api=Direct3DCreate9(D3D_SDK_VERSION);assert(api&&window);
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferWidth=pp.BackBufferHeight=64;pp.hDeviceWindow=window;
 assert(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&targetDevice)));
 IDirect3DTexture9 *mips=nullptr,*flat=nullptr,*rt=nullptr;
 assert(SUCCEEDED(targetDevice->CreateTexture(32,32,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&mips,nullptr)));
 assert(SUCCEEDED(targetDevice->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&flat,nullptr)));
 assert(SUCCEEDED(targetDevice->CreateTexture(32,32,0,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&rt,nullptr)));
 for(unsigned s:{0u,3u}){targetDevice->SetTexture(s,mips);targetDevice->SetSamplerState(s,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);targetDevice->SetSamplerState(s,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);targetDevice->SetSamplerState(s,D3DSAMP_MIPFILTER,D3DTEXF_POINT);targetDevice->SetSamplerState(s,D3DSAMP_MAXANISOTROPY,1);targetDevice->SetSamplerState(s,D3DSAMP_ADDRESSU,D3DTADDRESS_MIRROR);}
 D3DCAPS9 caps{};assert(SUCCEEDED(targetDevice->GetDeviceCaps(&caps)));
 for(int mode:{0,1,2})for(int af:{0,2,4,8,16}){
  textureFiltering=mode;anisotropyRequested=af;
  {TextureFilteringState scope(targetDevice);
   for(unsigned s:{0u,3u}){
    DWORD effective=af>1&&(caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC)?std::min(DWORD(af),caps.MaxAnisotropy):1;
    assert(sampler(s,D3DSAMP_MINFILTER)==(effective>1?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR));
    assert(sampler(s,D3DSAMP_MAXANISOTROPY)==effective);
    assert(sampler(s,D3DSAMP_MIPFILTER)==(mode==2&&(caps.TextureFilterCaps&D3DPTFILTERCAPS_MIPFLINEAR)?D3DTEXF_LINEAR:D3DTEXF_POINT));
    assert(sampler(s,D3DSAMP_ADDRESSU)==D3DTADDRESS_MIRROR);
   }
  }
  for(unsigned s:{0u,3u}){assert(sampler(s,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR);assert(sampler(s,D3DSAMP_MIPFILTER)==D3DTEXF_POINT);assert(sampler(s,D3DSAMP_MAXANISOTROPY)==1);}
 }
 textureFiltering=2;anisotropyRequested=16;
 interfaceView=true;{TextureFilteringState scope(targetDevice);assert(sampler(0,D3DSAMP_MIPFILTER)==D3DTEXF_POINT);}interfaceView=false;
 for(auto*t:{flat,rt}){targetDevice->SetTexture(0,t);TextureFilteringState scope(targetDevice);assert(sampler(0,D3DSAMP_MIPFILTER)==D3DTEXF_POINT);assert(sampler(0,D3DSAMP_MAXANISOTROPY)==1);}
 targetDevice->SetTexture(0,mips);targetDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
 {TextureFilteringState scope(targetDevice);assert(sampler(0,D3DSAMP_MINFILTER)==D3DTEXF_POINT);assert(sampler(0,D3DSAMP_MAXANISOTROPY)==1);}
 targetDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);targetDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
 {TextureFilteringState scope(targetDevice);assert(sampler(0,D3DSAMP_MIPFILTER)==D3DTEXF_NONE);assert(sampler(0,D3DSAMP_MAXANISOTROPY)==1);}
 targetDevice->SetTexture(0,nullptr);targetDevice->SetTexture(3,nullptr);mips->Release();flat->Release();rt->Release();targetDevice->Release();api->Release();DestroyWindow(window);
 std::printf("PASS: hardware filtering, levels 0/2/4/8/16, capability clamp, stages 0/3, full restoration, native/UI/point/no-mip/render-target exclusions. GPU maximum=%lu\n",static_cast<unsigned long>(caps.MaxAnisotropy));
}
