#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
int main(int argc,char**argv){
 if(argc!=3)return 2;
 const unsigned w=std::strtoul(argv[1],nullptr,10),h=std::strtoul(argv[2],nullptr,10);
 auto api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 3;
 bool found=false;
 for(auto format:{D3DFMT_X8R8G8B8,D3DFMT_R5G6B5}){
  const unsigned count=api->GetAdapterModeCount(D3DADAPTER_DEFAULT,format);
  for(unsigned i=0;i<count;++i){D3DDISPLAYMODE mode{};
   if(SUCCEEDED(api->EnumAdapterModes(D3DADAPTER_DEFAULT,format,i,&mode))&&mode.Width==w&&mode.Height==h)found=true;
  }
 }
 api->Release();std::printf("Fullscreen mode %ux%u: %s\n",w,h,found?"available":"unavailable");return found?0:1;
}
