// Recognize shipped town artwork across nations and wealth levels.
// Scenery only: UI and picking remain untouched.
#include "town-assets.h"
static bool readable(const void*,size_t);
static bool townLayoutActive=false;
static unsigned lastTownDraw=~0u;
static unsigned townShiftFrame=~0u;
static float townShift=0;
struct TownAsset { IDirect3DTexture9* texture; unsigned long long hash; };
static TownAsset townAssets[64]{};
static unsigned townAssetCount=0;
static IDirect3DStateBlock9* townStateBlock=nullptr;
static void clearTownAssets(){
 for(unsigned i=0;i<townAssetCount;++i)townAssets[i].texture->Release();
 townAssetCount=0;townShiftFrame=~0u;townShift=0;townLayoutActive=false;lastTownDraw=~0u;
 if(townStateBlock){townStateBlock->Release();townStateBlock=nullptr;}
}
static void updateTownMenu(){
 // Recognize actual scenery draws, independent of language, city name or
 // transient dialogue. Keep the outgoing layout through captured fades.
 if(townLayoutActive&&frames.load()-lastTownDraw>2)clearTownAssets();
}
static unsigned long long townFingerprint(IDirect3DTexture9* texture,const D3DSURFACE_DESC& desc){
 for(unsigned i=0;i<townAssetCount;++i)if(townAssets[i].texture==texture)return townAssets[i].hash;
 unsigned bytes=0;
 if(desc.Format==D3DFMT_DXT1)bytes=((desc.Width+3)/4)*8;
 if(desc.Format==D3DFMT_DXT3||desc.Format==D3DFMT_DXT5)bytes=((desc.Width+3)/4)*16;
 if(!bytes||desc.Usage&D3DUSAGE_RENDERTARGET)return 0;
 D3DLOCKED_RECT lock{};
 if(FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))return 0;
 unsigned long long hash=14695981039346656037ull;
 for(unsigned y=0;y<(desc.Height+3)/4;++y)for(unsigned x=0;x<bytes;++x){hash^=static_cast<unsigned char*>(lock.pBits)[y*lock.Pitch+x];hash*=1099511628211ull;}
 texture->UnlockRect(0);
 // Retaining resources prevents recycled pointers matching stale hashes.
 if(townAssetCount<64){texture->AddRef();townAssets[townAssetCount++]={texture,hash};}return hash;
}
static void townMultiply(const float in[4],const D3DMATRIX& m,float out[4]){
 for(unsigned j=0;j<4;++j){out[j]=0;for(unsigned i=0;i<4;++i)out[j]+=in[i]*m.m[i][j];}
}
static void townClip(const float* xyz,const D3DMATRIX& world,const D3DMATRIX& view,const D3DMATRIX& projection,float out[4]){
 float local[4]={xyz[0],xyz[1],xyz[2],1},a[4],b[4];
 townMultiply(local,world,a);townMultiply(a,view,b);townMultiply(b,projection,out);
}
static D3DMATRIX townTranslatedProjection(D3DMATRIX projection,float shift){
 // Row vectors: x_clip += shift*w_clip. This preserves size and aspect.
 for(unsigned i=0;i<4;++i)projection.m[i][0]+=shift*projection.m[i][3];
 return projection;
}
struct TownDrawState {
 IDirect3DDevice9* device;D3DMATRIX saved{};bool active=false,clipped=false;
 DWORD oldScissorEnabled=0;RECT oldScissor{};
 TownDrawState(IDirect3DDevice9* d,INT base,UINT minimum,UINT count,UINT primitives):device(d){
  const int w=width.load(),h=height.load();
  if(d!=targetDevice||!widenWorld||!centerUi||h<=0||3*w<=4*h)return;
  D3DVIEWPORT9 vp{};if(FAILED(d->GetViewport(&vp))||vp.X||vp.Y||!vp.Width||!vp.Height)return;
  // Scene transitions capture to square textures using the output camera.
  if((vp.Width!=unsigned(w)||vp.Height!=unsigned(h))&&!(vp.Width<=1024&&vp.Height<=1024)){
   if(traceEnabled)std::fprintf(journal,"TOWN skip viewport %lux%lu\n",vp.Width,vp.Height);
   return;
  }
  DWORD fvf=0;d->GetFVF(&fvf);if(fvf!=0x112&&fvf!=0x152)return;
  const bool backgroundGeometry=count==4&&primitives==2&&fvf==0x112;
  if(!backgroundGeometry&&townShiftFrame!=frames.load())return;
  IDirect3DBaseTexture9* resource=nullptr;
  if(FAILED(d->GetTexture(0,&resource))||!resource)return;
  if(resource->GetType()!=D3DRTYPE_TEXTURE){resource->Release();return;}
  auto texture=static_cast<IDirect3DTexture9*>(resource);D3DSURFACE_DESC desc{};
  if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Width>1024||desc.Height>1024||desc.Usage&D3DUSAGE_RENDERTARGET){resource->Release();return;}
  const bool backgroundSize=backgroundGeometry&&desc.Width==1024&&desc.Height==1024;
  const bool companionSize=townShiftFrame==frames.load()&&desc.Width==desc.Height&&(desc.Width==128||desc.Width==256);
  if(!backgroundSize&&!companionSize){resource->Release();return;}
  auto hash=townFingerprint(texture,desc);resource->Release();
  const bool town=std::find(std::begin(townBackgroundHashes),std::end(townBackgroundHashes),hash)!=std::end(townBackgroundHashes);
  const bool companion=std::find(std::begin(townCompanionHashes),std::end(townCompanionHashes),hash)!=std::end(townCompanionHashes);
  if(!town&&!companion)return;
  if(town){lastTownDraw=frames.load();townLayoutActive=true;}
  if(FAILED(d->GetTransform(D3DTS_PROJECTION,&saved)))return;
  if(saved._33<1||saved._33>1.1f||std::fabs(saved._34-1)>.001f)return;
  if(town&&traceEnabled&&vp.Width<=1024)std::fprintf(journal,"TOWN capture frame=%u vp=%lux%lu proj=%g uiFlag=%d\n",frames.load(),vp.Width,vp.Height,saved._11,interfaceView);
  if(companion){if(townShiftFrame!=frames.load())return;auto moved=townTranslatedProjection(saved,townShift);active=SUCCEEDED(originalTransform(d,D3DTS_PROJECTION,&moved));return;}
  if(count!=4||primitives!=2||fvf!=0x112)return;
  IDirect3DVertexBuffer9* vb=nullptr;UINT offset=0,stride=0;
  if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||!vb)return;
  alignas(float) unsigned char verts[4][32]{};void* data=nullptr;const int first=base+minimum;
  bool valid=first>=0&&stride==32&&SUCCEEDED(vb->Lock(offset+first*stride,4*stride,&data,D3DLOCK_READONLY));
  if(valid){std::memcpy(verts,data,sizeof(verts));vb->Unlock();}vb->Release();if(!valid)return;
  D3DMATRIX world{},view{};
  if(FAILED(d->GetTransform(D3DTS_WORLD,&world))||FAILED(d->GetTransform(D3DTS_VIEW,&view)))return;
  float clip[4][4],ndc[4];unsigned order[4]={0,1,2,3};
  for(unsigned i=0;i<4;++i){townClip(reinterpret_cast<float*>(verts[i]),world,view,saved,clip[i]);if(clip[i][3]<=0)return;ndc[i]=clip[i][0]/clip[i][3];}
  std::sort(order,order+4,[&](unsigned a,unsigned b){return ndc[a]<ndc[b];});
  const float left=ndc[order[0]],right=ndc[order[3]];
  if(right>=1||right-left<0.1f)return;
  townShift=1-right;auto moved=townTranslatedProjection(saved,townShift);
  // Copy only the left edge's UVs to a new strip; original art is unscaled.
  alignas(float) unsigned char extension[4][32]{};
  float blueBoundary=-1;
  for(unsigned k=0;k<2;++k){
   const unsigned edge=order[k];unsigned opposite=order[2];auto xyz=reinterpret_cast<float*>(verts[edge]);
   for(unsigned j=2;j<4;++j)if(std::fabs(reinterpret_cast<float*>(verts[order[j]])[0]-xyz[0])<0.01f)opposite=order[j];
   const float a=clip[edge][0]+(1+townShift)*clip[edge][3],b=clip[opposite][0]+(1+townShift)*clip[opposite][3];
   if(std::fabs(b-a)<0.0001f)return;
   const float t=-a/(b-a);
   std::memcpy(extension[k],verts[edge],32);std::memcpy(extension[k+2],verts[edge],32);
   // Stay inside the blue edge, away from the texture's border/wrap seam.
   const float edgeU=reinterpret_cast<float*>(verts[edge])[6];
   const float safeU=edgeU<0.5f?4.5f/1024.f:1.f-4.5f/1024.f;
   reinterpret_cast<float*>(extension[k])[6]=safeU;
   reinterpret_cast<float*>(extension[k+2])[6]=safeU;
   auto extended=reinterpret_cast<float*>(extension[k+2]);auto other=reinterpret_cast<float*>(verts[opposite]);
   for(unsigned j=0;j<3;++j)extended[j]=xyz[j]+t*(other[j]-xyz[j]);
   // Cover the texture's narrow dark border using blue only. Clip that border
   // from the stock art draw; do not rescale or alter any scenery pixels.
   auto inner=reinterpret_cast<float*>(extension[k]);
   for(unsigned j=0;j<3;++j)inner[j]=xyz[j]+(4.5f/1024.f)*(other[j]-xyz[j]);
   float innerClip[4];townClip(inner,world,view,moved,innerClip);
   blueBoundary=std::max(blueBoundary,innerClip[0]/innerClip[3]);
  }
  const LONG seamPixel=static_cast<LONG>(std::floor((blueBoundary+1)*vp.Width/2));
  const float seamNdc=2.f*seamPixel/vp.Width-1;
  for(unsigned k=0;k<2;++k){
   const unsigned edge=order[k];unsigned opposite=order[2];auto xyz=reinterpret_cast<float*>(verts[edge]);
   for(unsigned j=2;j<4;++j)if(std::fabs(reinterpret_cast<float*>(verts[order[j]])[0]-xyz[0])<0.01f)opposite=order[j];
   const float a=clip[edge][0]+(townShift-seamNdc)*clip[edge][3],b=clip[opposite][0]+(townShift-seamNdc)*clip[opposite][3];
   if(std::fabs(b-a)<0.0001f)return;
   const float t=-a/(b-a);auto inner=reinterpret_cast<float*>(extension[k]);auto other=reinterpret_cast<float*>(verts[opposite]);
   for(unsigned j=0;j<3;++j)inner[j]=xyz[j]+t*(other[j]-xyz[j]);
  }
  if(!townStateBlock&&FAILED(d->CreateStateBlock(D3DSBT_ALL,&townStateBlock)))return;
  auto block=townStateBlock;
  if(FAILED(block->Capture()))return;
  // UP clears stream zero: full state restoration is essential for the next draw.
  originalTransform(d,D3DTS_PROJECTION,&moved);d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
  d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
  d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
  const WORD indices[]={0,1,2,2,1,3};
  auto result=d->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,extension,32);
  block->Apply();if(FAILED(result))return;
  d->GetRenderState(D3DRS_SCISSORTESTENABLE,&oldScissorEnabled);d->GetScissorRect(&oldScissor);
  RECT clipRect{seamPixel,0,static_cast<LONG>(vp.Width),static_cast<LONG>(vp.Height)};
  if(oldScissorEnabled){clipRect.left=std::max(clipRect.left,oldScissor.left);clipRect.top=std::max(clipRect.top,oldScissor.top);clipRect.right=std::min(clipRect.right,oldScissor.right);clipRect.bottom=std::min(clipRect.bottom,oldScissor.bottom);}
  d->SetScissorRect(&clipRect);d->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);clipped=true;
  active=SUCCEEDED(originalTransform(d,D3DTS_PROJECTION,&moved));
  if(active){townShiftFrame=frames.load();
   if(traceEnabled&&frames.load()%300==0){std::fprintf(journal,"TOWN size=%dx%d aspect=%g fourThree=%g extra=%g oldX=%g newX=%g townWidth=%g right=%g shiftNdc=%g blue=left-edge-column\n",w,h,float(w)/h,4.f*h/3,w-4.f*h/3,(left+1)*w/2,(left+townShift+1)*w/2,(right-left)*w/2,(right+townShift+1)*w/2,townShift);std::fflush(journal);}
  }
 }
 ~TownDrawState(){if(active)originalTransform(device,D3DTS_PROJECTION,&saved);if(clipped){device->SetScissorRect(&oldScissor);device->SetRenderState(D3DRS_SCISSORTESTENABLE,oldScissorEnabled);}}
};
