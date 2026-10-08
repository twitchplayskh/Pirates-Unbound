// Own a copy of this tick's display-overlay geometry and D3D state. Never keep
// engine node pointers or borrow vertex-buffer storage after a native draw.
struct SailingUiDraw {
 bool label=false,effect=false;
 D3DMATRIX world{},view{},projection{};
 D3DVIEWPORT9 viewport{};
 IDirect3DStateBlock9* state=nullptr;
 IDirect3DSurface9* depth=nullptr;
 D3DPRIMITIVETYPE type{};
 unsigned vertices=0,primitives=0,stride=0;
 std::vector<unsigned char> data;
 std::vector<unsigned> indices;
};
static std::vector<SailingUiDraw> sailingUiDraws;
static bool sailingUiFailed=false,sailingUiReplaying=false;
static bool sailingUiPending=false;
#include "sailing-buffer.h"
static bool sailingUiCamera(IDirect3DDevice9* device,SailingUiDraw& draw){
 return SUCCEEDED(device->GetTransform(D3DTS_WORLD,&draw.world))&&SUCCEEDED(device->GetTransform(D3DTS_VIEW,&draw.view))&&SUCCEEDED(device->GetTransform(D3DTS_PROJECTION,&draw.projection))&&SUCCEEDED(device->GetViewport(&draw.viewport));
}
static bool sailingUiApplyCamera(IDirect3DDevice9* device,const SailingUiDraw& draw){
 // Captured matrices already include the widescreen correction. Bypass the
 // transform hook so replay neither corrects them twice nor changes its UI flag.
 auto view=draw.view;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(sailingExtra&&sailingUiReplaying&&(draw.label||draw.effect)&&sailingVisualCameraRestore.camera)
  view=sailingCameraView(draw.view,sailingVisualCameraAdvance);
#endif
 return SUCCEEDED(originalTransform(device,D3DTS_WORLD,&draw.world))&&SUCCEEDED(originalTransform(device,D3DTS_VIEW,&view))&&SUCCEEDED(originalTransform(device,D3DTS_PROJECTION,&draw.projection))&&SUCCEEDED(device->SetViewport(&draw.viewport));
}
static unsigned sailingUiReplays=0;
static void sailingUiDescribeState(IDirect3DDevice9* device){
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(sailingTicks!=12)return;
 DWORD fvf=0,z=0,zwrite=0,zfunc=0,alpha=0,scissor=0;RECT rect{};
 device->GetFVF(&fvf);device->GetRenderState(D3DRS_ZENABLE,&z);device->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite);device->GetRenderState(D3DRS_ZFUNC,&zfunc);
 device->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha);device->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor);device->GetScissorRect(&rect);
 IDirect3DVertexBuffer9* vb=nullptr;UINT offset=0,stride=0;D3DVERTEXBUFFER_DESC desc{};
 device->GetStreamSource(0,&vb,&offset,&stride);if(vb){vb->GetDesc(&desc);vb->Release();}
 std::fprintf(journal,"OVERLAY state fvf=%lx z=%lu write=%lu func=%lu alpha=%lu scissor=%lu rect=%ld,%ld,%ld,%ld vbPool=%u vbUsage=%lx\n",static_cast<unsigned long>(fvf),static_cast<unsigned long>(z),static_cast<unsigned long>(zwrite),static_cast<unsigned long>(zfunc),static_cast<unsigned long>(alpha),static_cast<unsigned long>(scissor),rect.left,rect.top,rect.right,rect.bottom,static_cast<unsigned>(desc.Pool),static_cast<unsigned long>(desc.Usage));
#else
 (void)device;
#endif
}
static void sailingUiDescribe(const SailingUiDraw& draw){
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(sailingTicks!=12)return;
 float lowX=1e20f,lowY=1e20f,highX=-1e20f,highY=-1e20f;
 for(unsigned vertex=0;vertex<draw.vertices;++vertex){
  float p[4]={0,0,0,1};std::memcpy(p,draw.data.data()+vertex*draw.stride,12);
  for(const auto* matrix:{&draw.world,&draw.view,&draw.projection}){
   float next[4]{};for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)next[col]+=p[row]*matrix->m[row][col];std::memcpy(p,next,sizeof(p));
  }
  if(p[3]==0)continue;
  const float x=draw.viewport.X+(p[0]/p[3]+1)*draw.viewport.Width*.5f;
  const float y=draw.viewport.Y+(1-p[1]/p[3])*draw.viewport.Height*.5f;
  lowX=std::min(lowX,x);highX=std::max(highX,x);lowY=std::min(lowY,y);highY=std::max(highY,y);
 }
 std::fprintf(journal,"OVERLAY label=%d effect=%d primitives=%u bounds=%.1f,%.1f,%.1f,%.1f stride=%u\n",draw.label,draw.effect,draw.primitives,lowX,lowY,highX,highY,draw.stride);std::fflush(journal);
#else
 (void)draw;
#endif
}
static void sailingUiClear(){
 for(auto& draw:sailingUiDraws){if(draw.state)draw.state->Release();if(draw.depth)draw.depth->Release();}
 sailingUiDraws.clear();sailingUiFailed=false;sailingUiPending=false;
}
static bool sailingUiReady(){
 if(sailingUiFailed||sailingUiPending)return false;
 // Loading can enter the sailing wait before a HUD pass has drawn. An empty
 // replay is not a complete frame: wait for native interface geometry first.
 for(const auto& draw:sailingUiDraws)if(!draw.label&&!draw.effect)return true;
 return false;
}
static bool sailingUiDisplay(IDirect3DDevice9* device){
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(!sailingCaptureOverlay)return false;
#endif
 if(device!=targetDevice||!(interfaceView||sailingLabelPass||sailingEffectPass)||sailingUiReplaying||!(sailingFrameActive||sailingExtra))return false;
 IDirect3DSurface9* target=nullptr,*buffer=nullptr;
 const bool ok=SUCCEEDED(device->GetRenderTarget(0,&target))&&SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&buffer))&&target==buffer;
 if(target)target->Release();
 if(buffer)buffer->Release();
 return ok;
}
static void sailingUiFail(){
 if(!sailingUiFailed){std::fprintf(journal,"SAILING overlay capture failed; extra redraws disabled\n");std::fflush(journal);}
 sailingUiFailed=true;sailingHealthy=false;
}
static bool sailingUiIndexed(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,int base,unsigned minimum,unsigned vertices,unsigned start,unsigned primitives){
 if(!sailingUiDisplay(device))return false;
 if(sailingExtra)return true;
 if(sailingUiFailed)return false;
 // This game uses one interleaved stream with lists and strips for screen text.
 // Reject unfamiliar layouts instead of replaying an incomplete overlay.
 IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;
 unsigned offset=0,stride=0;D3DINDEXBUFFER_DESC desc{};D3DVERTEXBUFFER_DESC vertexDesc{};
 void* vertexData=nullptr,*indexData=nullptr;SailingUiDraw draw;
 const long long first=static_cast<long long>(base)+minimum;
 const unsigned indexCount=type==D3DPT_TRIANGLESTRIP?primitives+2:3*primitives;
 bool ok=(type==D3DPT_TRIANGLELIST||type==D3DPT_TRIANGLESTRIP)&&vertices>0&&vertices<=8192&&primitives>0&&primitives<=8192&&first>=0&&sailingUiDraws.size()<256;
 ok=ok&&SUCCEEDED(device->GetStreamSource(0,&vb,&offset,&stride))&&vb&&stride>0&&stride<=128&&SUCCEEDED(vb->GetDesc(&vertexDesc));
 ok=ok&&SUCCEEDED(device->GetIndices(&ib))&&ib&&SUCCEEDED(ib->GetDesc(&desc))&&(desc.Format==D3DFMT_INDEX16||desc.Format==D3DFMT_INDEX32);
 const unsigned indexBytes=desc.Format==D3DFMT_INDEX16?2:4;
 const unsigned long long vertexStart=static_cast<unsigned long long>(offset)+first*static_cast<unsigned long long>(stride);
 ok=ok&&vertexStart+static_cast<unsigned long long>(vertices)*stride<=vertexDesc.Size&&(static_cast<unsigned long long>(start)+indexCount)*indexBytes<=desc.Size;
 unsigned stage=1;HRESULT last=S_OK;
 if(ok){stage=2;last=vb->Lock(static_cast<unsigned>(vertexStart),vertices*stride,&vertexData,D3DLOCK_READONLY);ok=SUCCEEDED(last);}
 if(ok){draw.data.resize(vertices*stride);std::memcpy(draw.data.data(),vertexData,draw.data.size());vb->Unlock();
  stage=3;last=ib->Lock(start*indexBytes,indexCount*indexBytes,&indexData,D3DLOCK_READONLY);ok=SUCCEEDED(last);
  if(ok){draw.indices.resize(indexCount);
   for(unsigned i=0;i<draw.indices.size();++i){const unsigned value=indexBytes==2?static_cast<unsigned short*>(indexData)[i]:static_cast<unsigned*>(indexData)[i];
    if(value<minimum||value-minimum>=vertices){stage=4;ok=false;break;}draw.indices[i]=value-minimum;
   }ib->Unlock();}
 }
 if(ok){stage=5;sailingUiDescribeState(device);last=device->CreateStateBlock(D3DSBT_ALL,&draw.state);ok=SUCCEEDED(last)&&sailingUiCamera(device,draw);}
 if(ok){device->GetDepthStencilSurface(&draw.depth);draw.label=sailingLabelPass;draw.effect=sailingEffectPass;draw.type=type;draw.vertices=vertices;draw.primitives=primitives;draw.stride=stride;sailingUiDescribe(draw);sailingUiDraws.push_back(std::move(draw));}
 else{std::fprintf(journal,"SAILING overlay rejected stage=%u result=%lx type=%u base=%d min=%u vertices=%u start=%u primitives=%u offset=%u stride=%u vb=%u ib=%u format=%u\n",stage,static_cast<unsigned long>(last),static_cast<unsigned>(type),base,minimum,vertices,start,primitives,offset,stride,vertexDesc.Size,desc.Size,static_cast<unsigned>(desc.Format));if(draw.state)draw.state->Release();if(draw.depth)draw.depth->Release();sailingUiFail();}
 if(vb)vb->Release();
 if(ib)ib->Release();
 return false;
}
static bool sailingUiPrimitive(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,unsigned first,unsigned primitives){
 if(!sailingUiDisplay(device))return false;
 if(sailingExtra)return true;
 if(sailingUiFailed)return false;
 SailingUiDraw draw;IDirect3DVertexBuffer9* vb=nullptr;UINT offset=0,stride=0;D3DVERTEXBUFFER_DESC desc{};void* data=nullptr;
 const unsigned count=type==D3DPT_TRIANGLELIST?3*primitives:primitives+2;
 bool ok=(type==D3DPT_TRIANGLELIST||type==D3DPT_TRIANGLESTRIP)&&primitives>0&&primitives<=8192&&count<=24576&&sailingUiDraws.size()<256;
 ok=ok&&SUCCEEDED(device->GetStreamSource(0,&vb,&offset,&stride))&&vb&&stride>0&&stride<=128&&SUCCEEDED(vb->GetDesc(&desc));
 const unsigned long long begin=static_cast<unsigned long long>(offset)+static_cast<unsigned long long>(first)*stride;
 ok=ok&&begin+static_cast<unsigned long long>(count)*stride<=desc.Size;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(ok&&sailingTicks==12){
  IDirect3DVertexShader9* shader=nullptr;device->GetVertexShader(&shader);
  std::fprintf(journal,"OVERLAY buffer=%p pool=%u usage=%lx first=%u offset=%u stride=%u shader=%p\n",vb,static_cast<unsigned>(desc.Pool),static_cast<unsigned long>(desc.Usage),first,offset,stride,shader);std::fflush(journal);if(shader)shader->Release();
 }
#endif
 if(ok){
  ok=sailingObserveBuffer(vb,desc.Size);auto copy=sailingFindBuffer(vb);
  if(ok&&copy&&(begin<copy->validBegin||begin+count*stride>copy->validEnd)){
   sailingUiPending=true;vb->Release();return false;
  }
  if(ok)data=copy->bytes.data()+begin;
 }
 if(ok){draw.data.resize(count*stride);std::memcpy(draw.data.data(),data,draw.data.size());
  draw.indices.resize(count);for(unsigned i=0;i<count;++i)draw.indices[i]=i;
  ok=SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&draw.state))&&sailingUiCamera(device,draw);
 }
 if(vb)vb->Release();
 if(ok){device->GetDepthStencilSurface(&draw.depth);draw.label=sailingLabelPass;draw.effect=sailingEffectPass;draw.type=type;draw.vertices=count;draw.primitives=primitives;draw.stride=stride;sailingUiDescribe(draw);sailingUiDraws.push_back(std::move(draw));}
 else{std::fprintf(journal,"SAILING primitive capture rejected effect=%d label=%d type=%u primitives=%u first=%u stride=%u bufferBytes=%u retainedBuffers=%u\n",sailingEffectPass,sailingLabelPass,static_cast<unsigned>(type),primitives,first,stride,desc.Size,unsigned(sailingBuffers.size()));if(draw.state)draw.state->Release();sailingUiFail();}
 return false;
}
static bool sailingUiOther(IDirect3DDevice9* device){
 if(!sailingUiDisplay(device))return false;
 if(sailingExtra)return true;
 std::fprintf(journal,"SAILING overlay uses unsupported non-indexed draw\n");
 sailingUiFail();return false;
}
static bool sailingUiReplay(IDirect3DDevice9* device,bool labels=false,bool insideScene=false,bool effects=false){
 if(sailingUiFailed||sailingUiDraws.empty())return !sailingUiFailed;
 IDirect3DStateBlock9* restore=nullptr;IDirect3DSurface9* target=nullptr,*depth=nullptr,*buffer=nullptr;
 SailingUiDraw camera;
 bool ok=sailingUiCamera(device,camera)&&SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&restore))&&SUCCEEDED(device->GetRenderTarget(0,&target))&&SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&buffer));
 device->GetDepthStencilSurface(&depth);
 bool began=false;sailingUiReplaying=true;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 char probePath[MAX_PATH];std::snprintf(probePath,sizeof(probePath),"%stiming-test.ini",traceFolder);
 const bool ignoreHudDepth=!labels&&GetPrivateProfileIntA("Diagnostic","HudIgnoreDepth",0,probePath)!=0;
#endif
 if(ok){ok=SUCCEEDED(device->SetRenderTarget(0,buffer));if(ok&&!insideScene){ok=SUCCEEDED(device->BeginScene());began=ok;}}
 if(ok)for(const auto& draw:sailingUiDraws){
  if(draw.label!=labels||draw.effect!=effects)continue;
  ok=SUCCEEDED(device->SetDepthStencilSurface(draw.depth))&&SUCCEEDED(draw.state->Apply())&&sailingUiApplyCamera(device,draw);
#ifdef PIRATES_TIMING_DIAGNOSTIC
  // A/B probe only: establish whether a later world pass occludes HUD replay.
  // Never change native draws or world-label depth, and restore all state below.
  if(ok&&ignoreHudDepth)ok=SUCCEEDED(device->SetRenderState(D3DRS_ZENABLE,FALSE));
#endif
  if(ok)ok=SUCCEEDED(originalDrawIndexedUP(device,draw.type,0,draw.vertices,draw.primitives,draw.indices.data(),D3DFMT_INDEX32,draw.data.data(),draw.stride));
  if(!ok)break;
 }
 if(began)ok=SUCCEEDED(device->EndScene())&&ok;
 if(target)ok=SUCCEEDED(device->SetRenderTarget(0,target))&&ok;
 ok=SUCCEEDED(device->SetDepthStencilSurface(depth))&&ok;
 if(restore)ok=SUCCEEDED(restore->Apply())&&ok;
 if(restore)ok=sailingUiApplyCamera(device,camera)&&ok;
 if(restore)restore->Release();
 if(target)target->Release();
 if(depth)depth->Release();
 if(buffer)buffer->Release();
 sailingUiReplaying=false;if(ok)++sailingUiReplays;else sailingUiFail();return ok;
}
