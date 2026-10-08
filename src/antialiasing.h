// Owned multisample main target. The game's textures and swap chain stay native.
// Logical surface identities are preserved for engine target caches and overlays.
using AaGetTarget=HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,IDirect3DSurface9**);
using AaSetTarget=HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
using AaGetDepth=HRESULT(WINAPI*)(IDirect3DDevice9*,IDirect3DSurface9**);
using AaSetDepth=HRESULT(WINAPI*)(IDirect3DDevice9*,IDirect3DSurface9*);
using AaClear=HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD);
using AaBegin=HRESULT(WINAPI*)(IDirect3DDevice9*);
using AaSetState=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DRENDERSTATETYPE,DWORD);
static AaGetTarget aaGetTarget=nullptr;static AaSetTarget aaSetTarget=nullptr;
static AaGetDepth aaGetDepth=nullptr;static AaSetDepth aaSetDepth=nullptr;
static AaClear aaClear=nullptr;static AaBegin aaBegin=nullptr;static AaSetState aaSetState=nullptr;
static int aaRequested=0;
struct AaMainTarget {
 IDirect3DSurface9 *back=nullptr,*nativeDepth=nullptr,*color=nullptr,*depth=nullptr;
 bool attempted=false,dirty=false,failed=false;unsigned samples=0,resolves=0;
 bool selected(IDirect3DDevice9* d){IDirect3DSurface9* actual=nullptr;
  const bool yes=color&&SUCCEEDED(aaGetTarget(d,0,&actual))&&actual==color;if(actual)actual->Release();return yes;}
 void release(IDirect3DDevice9* d){
  if(color&&selected(d)){aaSetDepth(d,nullptr);aaSetTarget(d,0,back);aaSetDepth(d,nativeDepth);}
  if(depth)depth->Release();
  if(color)color->Release();
  if(nativeDepth)nativeDepth->Release();
  if(back)back->Release();
  back=nativeDepth=color=depth=nullptr;attempted=dirty=failed=false;samples=0;
 }
 bool ensure(IDirect3DDevice9* d){
  if(aaRequested==0||failed)return false;
  if(color)return true;
  if(attempted)return false;
  IDirect3DSurface9 *bb=nullptr,*current=nullptr,*ds=nullptr;D3DSURFACE_DESC bd{},dd{};
  bool ok=SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb))&&SUCCEEDED(aaGetTarget(d,0,&current));
  // Wait until the engine binds its main target and depth surface.
  ok=ok&&bb==current&&SUCCEEDED(aaGetDepth(d,&ds))&&ds;
  if(current)current->Release();
  if(!ok){if(bb)bb->Release();if(ds)ds->Release();return false;}
  attempted=true;ok=SUCCEEDED(bb->GetDesc(&bd))&&SUCCEEDED(ds->GetDesc(&dd));
  std::fprintf(journal,"AA native main target=%ux%u color=%u depth=%u multisample=%u requested=%d\n",bd.Width,bd.Height,unsigned(bd.Format),unsigned(dd.Format),unsigned(bd.MultiSampleType),aaRequested);
  D3DDEVICE_CREATION_PARAMETERS creation{};IDirect3D9* api=nullptr;
  IDirect3DSwapChain9* swap=nullptr;D3DPRESENT_PARAMETERS pp{};
  ok=ok&&bd.MultiSampleType==D3DMULTISAMPLE_NONE&&SUCCEEDED(d->GetCreationParameters(&creation))&&SUCCEEDED(d->GetDirect3D(&api));
  if(ok)ok=SUCCEEDED(d->GetSwapChain(0,&swap))&&SUCCEEDED(swap->GetPresentParameters(&pp));
  if(swap)swap->Release();
  if(ok)for(int count=aaRequested;count>=2;count/=2){
   const auto type=static_cast<D3DMULTISAMPLE_TYPE>(count);DWORD cq=0,dq=0;
   if(FAILED(api->CheckDeviceMultiSampleType(creation.AdapterOrdinal,creation.DeviceType,bd.Format,pp.Windowed,type,&cq))||
      FAILED(api->CheckDeviceMultiSampleType(creation.AdapterOrdinal,creation.DeviceType,dd.Format,pp.Windowed,type,&dq))||!cq||!dq)continue;
   if(FAILED(d->CreateRenderTarget(bd.Width,bd.Height,bd.Format,type,0,FALSE,&color,nullptr)))continue;
   if(FAILED(d->CreateDepthStencilSurface(bd.Width,bd.Height,dd.Format,type,0,TRUE,&depth,nullptr))){color->Release();color=nullptr;continue;}
   samples=count;break;
  }
  if(api)api->Release();
  if(color){back=bb;nativeDepth=ds;std::fprintf(journal,"AA active %ux MSAA quality=0\n",samples);}
  else{bb->Release();ds->Release();std::fprintf(journal,"AA could not create compatible targets; native rendering retained\n");}
  std::fflush(journal);return color!=nullptr;
 }
 void bind(IDirect3DDevice9* d){
  if(!ensure(d))return;
  IDirect3DSurface9* current=nullptr;if(FAILED(aaGetTarget(d,0,&current)))return;
  const bool main=current==back;if(current)current->Release();if(!main)return;
  D3DVIEWPORT9 viewport{};d->GetViewport(&viewport);
  aaSetDepth(d,nullptr);
  const bool ok=SUCCEEDED(aaSetTarget(d,0,color))&&SUCCEEDED(aaSetDepth(d,depth));
  d->SetViewport(&viewport);
  if(ok)aaSetState(d,D3DRS_MULTISAMPLEANTIALIAS,TRUE);
  else{aaSetDepth(d,nullptr);aaSetTarget(d,0,back);aaSetDepth(d,nativeDepth);d->SetViewport(&viewport);failed=true;
   std::fprintf(journal,"AA target binding failed; native rendering retained\n");std::fflush(journal);}
 }
 bool resolve(IDirect3DDevice9* d){
  if(!dirty||!color||failed)return true;
  const auto result=d->StretchRect(color,nullptr,back,nullptr,D3DTEXF_NONE);dirty=false;
  if(SUCCEEDED(result)){++resolves;if(resolves==1||resolves%600==0){std::fprintf(journal,"AA resolved frames=%u samples=%u\n",resolves,samples);std::fflush(journal);}return true;}
  std::fprintf(journal,"AA resolve failed result=%lx; reverting to native rendering\n",static_cast<unsigned long>(result));std::fflush(journal);
  release(d);failed=true;return false;
 }
};
static AaMainTarget aaMain;
static HRESULT WINAPI onAaGetTarget(IDirect3DDevice9* d,DWORD index,IDirect3DSurface9** surface){
 auto result=aaGetTarget(d,index,surface);
 if(d==targetDevice&&index==0&&SUCCEEDED(result)&&surface&&*surface==aaMain.color&&aaMain.back){(*surface)->Release();*surface=aaMain.back;(*surface)->AddRef();}
 return result;
}
static HRESULT WINAPI onAaGetDepth(IDirect3DDevice9* d,IDirect3DSurface9** surface){
 auto result=aaGetDepth(d,surface);
 if(d==targetDevice&&SUCCEEDED(result)&&surface&&*surface==aaMain.depth&&aaMain.nativeDepth){(*surface)->Release();*surface=aaMain.nativeDepth;(*surface)->AddRef();}
 return result;
}
static HRESULT WINAPI onAaSetDepth(IDirect3DDevice9* d,IDirect3DSurface9* surface){
 if(d==targetDevice&&!aaMain.failed&&surface==aaMain.nativeDepth&&aaMain.selected(d))surface=aaMain.depth;
 return aaSetDepth(d,surface);
}
static HRESULT WINAPI onAaSetTarget(IDirect3DDevice9* d,DWORD index,IDirect3DSurface9* surface){
 if(d!=targetDevice||index!=0||!aaMain.color||aaMain.failed)return aaSetTarget(d,index,surface);
 IDirect3DSurface9 *savedDepth=nullptr,*savedTarget=nullptr;aaGetDepth(d,&savedDepth);aaGetTarget(d,0,&savedTarget);
 D3DVIEWPORT9 viewport{};d->GetViewport(&viewport);
 IDirect3DSurface9* logical=savedDepth==aaMain.depth?aaMain.nativeDepth:savedDepth;
 const bool main=surface==aaMain.back||surface==aaMain.color;
 aaSetDepth(d,nullptr);auto result=aaSetTarget(d,index,main?aaMain.color:surface);
 auto depthResult=aaSetDepth(d,FAILED(result)?savedDepth:main&&logical==aaMain.nativeDepth?aaMain.depth:logical);
 if(FAILED(result)||FAILED(depthResult)){aaSetDepth(d,nullptr);aaSetTarget(d,0,savedTarget);aaSetDepth(d,savedDepth);d->SetViewport(&viewport);}
 if(savedTarget)savedTarget->Release();
 if(savedDepth)savedDepth->Release();
 if(SUCCEEDED(result)&&main)aaSetState(d,D3DRS_MULTISAMPLEANTIALIAS,TRUE);
 return FAILED(result)?result:depthResult;
}
static HRESULT WINAPI onAaClear(IDirect3DDevice9* d,DWORD count,const D3DRECT* rects,DWORD flags,D3DCOLOR color,float z,DWORD stencil){
 if(d==targetDevice){aaMain.bind(d);if(aaMain.selected(d))aaMain.dirty=true;}
 return aaClear(d,count,rects,flags,color,z,stencil);
}
static HRESULT WINAPI onAaBegin(IDirect3DDevice9* d){
 if(d==targetDevice){aaMain.bind(d);if(aaMain.selected(d))aaMain.dirty=true;}
 return aaBegin(d);
}
static HRESULT WINAPI onAaSetState(IDirect3DDevice9* d,D3DRENDERSTATETYPE state,DWORD value){
 if(d==targetDevice&&state==D3DRS_MULTISAMPLEANTIALIAS&&aaMain.selected(d))value=TRUE;
 return aaSetState(d,state,value);
}
