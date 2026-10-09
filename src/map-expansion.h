// Experimental, opt-in map expansion. Read additional stock cartography; never
// scale the native crop or alter the game's marker/picking coordinates.
using MapGenerate=void(__cdecl*)(int,int,int,int,int,int,int,int);
static MapGenerate originalMapGenerate;
static bool mapExpansionEnabled=false;
struct MapCrop {int zoom=0,x=0,y=0;unsigned frame=~0u;};
static MapCrop mapCrop;
static unsigned mapExpandedFrame=~0u;
static unsigned mapEpoch=0,rejectedMapEpoch=~0u;
static IDirect3DTexture9* rejectedMap=nullptr;
static void __cdecl observeMapGenerate(int zoom,int x,int y,int dx,int dy,int w,int h,int depth){
 ++mapEpoch;
 mapCrop={zoom,zoom*x,zoom*y,frames.load()};
 static MapCrop last;
 if(journal&&(last.zoom!=zoom||last.x!=mapCrop.x||last.y!=mapCrop.y)){
  std::fprintf(journal,"MAP crop zoom=%d source=%d,%d destination=%d,%d %dx%d depth=%d frame=%u\n",zoom,mapCrop.x,mapCrop.y,dx,dy,w,h,depth,frames.load());std::fflush(journal);last=mapCrop;
 }
 originalMapGenerate(zoom,x,y,dx,dy,w,h,depth);
}
struct MapBitmap {
 std::vector<unsigned char> bytes;int zoom=0,w=0,h=0,failedLevel=0;unsigned offset=0;DWORD palette[256]{};
 bool decode(int level){
  if(bytes.size()<1078)return false;
  auto u16=[&](unsigned p){WORD v;std::memcpy(&v,bytes.data()+p,2);return v;};
  auto u32=[&](unsigned p){DWORD v;std::memcpy(&v,bytes.data()+p,4);return v;};
  offset=u32(10);w=int(u32(18));h=int(u32(22));
  if((level!=2&&level!=4&&level!=6)||u16(0)!=0x4d42||u32(14)!=40||u16(26)!=1||u16(28)!=8||u32(30)!=0||w<40||h<40||w%4||offset<1078||static_cast<unsigned long long>(offset)+static_cast<unsigned long long>(w)*h>bytes.size()){bytes.clear();return false;}
  for(unsigned i=0;i<256;++i)palette[i]=0xff000000u|(u32(54+4*i)&0xffffffu);
  zoom=level;failedLevel=0;return true;
 }
 bool load(int level){
  if(zoom==level&&!bytes.empty())return true;
  if(failedLevel==level)return false;
  failedLevel=level;
  bytes.clear();zoom=0;
  if(level!=2&&level!=4&&level!=6)return false;
  char game[MAX_PATH];GetModuleFileNameA(nullptr,game,MAX_PATH);auto slash=std::strrchr(game,'\\');if(!slash)return false;slash[1]=0;
  char desired[40];std::snprintf(desired,sizeof(desired),"caribbeanmap%d.bmp",level);
  for(unsigned pak=0;pak<5;++pak){
   char path[MAX_PATH];std::snprintf(path,sizeof(path),"%sAssets\\Pak%u.FPK",game,pak);FILE*f=std::fopen(path,"rb");if(!f)continue;
   DWORD version=0,count=0;bool found=false;
   if(std::fread(&version,4,1,f)==1&&version==2&&std::fread(&count,4,1,f)==1&&count<100000){
    for(unsigned i=0;i<count;++i){
     DWORD length=0,meta[4]{};if(std::fread(&length,4,1,f)!=1||!length||length>4096)break;
     std::vector<unsigned char> name((length+3)&~3u);if(std::fread(name.data(),1,name.size(),f)!=name.size()||std::fread(meta,4,4,f)!=4)break;
     for(auto&c:name)c-=1;
     name.resize(length+1);name[length]=0;
     if(_stricmp(reinterpret_cast<char*>(name.data()),desired))continue;
     if(meta[2]<1078||meta[2]>32000000||std::fseek(f,meta[3],SEEK_SET))break;
     bytes.resize(meta[2]);found=std::fread(bytes.data(),1,bytes.size(),f)==bytes.size();break;
    }
   }
   std::fclose(f);if(found)break;bytes.clear();
  }
  if(!decode(level))return false;
  if(journal)std::fprintf(journal,"MAP source zoom=%d %dx%d offset=%u\n",zoom,w,h,offset);
  return true;
 }
 DWORD pixel(int x,int y)const{
  // Native reads BMP row (height - cropY), then retreats one row per line.
  // Out-of-image samples are never wrapped into an adjacent scanline.
  if(x<0||x>=w||y<=0||y>h){
   // Paper beyond the finite chart: sample an unmarked corner of the stock
   // image, without repeating coastlines or wrapping adjacent BMP rows.
   const int px=8+(unsigned(x)&31u),py=8+(unsigned(y)&31u);
   return palette[bytes[offset+size_t(h-py)*w+px]];
  }
  return palette[bytes[offset+size_t(h-y)*w+x]];
 }
};
static MapBitmap mapBitmap;
static IDirect3DTexture9* expandedMapTexture=nullptr;
static IDirect3DTexture9* qualifiedNativeMap=nullptr;
static MapCrop expandedMapCrop;
static int expandedMapPixels=0;
static void clearMapExpansion(){if(expandedMapTexture)expandedMapTexture->Release();if(qualifiedNativeMap)qualifiedNativeMap->Release();if(rejectedMap)rejectedMap->Release();rejectedMap=nullptr;++mapEpoch;qualifiedNativeMap=nullptr;expandedMapTexture=nullptr;expandedMapPixels=0;mapExpandedFrame=~0u;mapCrop={};}
static bool drawExpandedMap(IDirect3DDevice9*d,INT base,UINT minimum){
 if(!mapExpansionEnabled||!mapCrop.zoom||height.load()<=0||!centerUi||!fillBackgrounds||3ll*width.load()<=4ll*height.load()||3ll*width.load()>8ll*height.load())return false;
 // NiScreenElements retains the quad between native UI updates. Qualify the
 // recorded crop against the generator's cache, not the D3D present number.
 if(*reinterpret_cast<const int*>(0x72554c)!=mapCrop.zoom||*reinterpret_cast<const int*>(0x725550)!=mapCrop.x||*reinterpret_cast<const int*>(0x725554)!=mapCrop.y)return false;
 if(!mapBitmap.load(mapCrop.zoom))return false;
 IDirect3DBaseTexture9*resource=nullptr;D3DSURFACE_DESC desc{};
 if(FAILED(d->GetTexture(0,&resource))||!resource)return false;
 auto native=resource->GetType()==D3DRTYPE_TEXTURE?static_cast<IDirect3DTexture9*>(resource):nullptr;
 if(native&&native==rejectedMap&&rejectedMapEpoch==mapEpoch){resource->Release();return false;}
 bool valid=native&&SUCCEEDED(native->GetLevelDesc(0,&desc))&&desc.Width==1024&&desc.Height==1024&&desc.Format==D3DFMT_X8R8G8B8&&!(desc.Usage&D3DUSAGE_RENDERTARGET);
 // Exact central-pixel oracle before every changed crop. Refuse substitutions
 // if a different asset, palette, source convention or scene is encountered.
 bool changed=qualifiedNativeMap!=native||expandedMapCrop.zoom!=mapCrop.zoom||expandedMapCrop.x!=mapCrop.x||expandedMapCrop.y!=mapCrop.y;
 if(valid&&(changed||!expandedMapTexture)){
  D3DLOCKED_RECT pixels{};valid=SUCCEEDED(native->LockRect(0,&pixels,nullptr,D3DLOCK_READONLY));unsigned compared=0;
  if(valid){
   for(int y=0;y<646;++y)for(int x=0;x<1024;++x){
    if(mapCrop.x+x<0||mapCrop.x+x>=mapBitmap.w||mapCrop.y+y<=0||mapCrop.y+y>mapBitmap.h)continue;
    DWORD actual;std::memcpy(&actual,static_cast<unsigned char*>(pixels.pBits)+y*pixels.Pitch+x*4,4);
    if((actual&0xffffff)!=(mapBitmap.pixel(mapCrop.x+x,mapCrop.y+y)&0xffffff))valid=false;
    ++compared;
   }
   native->UnlockRect(0);valid=valid&&compared>=100000;
  }
  static unsigned notices=0;
  if(journal&&notices<256){++notices;std::fprintf(journal,"MAP central oracle matches=%d samples=%u crop=%d,%d zoom=%d\n",valid,compared,mapCrop.x,mapCrop.y,mapCrop.zoom);std::fflush(journal);}
 }
 if(valid&&qualifiedNativeMap!=native){if(qualifiedNativeMap)qualifiedNativeMap->Release();qualifiedNativeMap=native;qualifiedNativeMap->AddRef();}
 if(!valid&&native){if(rejectedMap)rejectedMap->Release();rejectedMap=native;rejectedMap->AddRef();rejectedMapEpoch=mapEpoch;}
 resource->Release();if(!valid)return false;
 const int pixels=std::min(2048,int(std::ceil(1024.f*3.f*width.load()/(4.f*height.load()))));
 if(!expandedMapTexture||changed||expandedMapPixels!=pixels){
  if(expandedMapTexture){expandedMapTexture->Release();expandedMapTexture=nullptr;}
  if(FAILED(d->CreateTexture(2048,1024,1,0,D3DFMT_X8R8G8B8,D3DPOOL_MANAGED,&expandedMapTexture,nullptr)))return false;
  D3DLOCKED_RECT lock{};if(FAILED(expandedMapTexture->LockRect(0,&lock,nullptr,0))){clearMapExpansion();return false;}
  const int extra=(pixels-1024)/2;
  for(int y=0;y<1024;++y)for(int x=0;x<2048;++x)static_cast<DWORD*>(static_cast<void*>(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch))[x]=mapBitmap.pixel(mapCrop.x+x-extra,mapCrop.y+y);
  expandedMapTexture->UnlockRect(0);expandedMapCrop=mapCrop;expandedMapPixels=pixels;
 }
 struct Vertex {float x,y,z;DWORD color;float u,v;};Vertex vertices[4]{};
 IDirect3DVertexBuffer9*vb=nullptr;UINT offset=0,stride=0;void*data=nullptr;const int first=base+minimum;
 if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||!vb)return false;
 valid=first>=0&&stride==sizeof(Vertex)&&SUCCEEDED(vb->Lock(offset+first*stride,sizeof(vertices),&data,D3DLOCK_READONLY));
 if(valid){std::memcpy(vertices,data,sizeof(vertices));vb->Unlock();}vb->Release();if(!valid)return false;
 const float extra=float((pixels-1024)/2)*640.f/1024.f;
 for(auto&v:vertices){
  v.x+=v.x<0?-extra:float(pixels-1024)*640.f/1024.f-extra;v.u=v.x<0?0.f:float(pixels)/2048.f;
  // Reveal another 40 logical units below the native crop; preserve its
  // texels-per-unit ratio. Native toolbar and compass draws remain above it.
  if(v.z<0){v.z=-240.f;v.v*=480.f/440.f;}
 }
 IDirect3DStateBlock9*block=nullptr;if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&block)))return false;
 if(FAILED(block->Capture())){block->Release();return false;}
 d->SetTexture(0,expandedMapTexture);d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
 const WORD indices[]={0,1,2,2,1,3};
 HRESULT result=originalDrawIndexedUP(d,D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,vertices,sizeof(Vertex));block->Apply();block->Release();
 if(FAILED(result))return false;
 mapExpandedFrame=frames.load();return true;
}
static bool installMapExpansion(unsigned char*base){
 const unsigned char signature[]={0xa1,0x4c,0x55,0x72,0x00,0x83,0xec,0x58,0x55};
 if(std::memcmp(base+0x458d0,signature,sizeof(signature)))return false;
 return MH_CreateHook(base+0x458d0,reinterpret_cast<void*>(observeMapGenerate),reinterpret_cast<void**>(&originalMapGenerate))==MH_OK;
}
