// Extend only verified stock map cutouts. The complete native center remains
// unchanged; reflected side samples add width at the same texels-per-unit.
// A draw-local UV inset omits the stock texture's baked dark outer columns.
struct MapDecorationVertex {float x,y,z;DWORD color;float u,v;};
struct MapDecorationAsset {IDirect3DTexture9* texture;unsigned long long hash;};
static std::vector<MapDecorationAsset> mapDecorationAssets;
static void clearMapDecoration(){for(auto&a:mapDecorationAssets)a.texture->Release();mapDecorationAssets.clear();}
static unsigned long long mapDecorationHash(IDirect3DTexture9*t,const D3DSURFACE_DESC&desc){
 for(auto&a:mapDecorationAssets)if(a.texture==t)return a.hash;
 if(mapDecorationAssets.size()>=8)return 0;
 D3DLOCKED_RECT lock{};if(FAILED(t->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))return 0;
 unsigned long long hash=14695981039346656037ull;
 const unsigned bytes=((desc.Width+3)/4)*16;
 for(unsigned y=0;y<(desc.Height+3)/4;++y)for(unsigned x=0;x<bytes;++x){hash^=static_cast<unsigned char*>(lock.pBits)[y*lock.Pitch+x];hash*=1099511628211ull;}
 t->UnlockRect(0);t->AddRef();mapDecorationAssets.push_back({t,hash});return hash;
}
static bool mapDecorationMesh(const MapDecorationVertex native[4],float extra,MapDecorationVertex result[12]){
 if(!std::isfinite(extra)||extra<=0)return false;
 const float span=native[1].x-native[0].x;
 if(!std::isfinite(span)||span<=0||extra>span/2)return false;
 // The native order is bottom-left, bottom-right, top-left, top-right.
 if(native[0].x!=native[2].x||native[1].x!=native[3].x||native[0].z!=native[1].z||native[2].z!=native[3].z||native[0].z>=native[2].z)return false;
 for(unsigned i=0;i<4;++i){const auto&v=native[i];if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||v.y!=native[0].y)return false;}
 if(native[0].u!=native[2].u||native[1].u!=native[3].u||native[0].v!=native[1].v||native[2].v!=native[3].v)return false;
 std::memcpy(result+4,native,4*sizeof(*native));
 for(unsigned row=0;row<2;++row){
  const auto&left=native[row*2];const auto&right=native[row*2+1];
  result[row*2]=left;result[row*2+1]=left;
  result[row*2].x-=extra;result[row*2].u=left.u+(right.u-left.u)*extra/span;
  result[8+row*2]=right;result[9+row*2]=right;
  result[9+row*2].x+=extra;result[9+row*2].u=right.u-(right.u-left.u)*extra/span;
 }
 return true;
}
static bool drawMapDecoration(IDirect3DDevice9*d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT count,UINT primitives){
 if(type!=D3DPT_TRIANGLESTRIP&&type!=D3DPT_TRIANGLELIST)return false;
 if(!mapExpansionEnabled||mapExpandedFrame!=frames.load()||d!=targetDevice||!interfaceView||!centerUi||!fillBackgrounds||count!=4||primitives!=2)return false;
 const int w=width.load(),h=height.load();if(h<=0||3ll*w<=4ll*h||3ll*w>8ll*h)return false;
 DWORD fvf=0;if(FAILED(d->GetFVF(&fvf))||fvf!=0x142)return false;
 D3DVIEWPORT9 viewport{};if(FAILED(d->GetViewport(&viewport))||viewport.X||viewport.Y||viewport.Width!=unsigned(w)||viewport.Height!=unsigned(h))return false;
 D3DMATRIX world{},view{},projection{};
 if(FAILED(d->GetTransform(D3DTS_WORLD,&world))||FAILED(d->GetTransform(D3DTS_VIEW,&view))||FAILED(d->GetTransform(D3DTS_PROJECTION,&projection)))return false;
 if(!closeTo(world._11,.625f)||!closeTo(world._33,.625f)||!closeTo(world._41,0)||!closeTo(view._43,640)||!closeTo(projection._33,2.5f)||!closeTo(projection._34,1))return false;
 if(!closeTo(world._12,0)||!closeTo(world._13,0)||!closeTo(world._14,0)||!closeTo(world._21,0)||!closeTo(world._23,0)||!closeTo(world._24,0)||!closeTo(world._31,0)||!closeTo(world._32,0)||!closeTo(world._34,0)||!closeTo(world._44,1))return false;
 IDirect3DBaseTexture9*resource=nullptr;if(FAILED(d->GetTexture(0,&resource))||!resource)return false;
 D3DSURFACE_DESC desc{};unsigned long long hash=0;
 if(resource->GetType()==D3DRTYPE_TEXTURE){auto*t=static_cast<IDirect3DTexture9*>(resource);
  if(SUCCEEDED(t->GetLevelDesc(0,&desc))&&desc.Width==1024&&!(desc.Usage&D3DUSAGE_RENDERTARGET)&&((desc.Height==512&&desc.Format==D3DFMT_DXT3)||(desc.Height==256&&desc.Format==D3DFMT_DXT5)))hash=mapDecorationHash(t,desc);
 }
 resource->Release();if(hash!=0x07687d5743b2ca34ull&&hash!=0xcd3a352b015adea1ull)return false;
 IDirect3DVertexBuffer9*vb=nullptr;UINT offset=0,stride=0;void*data=nullptr;const int first=base+minimum;
 if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||!vb)return false;
 MapDecorationVertex native[4]{},vertices[12]{};
 bool valid=first>=0&&stride==sizeof(*native)&&SUCCEEDED(vb->Lock(offset+first*stride,sizeof(native),&data,D3DLOCK_READONLY));
 if(valid){std::memcpy(native,data,sizeof(native));vb->Unlock();}vb->Release();
 const float extra=320.f*(3.f*w/(4.f*h)-1.f)/world._11;
 if(!valid||!closeTo(native[0].x,-512)||!closeTo(native[1].x,512)||!mapDecorationMesh(native,extra,vertices))return false;
 if(native[0].u!=0||native[1].u!=1)return false;
 // Both original masks contain a dark terminal column at the left and a
 // three-column stripe at the right. These were screen edges at 4:3.
 // Omit four columns per side, without touching positions or game assets.
 for(auto&vertex:vertices)vertex.u=(4.f+1016.f*vertex.u)/1024.f;
 IDirect3DStateBlock9*block=nullptr;if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&block)))return false;
 if(FAILED(block->Capture())){block->Release();return false;}
 DWORD addressU=0;d->GetSamplerState(0,D3DSAMP_ADDRESSU,&addressU);
 // Keep filtering inside the texture even if a later stock state differs.
 if(FAILED(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE))||FAILED(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP))){block->Apply();block->Release();return false;}
 const WORD indices[]={0,1,2,2,1,3,4,5,6,6,5,7,8,9,10,10,9,11};
 HRESULT result=originalDrawIndexedUP(d,D3DPT_TRIANGLELIST,0,12,6,indices,D3DFMT_INDEX16,vertices,sizeof(*vertices));
 HRESULT restored=block->Apply();block->Release();
 static unsigned notices[2]={};const unsigned mask=hash==0x07687d5743b2ca34ull?0:1;
 if(journal&&notices[mask]<4){++notices[mask];std::fprintf(journal,"MAP decoration hash=%016llx extra=%g nativeUV=%g,%g..%g,%g world=%g,%g,%g nativeAddressU=%lu result=%lx restored=%lx\n",hash,extra,native[0].u,native[0].v,native[3].u,native[3].v,world._11,world._42,world._43,static_cast<unsigned long>(addressU),static_cast<unsigned long>(result),static_cast<unsigned long>(restored));std::fflush(journal);}
 return SUCCEEDED(result);
}
