// Observation only. No timing, input, interpolation or extra-frame operations.
#ifdef PIRATES_SAILING_OBSERVATION
#include <array>
struct ObsCache {uintptr_t actor=0,root=0;int owner=-1,previous=-1,model=-1;};
static bool obsReadable(const void* p,size_t n){MEMORY_BASIC_INFORMATION m{};auto a=reinterpret_cast<uintptr_t>(p);return a&&a+n>=a&&VirtualQuery(p,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&a+n<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;}
static ObsCache obsCache(int i){ObsCache x;x.actor=*reinterpret_cast<uintptr_t*>(0x8b9870+4*i);x.owner=*reinterpret_cast<int*>(0x8b97a8+4*i);x.previous=*reinterpret_cast<int*>(0x8b9730+4*i);x.model=*reinterpret_cast<int*>(0x8b9820+4*i);if(obsReadable(reinterpret_cast<void*>(x.actor),0x20))x.root=*reinterpret_cast<uintptr_t*>(x.actor+0x1c);return x;}
static unsigned obsEpoch=1,obsGeneration[20]{},obsCacheCalls=0,obsClears=0;
static ObsCache obsKnown[20];
struct ObsEvent {unsigned frame,epoch,slot,generation;ObsCache before,after;};
static std::vector<ObsEvent> obsEvents;
static std::vector<ObsEvent> obsEventBatch;
static std::atomic<DWORD> obsThread{0};static std::atomic<bool> obsThreadFailed{false};static unsigned obsTreeFailures=0;
struct ObsMetadata {DWORD thread;bool threadFailed;unsigned treeFailures,cacheCalls,clears,epoch,lifeDropped;};
static ObsMetadata obsMetadata{};
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
static bool obsStrictCacheChange(int,const ObsCache&,const ObsCache&);
#else
static bool obsStrictCacheChange(int,const ObsCache&,const ObsCache&){return true;}
#endif
static bool obsSameThread(){DWORD t=GetCurrentThreadId(),unset=0;obsThread.compare_exchange_strong(unset,t);if(t!=obsThread.load()){obsThreadFailed=true;return false;}return !obsThreadFailed.load();}
static void obsScan(){for(int i=0;i<20;++i){const auto x=obsCache(i);auto& p=obsKnown[i];if(x.actor!=p.actor||x.root!=p.root||x.owner!=p.owner||x.model!=p.model){
 // Fingerprint generation is diagnostic only. Owner flips do not establish lifetime.
 if(x.actor!=p.actor||x.root!=p.root||x.model!=p.model)++obsGeneration[i];
 if(obsStrictCacheChange(i,p,x)&&obsEvents.size()<2048){obsEvents.push_back({frames.load(),obsEpoch,unsigned(i),obsGeneration[i],p,x});}
 p=x;}else p.previous=x.previous;}}
extern "C" void __attribute__((cdecl)) obsCacheBefore(){if(obsSameThread())obsScan();}
extern "C" void __attribute__((cdecl)) obsCacheAfter(){if(obsSameThread()){++obsCacheCalls;obsScan();}}
extern "C" {static void* obsCacheTrampoline=nullptr;}
// Verified native ABI: ship type in EAX, owner/create flag on stack, RET 0.
// Duplicate both arguments for the native call; preserve its result and flags.
extern "C" void __attribute__((naked)) obsCacheEntry(){__asm__ __volatile__("pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tcall _obsCacheBefore\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tpush 8(%esp)\n\tpush 8(%esp)\n\tcall *_obsCacheTrampoline\n\tleal 8(%esp),%esp\n\tpushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tcall _obsCacheAfter\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tret");}
// Do not detour 45ac10: its loop branches into the entry patch span.
// Epoch is deliberately uncertified until a safe lifecycle observer exists.
struct ObsNode {uintptr_t node,parent;int slot;char name[64];};
static std::vector<ObsNode> obsNodes;
static bool obsTree(uintptr_t at,uintptr_t parent,int slot,int depth){
 if(!at)return true;
 if(depth>24||obsNodes.size()>=2500||!obsReadable(reinterpret_cast<void*>(at),0xc4))return false;
 for(const auto& n:obsNodes)if(n.node==at)return true;
 auto v=*reinterpret_cast<uintptr_t*>(at);if(v!=0x6c0bd8&&v!=0x6c1a00&&v!=0x6c2478&&v!=0x6c1c88)return false;
 ObsNode node{};node.node=at;node.parent=parent;node.slot=slot;const auto name=*reinterpret_cast<const char**>(at+0xc);
 if(obsReadable(name,64))for(unsigned i=0;i<63&&name[i];++i)node.name[i]=name[i];
 obsNodes.push_back(node);
 if(v==0x6c0bd8||v==0x6c1c88){auto children=*reinterpret_cast<uintptr_t**>(at+0xb8);auto count=*reinterpret_cast<unsigned*>(at+0xc0);if(count>128||(count&&!obsReadable(children,count*4)))return false;for(unsigned i=0;i<count;++i)if(!obsTree(children[i],at,slot,depth+1))return false;}
 return true;
}
static bool obsRecording();
struct ObsDetail {
 uintptr_t renderer=0,palette=0;int apply=-1;unsigned copied=0,converted=0,paletteValid=0,submitted=0;
 float stackTransform[13]{},stackBound[4]{},converterInput[13]{},converterOutput[16]{},cached[16]{},rootTransform[13]{};
 unsigned short indices[4]{};D3DMATRIX selected[4]{},payload[4]{};
};
struct ObsContext {uintptr_t geometry=0;bool alternate=false;unsigned route=0;uintptr_t source=0,skin=0,partition=0;float transform[13]{};ObsDetail detail;};
static thread_local ObsContext obsContext;
using ObsGeometry=void(__attribute__((thiscall))*)(void*,void*);static ObsGeometry obsOriginalGeometry,obsOriginalAlternate;
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
static void strictDispatchTrace(void*,void*,unsigned,const uintptr_t*);
#endif
static void __attribute__((fastcall,noinline)) obsGeometry(void* renderer,void*,void* geometry){
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 strictDispatchTrace(geometry,renderer,1,reinterpret_cast<const uintptr_t*>(__builtin_frame_address(0))+1);
#endif
 auto saved=obsContext;obsContext={};obsContext.geometry=reinterpret_cast<uintptr_t>(geometry);obsContext.alternate=saved.alternate;obsOriginalGeometry(renderer,geometry);obsContext=saved;}
static void __attribute__((fastcall)) obsAlternate(void* geometry,void*,void* camera){auto saved=obsContext;obsContext={};obsContext.geometry=reinterpret_cast<uintptr_t>(geometry);obsContext.alternate=true;obsOriginalAlternate(geometry,camera);obsContext=saved;}
using ObsSingle=void(__attribute__((thiscall))*)(void*,const void*,int);static ObsSingle obsOriginalSingle;
static void __attribute__((fastcall)) obsSingle(void* renderer,void*,const void* transform,int apply){obsContext.route=1;obsContext.detail.apply=apply;obsContext.detail.renderer=reinterpret_cast<uintptr_t>(renderer);obsContext.source=reinterpret_cast<uintptr_t>(transform);if(obsReadable(transform,52))std::memcpy(obsContext.transform,transform,52);obsOriginalSingle(renderer,transform,apply);}
using ObsIndexed=void(__attribute__((thiscall))*)(void*,const void*,void*,void*);static ObsIndexed obsOriginalIndexed;
static void __attribute__((fastcall)) obsIndexed(void* renderer,void*,const void* skin,void* part,void* transform){obsContext.route=2;obsContext.detail.apply=-1;obsContext.detail.renderer=reinterpret_cast<uintptr_t>(renderer);obsContext.skin=reinterpret_cast<uintptr_t>(skin);obsContext.partition=reinterpret_cast<uintptr_t>(part);obsContext.source=reinterpret_cast<uintptr_t>(transform);if(obsReadable(transform,52))std::memcpy(obsContext.transform,transform,52);if(obsRecording()&&obsReadable(skin,0x24)&&obsReadable(part,8)){
 obsContext.detail.palette=*reinterpret_cast<const uintptr_t*>(static_cast<const char*>(skin)+0x20);
 const auto map=*reinterpret_cast<const unsigned short* const*>(static_cast<const char*>(part)+4);
 if(obsReadable(map,8)){std::memcpy(obsContext.detail.indices,map,8);for(unsigned i=0;i<4;++i){auto at=obsContext.detail.palette+64u*map[i];if(obsReadable(reinterpret_cast<void*>(at),64)){std::memcpy(&obsContext.detail.selected[i],reinterpret_cast<void*>(at),64);obsContext.detail.paletteValid|=1u<<i;}}}
 }obsOriginalIndexed(renderer,skin,part,transform);}
struct ObsDraw {
 unsigned frame,epoch,serial,order,route,generation;int slot,owner,previous,model;uintptr_t geometry,parent,actor,root,vtable,source,skin;
 bool alternate;HRESULT result;unsigned validTransforms;char name[64];float engine[13],bound[4],input[13];D3DMATRIX matrices[6];
 uintptr_t target,depth,vs,ps,declaration,textures[4],targetTexture,indexedSkin,partition;LONGLONG captureCost;D3DSURFACE_DESC targetDesc;D3DVIEWPORT9 viewport;DWORD fvf,blend,indexedBlend;unsigned primitive;ObsDetail detail;
};
struct ObsUpload {unsigned frame,serial,start,count,route;uintptr_t geometry;std::array<float,1024> values;uintptr_t shader=0,skin=0,partition=0,target=0;unsigned order=0;};
static std::vector<ObsDraw> obsDraws;static std::vector<ObsUpload> obsUploads;
static std::atomic<bool> obsWriting{false};static bool obsActive=false,obsEnabled=false;static unsigned obsSerial=0,obsCaptureFrames=0,obsOrder=0,obsTotalRows=0;
static bool obsRecording(){return obsActive&&obsSameThread();}
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
static void obsFloats(FILE*,const float*,unsigned);
#include "strict-rigid-observation.h"
#ifdef PIRATES_OWNED_RENDER_PROOF
#include "owned-render-proof.h"
#endif
static void obsDeviceReset(){
#ifdef PIRATES_OWNED_RENDER_PROOF
 ++ownedResetCount;
#endif
 if(obsSameThread())strictInvalidate(rigid::InvalidReason::DeviceReset);}
#else
static void obsDeviceReset(){}
#endif
using ObsConvert=void(__attribute__((cdecl))*)(void*,const void*);static ObsConvert obsOriginalConvert;
static void __attribute__((cdecl)) obsConvert(void* output,const void* input){
#ifdef PIRATES_OWNED_RENDER_PROOF
 if(ownedProofActive)ownedProofActive->converterPointer=reinterpret_cast<uintptr_t>(input);
#endif
 const bool collect=obsRecording()&&obsContext.geometry&&obsReadable(input,52);
 if(collect)std::memcpy(obsContext.detail.converterInput,input,52);
 obsOriginalConvert(output,input);
 if(collect&&obsReadable(output,64)){std::memcpy(obsContext.detail.converterOutput,output,64);obsContext.detail.converted=1;}
}
static void obsCopied(const void* transform,const void* bound){if(obsRecording()&&obsContext.geometry&&obsReadable(transform,52)&&obsReadable(bound,16)){std::memcpy(obsContext.detail.stackTransform,transform,52);std::memcpy(obsContext.detail.stackBound,bound,16);obsContext.detail.copied=1;}}
using ObsLower6=void(__attribute__((thiscall))*)(void*,void*,void*,void*,void*,void*,void*);static ObsLower6 obsOriginalLower6;
static void __attribute__((fastcall,noinline)) obsLower6(void* renderer,void*,void* geometry,void* data,void* skin,void* transform,void* bound,void* stream){
#ifdef PIRATES_OWNED_RENDER_PROOF
 auto* proof=ownedProofPrepare(renderer,geometry,data,skin,transform,bound,reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
 FixtureWriteGuard guard;
 if(proof&&ownedProofGuardBegin(*proof,guard)){
  proof->substituted=proof->mode==2;
  auto* input=proof->substituted?static_cast<void*>(proof->transform):transform;
  auto* sphere=proof->substituted?static_cast<void*>(proof->bound):bound;
  obsCopied(input,sphere);obsOriginalLower6(renderer,geometry,data,skin,input,sphere,stream);
  ownedProofFinish(*proof,guard);return;
 }
#endif
 obsCopied(transform,bound);obsOriginalLower6(renderer,geometry,data,skin,transform,bound,stream);
}
using ObsLower5=void(__attribute__((thiscall))*)(void*,void*,void*,void*,void*,void*);static ObsLower5 obsOriginalLower5;
static void __attribute__((fastcall)) obsLower5(void* renderer,void*,void* geometry,void* data,void* skin,void* transform,void* bound){obsCopied(transform,bound);obsOriginalLower5(renderer,geometry,data,skin,transform,bound);}
static void obsTransformPayload(IDirect3DDevice9*d,D3DTRANSFORMSTATETYPE type,const D3DMATRIX*m){
 if(!m||!obsRecording()||d!=targetDevice||!obsContext.geometry||unsigned(type)<256||unsigned(type)>259)return;
 unsigned i=unsigned(type)-256;obsContext.detail.payload[i]=*m;obsContext.detail.submitted|=1u<<i;
 auto cached=obsContext.detail.renderer+0x780;if(obsReadable(reinterpret_cast<void*>(cached),64))std::memcpy(obsContext.detail.cached,reinterpret_cast<void*>(cached),64);
}
struct ObsLife {unsigned frame,epoch;uintptr_t pointer;unsigned kind;uintptr_t roots[5];};
static std::vector<ObsLife> obsLife,obsLifeBatch;static unsigned obsLifeDropped=0;
using ObsDestroy=void(__attribute__((thiscall))*)(void*);static ObsDestroy obsOriginalDestroy;
static void __attribute__((fastcall)) obsDestroy(void* object,void*){if(obsSameThread()){
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 if(strictTargeted&&!strictWatches(reinterpret_cast<uintptr_t>(object))){++strictIgnoredDestruction;obsOriginalDestroy(object);return;}
 if(strictTargeted)strictInvalidate(rigid::InvalidReason::Destruction);
#endif
 ++obsEpoch;if(obsLife.size()<8192){ObsLife e{};e.frame=frames.load();e.epoch=obsEpoch;e.pointer=reinterpret_cast<uintptr_t>(object);e.kind=1;obsLife.push_back(e);}else ++obsLifeDropped;
 }obsOriginalDestroy(object);}
static void obsScene(){static uintptr_t previous[5]{};uintptr_t now[5]{};for(unsigned i=0;i<5;++i)now[i]=*reinterpret_cast<uintptr_t*>(0x8e9fec+4*i);if(std::memcmp(previous,now,sizeof(now))){++obsEpoch;if(obsLife.size()<8192){ObsLife e{};e.frame=frames.load();e.epoch=obsEpoch;e.kind=2;std::memcpy(e.roots,now,sizeof(now));obsLife.push_back(e);}else ++obsLifeDropped;std::memcpy(previous,now,sizeof(now));}}
struct ObsShader {uintptr_t pointer;std::vector<unsigned char> code;};static std::vector<ObsShader> obsShaders;
static void obsShaderCode(IDirect3DVertexShader9* shader){if(!shader)return;for(const auto&s:obsShaders)if(s.pointer==reinterpret_cast<uintptr_t>(shader))return;if(obsShaders.size()>=32)return;UINT bytes=0;if(FAILED(shader->GetFunction(nullptr,&bytes))||bytes>16384||!bytes)return;ObsShader s{};s.pointer=reinterpret_cast<uintptr_t>(shader);s.code.resize(bytes);if(SUCCEEDED(shader->GetFunction(s.code.data(),&bytes)))obsShaders.push_back(std::move(s));}
static int obsDraw(IDirect3DDevice9*d,unsigned primitive){
#ifdef PIRATES_OWNED_RENDER_PROOF
 ownedProofAtDraw();
#endif
 if(!obsSameThread()||!obsActive||d!=targetDevice||obsDraws.size()>=4096||obsTotalRows>=40000)return -1;
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 if(strictTargeted&&obsContext.geometry!=strictChain.geometry)return -1;
 strictCaptureValues(2);
#endif
 LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
 ObsDraw r{};r.detail=obsContext.detail;r.indexedSkin=obsContext.skin;r.partition=obsContext.partition;r.frame=frames.load();r.epoch=obsEpoch;r.serial=obsSerial;r.order=obsOrder++;r.route=obsContext.route;r.geometry=obsContext.geometry;r.alternate=obsContext.alternate;r.source=obsContext.source;r.primitive=primitive;r.slot=-1;r.owner=-1;r.previous=-1;r.model=-1;std::memcpy(r.input,obsContext.transform,52);
 for(const auto& n:obsNodes)if(n.node==r.geometry){r.slot=n.slot;r.parent=n.parent;std::memcpy(r.name,n.name,64);break;}
 if(r.slot>=0){auto x=obsCache(r.slot);r.actor=x.actor;r.root=x.root;r.owner=x.owner;r.previous=x.previous;r.model=x.model;r.generation=obsGeneration[r.slot];if(obsReadable(reinterpret_cast<void*>(r.root+0x6c),52))std::memcpy(r.detail.rootTransform,reinterpret_cast<void*>(r.root+0x6c),52);}
 if(obsReadable(reinterpret_cast<void*>(r.geometry),0xc4)){r.vtable=*reinterpret_cast<uintptr_t*>(r.geometry);std::memcpy(r.engine,reinterpret_cast<void*>(r.geometry+0x6c),52);std::memcpy(r.bound,reinterpret_cast<void*>(r.geometry+0x28),16);r.skin=*reinterpret_cast<uintptr_t*>(r.geometry+0xc0);if(!r.name[0]){auto name=*reinterpret_cast<const char**>(r.geometry+0xc);if(obsReadable(name,64))for(unsigned i=0;i<63&&name[i];++i)r.name[i]=name[i];}}
 const D3DTRANSFORMSTATETYPE types[]={D3DTS_WORLD,D3DTRANSFORMSTATETYPE(257),D3DTRANSFORMSTATETYPE(258),D3DTRANSFORMSTATETYPE(259),D3DTS_VIEW,D3DTS_PROJECTION};for(unsigned i=0;i<6;++i)if(SUCCEEDED(d->GetTransform(types[i],&r.matrices[i])))r.validTransforms|=1u<<i;
 IDirect3DSurface9* rt=nullptr;IDirect3DSurface9* ds=nullptr;d->GetRenderTarget(0,&rt);d->GetDepthStencilSurface(&ds);r.target=reinterpret_cast<uintptr_t>(rt);r.depth=reinterpret_cast<uintptr_t>(ds);if(rt){IDirect3DTexture9* texture=nullptr;rt->GetContainer(IID_IDirect3DTexture9,reinterpret_cast<void**>(&texture));r.targetTexture=reinterpret_cast<uintptr_t>(texture);if(texture)texture->Release();rt->GetDesc(&r.targetDesc);rt->Release();}if(ds)ds->Release();d->GetViewport(&r.viewport);d->GetFVF(&r.fvf);d->GetRenderState(D3DRS_VERTEXBLEND,&r.blend);d->GetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,&r.indexedBlend);
 IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;d->GetVertexShader(&vs);d->GetPixelShader(&ps);d->GetVertexDeclaration(&decl);r.vs=reinterpret_cast<uintptr_t>(vs);r.ps=reinterpret_cast<uintptr_t>(ps);r.declaration=reinterpret_cast<uintptr_t>(decl);if(vs){obsShaderCode(vs);vs->Release();}if(ps)ps->Release();if(decl)decl->Release();
 for(unsigned i=0;i<4;++i){IDirect3DBaseTexture9* t=nullptr;d->GetTexture(i,&t);r.textures[i]=reinterpret_cast<uintptr_t>(t);if(t)t->Release();}
 QueryPerformanceCounter(&end);r.captureCost=end.QuadPart-begin.QuadPart;
 obsDraws.push_back(r);++obsTotalRows;return int(obsDraws.size()-1);
}
static HRESULT obsResult(int row,HRESULT hr){if(row>=0)obsDraws[size_t(row)].result=hr;return hr;}
using ObsConstants=HRESULT(WINAPI*)(IDirect3DDevice9*,UINT,const float*,UINT);static ObsConstants obsOriginalConstants;
static HRESULT WINAPI obsConstants(IDirect3DDevice9*d,UINT start,const float* values,UINT count){if(obsSameThread()&&obsActive&&d==targetDevice&&values&&count&&count<=256&&obsUploads.size()<2048){ObsUpload u{};u.frame=frames.load();u.serial=obsSerial;u.start=start;u.count=count;u.route=obsContext.route;u.geometry=obsContext.geometry;std::memcpy(u.values.data(),values,count*16);u.skin=obsContext.skin;u.partition=obsContext.partition;u.order=obsOrder;
 IDirect3DVertexShader9* shader=nullptr;d->GetVertexShader(&shader);u.shader=reinterpret_cast<uintptr_t>(shader);if(shader){obsShaderCode(shader);shader->Release();}IDirect3DSurface9* target=nullptr;d->GetRenderTarget(0,&target);u.target=reinterpret_cast<uintptr_t>(target);if(target)target->Release();obsUploads.push_back(u);}return obsOriginalConstants(d,start,values,count);}
static void obsFloats(FILE*f,const float*p,unsigned n){for(unsigned i=0;i<n;++i)std::fprintf(f,",%.9g",p[i]);}
static DWORD WINAPI obsWrite(void*){
#ifdef PIRATES_OWNED_RENDER_PROOF
 ownedProofWrite();
#endif
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 strictWrite();
#endif
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%sobservation-%u.csv",traceFolder,obsSerial);FILE*f=std::fopen(path,"w");
 if(f){std::fprintf(f,"serial,frame,epoch,order,geometry,parent,slot,generation,owner,previous,model,actor,root,vtable,alternate,route,source,skin,name,target,depth,target_width,target_height,viewport_x,viewport_y,viewport_width,viewport_height,fvf,vertex_blend,indexed_blend,vs,ps,declaration,texture0,texture1,texture2,texture3,primitives,result,valid_transforms,target_texture,indexed_skin,partition,capture_qpc");for(const char* label:{"engine","input"})for(int i=0;i<13;++i)std::fprintf(f,",%s%d",label,i);for(int i=0;i<4;++i)std::fprintf(f,",bound%d",i);for(const char* label:{"world0_","world1_","world2_","world3_","view","projection"})for(int i=0;i<16;++i)std::fprintf(f,",%s%d",label,i);std::fputc('\n',f);
 for(const auto&r:obsDraws){std::fprintf(f,"%u,%u,%u,%u,%p,%p,%d,%u,%d,%d,%d,%p,%p,%p,%d,%u,%p,%p,",r.serial,r.frame,r.epoch,r.order,reinterpret_cast<void*>(r.geometry),reinterpret_cast<void*>(r.parent),r.slot,r.generation,r.owner,r.previous,r.model,reinterpret_cast<void*>(r.actor),reinterpret_cast<void*>(r.root),reinterpret_cast<void*>(r.vtable),r.alternate,r.route,reinterpret_cast<void*>(r.source),reinterpret_cast<void*>(r.skin));for(char c:r.name){if(!c)break;std::fputc(c==','?'_':c,f);}std::fprintf(f,",%p,%p,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%p,%p,%p",reinterpret_cast<void*>(r.target),reinterpret_cast<void*>(r.depth),r.targetDesc.Width,r.targetDesc.Height,unsigned(r.viewport.X),unsigned(r.viewport.Y),unsigned(r.viewport.Width),unsigned(r.viewport.Height),r.fvf,r.blend,r.indexedBlend,reinterpret_cast<void*>(r.vs),reinterpret_cast<void*>(r.ps),reinterpret_cast<void*>(r.declaration));for(auto t:r.textures)std::fprintf(f,",%p",reinterpret_cast<void*>(t));std::fprintf(f,",%u,%ld,%u,%p,%p,%p,%lld",r.primitive,long(r.result),r.validTransforms,reinterpret_cast<void*>(r.targetTexture),reinterpret_cast<void*>(r.indexedSkin),reinterpret_cast<void*>(r.partition),r.captureCost);obsFloats(f,r.engine,13);obsFloats(f,r.input,13);obsFloats(f,r.bound,4);for(const auto&m:r.matrices)obsFloats(f,reinterpret_cast<const float*>(&m),16);std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%suploads-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"frame,serial,geometry,route,start,count,shader,skin,partition,target,draw_order");for(int i=0;i<1024;++i)std::fprintf(f,",v%d",i);std::fputc('\n',f);for(const auto&u:obsUploads){std::fprintf(f,"%u,%u,%p,%u,%u,%u",u.frame,u.serial,reinterpret_cast<void*>(u.geometry),u.route,u.start,u.count);std::fprintf(f,",%p,%p,%p,%p,%u",reinterpret_cast<void*>(u.shader),reinterpret_cast<void*>(u.skin),reinterpret_cast<void*>(u.partition),reinterpret_cast<void*>(u.target),u.order);obsFloats(f,u.values.data(),1024);std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%slifetime-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"frame,epoch,slot,generation,before_actor,before_root,before_owner,before_model,after_actor,after_root,after_owner,after_previous,after_model\n");for(const auto&e:obsEventBatch)std::fprintf(f,"%u,%u,%u,%u,%p,%p,%d,%d,%p,%p,%d,%d,%d\n",e.frame,e.epoch,e.slot,e.generation,reinterpret_cast<void*>(e.before.actor),reinterpret_cast<void*>(e.before.root),e.before.owner,e.before.model,reinterpret_cast<void*>(e.after.actor),reinterpret_cast<void*>(e.after.root),e.after.owner,e.after.previous,e.after.model);std::fclose(f);}
 std::snprintf(path,sizeof(path),"%smetadata-%u.txt",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"thread=%lu\nthread_failed=%d\ntree_failures=%u\nallocator_calls=%u\nclear_calls=%u\nepoch=%u\ndraws=%u\n",obsMetadata.thread,int(obsMetadata.threadFailed),obsMetadata.treeFailures,obsMetadata.cacheCalls,obsMetadata.clears,obsMetadata.epoch,unsigned(obsDraws.size()));LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);std::fprintf(f,"qpc_frequency=%lld\nlifetime_events_dropped=%u\n",frequency.QuadPart,obsMetadata.lifeDropped);std::fclose(f);}
 std::snprintf(path,sizeof(path),"%snodes-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"node,parent,slot,name\n");for(const auto&n:obsNodes)std::fprintf(f,"%p,%p,%d,%s\n",reinterpret_cast<void*>(n.node),reinterpret_cast<void*>(n.parent),n.slot,n.name);std::fclose(f);}

 std::snprintf(path,sizeof(path),"%sdetail-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){
 std::fprintf(f,"frame,order,geometry,renderer,apply,copied,converted,palette,palette_valid,submitted");for(int i=0;i<4;++i)std::fprintf(f,",index%d",i);
 for(const auto label:{"stack","converter_input","root"})for(unsigned i=0;i<13;++i)std::fprintf(f,",%s_%u",label,i);
 for(unsigned i=0;i<4;++i)std::fprintf(f,",stack_bound_%u",i);
 for(const auto label:{"converter_output","cached","selected0","selected1","selected2","selected3","payload0","payload1","payload2","payload3"})for(unsigned i=0;i<16;++i)std::fprintf(f,",%s_%u",label,i);
 std::fputc('\n',f);
 for(const auto&r:obsDraws){const auto&q=r.detail;std::fprintf(f,"%u,%u,%p,%p,%d,%u,%u,%p,%u,%u",r.frame,r.order,reinterpret_cast<void*>(r.geometry),reinterpret_cast<void*>(q.renderer),q.apply,q.copied,q.converted,reinterpret_cast<void*>(q.palette),q.paletteValid,q.submitted);for(auto i:q.indices)std::fprintf(f,",%u",unsigned(i));for(const auto*p:{q.stackTransform,q.converterInput,q.rootTransform})obsFloats(f,p,13);obsFloats(f,q.stackBound,4);obsFloats(f,q.converterOutput,16);obsFloats(f,q.cached,16);for(const auto&m:q.selected)obsFloats(f,reinterpret_cast<const float*>(&m),16);for(const auto&m:q.payload)obsFloats(f,reinterpret_cast<const float*>(&m),16);std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sdestruction-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"frame,epoch,pointer,kind,root0,root1,root2,root3,root4\n");for(const auto&e:obsLifeBatch){std::fprintf(f,"%u,%u,%p,%u",e.frame,e.epoch,reinterpret_cast<void*>(e.pointer),e.kind);for(auto p:e.roots)std::fprintf(f,",%p",reinterpret_cast<void*>(p));std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sshaders-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"shader,bytecode_hex\n");for(const auto&s:obsShaders){std::fprintf(f,"%p,",reinterpret_cast<void*>(s.pointer));for(auto b:s.code)std::fprintf(f,"%02x",unsigned(b));std::fputc('\n',f);}std::fclose(f);}
 obsWriting=false;return 0;
}
static void obsPresent(){
 if(!obsEnabled||!obsSameThread())return;
 obsScene();
 if(obsActive&&++obsCaptureFrames>=2){obsActive=false;
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 strictFreeze();
#endif
 obsEventBatch.clear();obsEventBatch.swap(obsEvents);obsLifeBatch.clear();obsLifeBatch.swap(obsLife);obsMetadata={obsThread.load(),obsThreadFailed.load(),obsTreeFailures,obsCacheCalls,obsClears,obsEpoch,obsLifeDropped};obsWriting=true;HANDLE t=CreateThread(nullptr,0,obsWrite,nullptr,0,nullptr);if(t)CloseHandle(t);else obsWriting=false;}
 if(obsActive||obsWriting||frames.load()%30||obsTotalRows>=40000)return;
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%sobservation.ini",traceFolder);auto serial=GetPrivateProfileIntA("Observation","Capture",0,path);
 if(serial<=0||unsigned(serial)==obsSerial)return;
 obsSerial=unsigned(serial);obsScan();obsNodes.clear();obsTreeFailures=0;for(int i=0;i<20;++i){auto c=obsCache(i);if(c.root&&!obsTree(c.root,0,i,0))++obsTreeFailures;}
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 strictTargeted=true;strictSelect(GetPrivateProfileIntA("Observation","TargetSlot",-1,path));strictCullRows.clear();strictDispatchRows.clear();strictShipVisibilityRows.clear();strictCameraRows.clear();
 strictCaptureValues(3);
 if(obsTreeFailures)strictInvalidate(rigid::InvalidReason::BadSample);
#endif
 obsDraws.clear();obsUploads.clear();obsShaders.clear();obsCaptureFrames=0;obsOrder=0;obsActive=true;
}
static bool obsInstall(void** table){
 // Absolute addresses are valid only for the verified non-relocated x86 image.
 const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
 if(base!=0x400000)return false;
 const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x1000)return false;
 const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC||nt->FileHeader.TimeDateStamp!=0x44e1234b)return false;
 struct Hook{uintptr_t address;unsigned char bytes[10];unsigned length;void* replacement;void** original;};
 const Hook hooks[]={
#ifdef PIRATES_OWNED_RENDER_PROOF
 OWNED_CENSUS_HOOKS
#endif
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 {0x471e00,{0x81,0xec,0xc0,0x02,0,0},6,reinterpret_cast<void*>(strictWorldModal),&strictWorldModalTrampoline},
 {0x45c270,{0x83,0xec,0x58,0x33,0xc0},5,reinterpret_cast<void*>(strictShoreEnter),&strictShoreEnterTrampoline},
 {0x45cab0,{0xa1,0x84,0x6a,0x72,0},5,reinterpret_cast<void*>(strictShoreLeave),&strictShoreLeaveTrampoline},
 {0x467f90,{0x83,0xec,0x68,0x53,0x55},5,reinterpret_cast<void*>(strictPlayerUpdate),&strictPlayerUpdateTrampoline},
 {0x4d0ab0,{0x83,0xec,0x20,0x8b,0x0d,0xf8,0x9f,0x8e,0},9,reinterpret_cast<void*>(strictShipPassEntry),&strictShipPassTrampoline},
 {0x5c6c70,{0x83,0xec,0x18,0x53,0x56},5,reinterpret_cast<void*>(strictCameraBuild),reinterpret_cast<void**>(&strictOriginalCameraBuild)},
 {0x4ffa00,{0x57,0x8b,0xf8,0x8d,0x87,0x8c,0x01,0,0},9,reinterpret_cast<void*>(strictShipVisibilityEntry),&strictShipVisibilityTrampoline},
 {0x547460,{0x56,0x57,0x8b,0x7c,0x24,0x0c},6,reinterpret_cast<void*>(strictRigidDispatch),reinterpret_cast<void**>(&strictOriginalRigidDispatch)},
 {0x533e70,{0x56,0x8b,0xf1,0xf6,0x46,0x20,0x01},7,reinterpret_cast<void*>(strictEntry),reinterpret_cast<void**>(&strictOriginalEntry)},
 {0x533df0,{0x53,0x8b,0x5c,0x24,0x08},5,reinterpret_cast<void*>(strictCull),reinterpret_cast<void**>(&strictOriginalCull)},
#endif
 {0x5c10f0,{0x8b,0x44,0x24,0x08,0xd9,0x00},6,reinterpret_cast<void*>(obsConvert),reinterpret_cast<void**>(&obsOriginalConvert)},
 {0x5c4270,{0x53,0x55,0x8b,0x6c,0x24,0x0c},6,reinterpret_cast<void*>(obsLower6),reinterpret_cast<void**>(&obsOriginalLower6)},
 {0x5c4450,{0x8b,0x44,0x24,0x0c,0x83,0xec,0x1c},7,reinterpret_cast<void*>(obsLower5),reinterpret_cast<void**>(&obsOriginalLower5)},
 {0x534040,{0x56,0x8b,0xf1,0x57,0x8d,0xbe,0xa0,0,0,0},10,reinterpret_cast<void*>(obsDestroy),reinterpret_cast<void**>(&obsOriginalDestroy)},
 {0x45aa10,{0x99,0xb9,9,0,0,0,0xf7,0xf9},8,reinterpret_cast<void*>(obsCacheEntry),&obsCacheTrampoline},
 {0x5c5b80,{0x83,0xec,0x48,0x8b,0x44,0x24,0x4c},7,reinterpret_cast<void*>(obsGeometry),reinterpret_cast<void**>(&obsOriginalGeometry)},
 {0x5491e0,{0x56,0x57,0x8b,0x7c,0x24,0x0c},6,reinterpret_cast<void*>(obsAlternate),reinterpret_cast<void**>(&obsOriginalAlternate)},
 {0x5c11d0,{0x83,0xec,0x30,0x53,0x56,0x57},6,reinterpret_cast<void*>(obsSingle),reinterpret_cast<void**>(&obsOriginalSingle)},
 {0x5c1290,{0x53,0x55,0x56,0x8b,0x74,0x24,0x18},7,reinterpret_cast<void*>(obsIndexed),reinterpret_cast<void**>(&obsOriginalIndexed)}};
 for(const auto&h:hooks)if(std::memcmp(reinterpret_cast<void*>(h.address),h.bytes,h.length)){std::fprintf(journal,"OBS signature failed %p; no observation hooks enabled\n",reinterpret_cast<void*>(h.address));return false;}
 obsLife.reserve(8192);obsLifeBatch.reserve(8192);obsShaders.reserve(32);obsDraws.reserve(4096);obsUploads.reserve(2048);obsEvents.reserve(2048);obsEventBatch.reserve(2048);obsNodes.reserve(2500);
#ifdef PIRATES_STRICT_RIGID_OBSERVATION
 strictDispatchRows.reserve(16);strictShipVisibilityRows.reserve(16);strictCameraRows.reserve(16);
 strictModeEvents.reserve(128);strictModeBatch.reserve(128);
#endif
 for(const auto&h:hooks)if(MH_CreateHook(reinterpret_cast<void*>(h.address),h.replacement,h.original)!=MH_OK)return false;
 if(MH_CreateHook(table[94],reinterpret_cast<void*>(obsConstants),reinterpret_cast<void**>(&obsOriginalConstants))!=MH_OK)return false;
 obsEnabled=true;std::fprintf(journal,"OBS observation only; 2-frame captures, max 4096 draws/capture, 40000/session; native timing\n");return true;
}
#else
static int obsDraw(IDirect3DDevice9*,unsigned){return -1;}
static HRESULT obsResult(int,HRESULT hr){return hr;}
static void obsTransformPayload(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*){}
static void obsPresent(){}
static void obsDeviceReset(){}
#endif
