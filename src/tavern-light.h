// Recompose only the Spanish tavern's doorway beam in the overview camera.
// Its two near-identical additive meshes must receive the same correction in
// both the small scene-copy target and the main target to avoid ghosting.
static D3DMATRIX lightProjection(D3DMATRIX p,float right,int w,int h){
 const float amount=std::clamp((3.f*w/(4.f*h)-1)*3.f,0.f,1.f);
 const float scale=1+.12f*amount;
 const float shift=(.97f-right)*amount;
 for(unsigned i=0;i<4;++i)p.m[i][0]=scale*p.m[i][0]+((1-scale)*right+shift)*p.m[i][3];
 return p;
}
struct TavernLightState {
 IDirect3DDevice9*device;D3DMATRIX saved{};bool active=false;
 TavernLightState(IDirect3DDevice9*d,INT base,UINT minimum,UINT count,UINT primitives):device(d){
  const int w=width.load(),h=height.load();
  if(d!=targetDevice||interfaceView||!widenWorld||h<=0||3*w<=4*h||count!=14||primitives!=21)return;
  DWORD fvf=0;d->GetFVF(&fvf);if(fvf!=0x152)return;
  D3DMATRIX world{},view{};d->GetTransform(D3DTS_WORLD,&world);d->GetTransform(D3DTS_VIEW,&view);
  // Exclude governor windows and tavern closeups sharing the effect material.
  if(std::fabs(world._41-286.2f)>1||std::fabs(world._42+58.4851f)>1||std::fabs(world._43-20.1118f)>1||
     std::fabs(view._11-.723331f)>.001f||std::fabs(view._21+.690229f)>.001f||std::fabs(view._41+122.373f)>.1f)return;
  IDirect3DBaseTexture9*resource=nullptr;d->GetTexture(0,&resource);if(!resource)return;
  bool match=false;
  if(resource->GetType()==D3DRTYPE_TEXTURE){auto texture=static_cast<IDirect3DTexture9*>(resource);D3DSURFACE_DESC desc{};
   if(SUCCEEDED(texture->GetLevelDesc(0,&desc))&&desc.Width==128&&desc.Height==256)match=townFingerprint(texture,desc)==0x145bd1cbe0ed1285ull;
  }resource->Release();if(!match)return;
  IDirect3DVertexBuffer9*vb=nullptr;UINT offset=0,stride=0;d->GetStreamSource(0,&vb,&offset,&stride);if(!vb)return;
  void*data=nullptr;const int first=base+minimum;float right=-1;
  if(first>=0&&stride==36&&SUCCEEDED(vb->Lock(offset+first*stride,count*stride,&data,D3DLOCK_READONLY))){
   d->GetTransform(D3DTS_PROJECTION,&saved);
   for(unsigned i=0;i<count;++i){float clip[4];townClip(reinterpret_cast<float*>(static_cast<unsigned char*>(data)+i*stride),world,view,saved,clip);
    if(clip[3]>0)right=std::max(right,clip[0]/clip[3]);
   }vb->Unlock();
  }vb->Release();if(right<=0||right>=1)return;
  const auto moved=lightProjection(saved,right,w,h);active=SUCCEEDED(originalTransform(d,D3DTS_PROJECTION,&moved));
  if(active&&traceEnabled&&frames.load()%300==0)std::fprintf(journal,"LIGHT frame=%u right=%g newRight=.97 widthScale=1.12\n",frames.load(),right);
 }
 ~TavernLightState(){if(active)originalTransform(device,D3DTS_PROJECTION,&saved);}
};
