// Scoped world-texture filtering: restore native sampler values after each draw
// so the game's state cache and subsequent screen-space artwork stay coherent.
static int textureFiltering=0,anisotropyRequested=0; // 0 native, 1 bilinear, 2 trilinear
struct TextureFilteringState {
 IDirect3DDevice9*d;struct Saved {DWORD stage,values[4];};Saved saved[16]{};unsigned count=0;
 static constexpr D3DSAMPLERSTATETYPE types[4]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_MAXANISOTROPY};
 TextureFilteringState(IDirect3DDevice9*device):d(device){
  if(d!=targetDevice||interfaceView||(!textureFiltering&&!anisotropyRequested))return;
  D3DCAPS9 caps{};if(FAILED(d->GetDeviceCaps(&caps)))return;
  DWORD af=(caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC)?std::min(DWORD(anisotropyRequested),caps.MaxAnisotropy):0;
  if(!(caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFLINEAR)||!(caps.TextureFilterCaps&D3DPTFILTERCAPS_MAGFLINEAR))return;
  for(DWORD stage=0;stage<16;++stage){
   IDirect3DBaseTexture9*resource=nullptr;if(FAILED(d->GetTexture(stage,&resource))||!resource)continue;
   D3DSURFACE_DESC desc{};bool eligible=false;
   if(resource->GetType()==D3DRTYPE_TEXTURE){auto*t=static_cast<IDirect3DTexture9*>(resource);
    eligible=t->GetLevelCount()>1&&SUCCEEDED(t->GetLevelDesc(0,&desc))&&!(desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL));}
   resource->Release();if(!eligible)continue;
   // Only ordinary color textures; preserve data/depth/special-format sampling.
   if(desc.Format!=D3DFMT_DXT1&&desc.Format!=D3DFMT_DXT3&&desc.Format!=D3DFMT_DXT5&&desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8&&desc.Format!=D3DFMT_R5G6B5&&desc.Format!=D3DFMT_A4R4G4B4&&desc.Format!=D3DFMT_A1R5G5B5)continue;
   Saved old{};old.stage=stage;bool ok=true;for(unsigned i=0;i<4;++i)if(FAILED(d->GetSamplerState(stage,types[i],&old.values[i])))ok=false;
   if(!ok||old.values[0]==D3DTEXF_POINT||old.values[2]==D3DTEXF_NONE)continue;
   DWORD next[4]={old.values[0],old.values[1],old.values[2],old.values[3]};
   if(textureFiltering){next[0]=next[1]=D3DTEXF_LINEAR;next[2]=textureFiltering==2&&(caps.TextureFilterCaps&D3DPTFILTERCAPS_MIPFLINEAR)?D3DTEXF_LINEAR:D3DTEXF_POINT;}
   if(af>1){next[0]=D3DTEXF_ANISOTROPIC;next[1]=D3DTEXF_LINEAR;next[3]=af;}
   if(!std::memcmp(next,old.values,sizeof(next)))continue;
   saved[count++]=old;
   for(unsigned i=0;i<4;++i)if(next[i]!=old.values[i]&&FAILED(d->SetSamplerState(stage,types[i],next[i]))){for(unsigned j=0;j<4;++j)d->SetSamplerState(stage,types[j],old.values[j]);--count;ok=false;break;}
   static unsigned notices=0;if(ok&&journal&&notices<8){++notices;std::fprintf(journal,"FILTER stage=%lu texture=%ux%u levels>1 native=%lu,%lu,%lu,%lu effective=%lu,%lu,%lu,%lu cap=%lu\n",static_cast<unsigned long>(stage),desc.Width,desc.Height,static_cast<unsigned long>(old.values[0]),static_cast<unsigned long>(old.values[1]),static_cast<unsigned long>(old.values[2]),static_cast<unsigned long>(old.values[3]),static_cast<unsigned long>(next[0]),static_cast<unsigned long>(next[1]),static_cast<unsigned long>(next[2]),static_cast<unsigned long>(next[3]),static_cast<unsigned long>(caps.MaxAnisotropy));std::fflush(journal);}
  }
 }
 ~TextureFilteringState(){for(unsigned n=0;n<count;++n)for(unsigned i=0;i<4;++i)d->SetSamplerState(saved[n].stage,types[i],saved[n].values[i]);}
 TextureFilteringState(const TextureFilteringState&)=delete;TextureFilteringState&operator=(const TextureFilteringState&)=delete;
};
