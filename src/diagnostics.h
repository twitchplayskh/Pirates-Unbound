// Opt-in, local graphics tracing. Never enabled in the release defaults.
static bool traceEnabled=false;
static bool forceTrace=false;
static char traceFolder[MAX_PATH]{};
static unsigned traceRows=0,traceFrame=~0u;
static void traceAssetFingerprint(IDirect3DTexture9*texture){
 static IDirect3DTexture9*seen[256]{};static unsigned seenCount=0,seenFrame=~0u;
 if(seenFrame!=frames.load()){seenFrame=frames.load();seenCount=0;}
 for(unsigned i=0;i<seenCount;++i)if(seen[i]==texture)return;
 if(seenCount==256)return;
 seen[seenCount++]=texture;
 D3DSURFACE_DESC desc{};if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Usage&D3DUSAGE_RENDERTARGET)return;
 unsigned rows=desc.Height,rowBytes=0;
 if(desc.Format==D3DFMT_DXT1){rows=(rows+3)/4;rowBytes=(desc.Width+3)/4*8;}
 else if(desc.Format==D3DFMT_DXT3||desc.Format==D3DFMT_DXT5){rows=(rows+3)/4;rowBytes=(desc.Width+3)/4*16;}
 else if(desc.Format==D3DFMT_A8R8G8B8||desc.Format==D3DFMT_X8R8G8B8)rowBytes=desc.Width*4;
 if(!rowBytes)return;
 D3DLOCKED_RECT rect{};if(FAILED(texture->LockRect(0,&rect,nullptr,D3DLOCK_READONLY)))return;
 unsigned long long hash=14695981039346656037ull;
 for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<rowBytes;++x){hash^=static_cast<unsigned char*>(rect.pBits)[y*rect.Pitch+x];hash*=1099511628211ull;}
 texture->UnlockRect(0);
 std::fprintf(journal,"ASSET tex=%p size=%ux%u format=%lu fingerprint=%016llx\n",texture,desc.Width,desc.Height,static_cast<unsigned long>(desc.Format),hash);
}
static bool traceSample(){
 const auto f=frames.load();if(!traceEnabled||(!forceTrace&&f%30!=0))return false;
 if(traceFrame!=f){traceFrame=f;traceRows=0;}return traceRows++<5000;
}
static void traceTexture(IDirect3DDevice9*d,IDirect3DTexture9*texture,unsigned stage){
 D3DSURFACE_DESC desc{};if(FAILED(texture->GetLevelDesc(0,&desc))||!(desc.Usage&D3DUSAGE_RENDERTARGET)||desc.Width>1024||desc.Height>1024)return;
 if(desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8)return;
 IDirect3DSurface9*gpu=nullptr,*cpu=nullptr;
 if(FAILED(texture->GetSurfaceLevel(0,&gpu)))return;
 if(SUCCEEDED(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr))&&SUCCEEDED(d->GetRenderTargetData(gpu,cpu))){
  D3DLOCKED_RECT rect{};if(SUCCEEDED(cpu->LockRect(&rect,nullptr,D3DLOCK_READONLY))){
   char path[MAX_PATH];std::snprintf(path,sizeof(path),"%strace-%u-%u-%p.bmp",traceFolder,frames.load(),stage,texture);
   FILE*file=std::fopen(path,"wb");if(file){
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+desc.Width*desc.Height*4;
    info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    std::fwrite(&header,sizeof(header),1,file);std::fwrite(&info,sizeof(info),1,file);
    for(unsigned y=0;y<desc.Height;++y)std::fwrite(static_cast<unsigned char*>(rect.pBits)+y*rect.Pitch,4,desc.Width,file);
    std::fclose(file);
   }cpu->UnlockRect();
  }
 }if(cpu)cpu->Release();gpu->Release();
}
static void traceDraw(IDirect3DDevice9*d,const char*kind,unsigned count,INT base=0,UINT minimum=0,UINT vertices=0){
 if((d!=targetDevice&&!forceTrace)||!traceSample())return;
 DWORD fvf=0,blend=0,src=0,dst=0;D3DMATRIX m{};D3DVIEWPORT9 viewport{};D3DSURFACE_DESC rt{};IDirect3DSurface9*surface=nullptr;
 d->GetFVF(&fvf);d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);d->GetRenderState(D3DRS_SRCBLEND,&src);d->GetRenderState(D3DRS_DESTBLEND,&dst);d->GetTransform(D3DTS_PROJECTION,&m);d->GetViewport(&viewport);
 if(SUCCEEDED(d->GetRenderTarget(0,&surface))){surface->GetDesc(&rt);surface->Release();}
 std::fprintf(journal,"TRACE f=%u draw=%u kind=%s n=%u vertices=%u ui=%d fvf=%lx rt=%ux%u vp=%lux%lu proj=%g,%g,%g,%g blend=%lu,%lu,%lu",frames.load(),traceRows,kind,count,vertices,interfaceView,fvf,rt.Width,rt.Height,viewport.Width,viewport.Height,m._11,m._22,m._33,m._34,blend,src,dst);
 for(unsigned stage=0;stage<4;++stage){
  IDirect3DBaseTexture9*texture=nullptr;if(SUCCEEDED(d->GetTexture(stage,&texture))&&texture){
   if(texture->GetType()==D3DRTYPE_TEXTURE){auto tex=static_cast<IDirect3DTexture9*>(texture);D3DSURFACE_DESC desc{};tex->GetLevelDesc(0,&desc);std::fprintf(journal," tex%u=%p:%ux%u:usage%lx",stage,tex,desc.Width,desc.Height,desc.Usage);if(count==2)traceTexture(d,tex,stage);}
   texture->Release();
  }
 }
 if(vertices==4||vertices==14){IDirect3DVertexBuffer9*buffer=nullptr;UINT offset=0,stride=0;d->GetStreamSource(0,&buffer,&offset,&stride);void*data=nullptr;int first=base+minimum;
  if(buffer&&first>=0&&stride>=12&&SUCCEEDED(buffer->Lock(offset+first*stride,vertices*stride,&data,D3DLOCK_READONLY))){for(unsigned i=0;i<vertices;++i){auto v=reinterpret_cast<float*>(static_cast<unsigned char*>(data)+i*stride);std::fprintf(journal," xyz%u=%g,%g,%g",i,v[0],v[1],v[2]);}buffer->Unlock();}if(buffer)buffer->Release();
 }
 std::fputc('\n',journal);std::fflush(journal);
 if(vertices==4||vertices==14){D3DMATRIX world{},view{};d->GetTransform(D3DTS_WORLD,&world);d->GetTransform(D3DTS_VIEW,&view);std::fprintf(journal,"MATRICES draw=%u world=",traceRows);for(float v:world.m[0])std::fprintf(journal," %g",v);for(unsigned row=1;row<4;++row)for(float v:world.m[row])std::fprintf(journal," %g",v);std::fprintf(journal," view=");for(unsigned row=0;row<4;++row)for(float v:view.m[row])std::fprintf(journal," %g",v);std::fputc('\n',journal);}
 for(unsigned stage=0;stage<4;++stage){IDirect3DBaseTexture9*texture=nullptr;if(SUCCEEDED(d->GetTexture(stage,&texture))&&texture){if(texture->GetType()==D3DRTYPE_TEXTURE)traceAssetFingerprint(static_cast<IDirect3DTexture9*>(texture));texture->Release();}}
 static unsigned constantsFrame=~0u;static UINT lastWidth=0;
 if(constantsFrame!=frames.load()||lastWidth!=rt.Width){constantsFrame=frames.load();lastWidth=rt.Width;float c[32]{};d->GetVertexShaderConstantF(0,c,8);std::fprintf(journal,"TRACE constants rt=%u",rt.Width);for(unsigned i=0;i<32;++i)std::fprintf(journal," %g",c[i]);std::fputc('\n',journal);std::fflush(journal);}
}
