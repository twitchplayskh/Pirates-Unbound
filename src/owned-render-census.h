// Bounded observation of natural callbacks; no traversal or lookup replay.
static unsigned ownedResetCount=0;
struct OwnedCensusEvent {unsigned serial,frame,method,stage;uintptr_t object,caller,result,args[9],fields[19],vt;};
static OwnedCensusEvent ownedCensusEvents[256]{};static unsigned ownedCensusCount=0;
static void ownedCensusEvent(unsigned method,unsigned stage,void* object,uintptr_t caller,uintptr_t result,std::initializer_list<uintptr_t> args){
 if(!ownedProofActive)return;
 auto& p=*ownedProofActive;
 if(!obsSameThread()||ownedCensusCount>=256){++p.censusFailures;return;}
 auto& e=ownedCensusEvents[ownedCensusCount++];e.serial=p.serial;e.frame=p.frame;e.method=method;e.stage=stage;e.object=reinterpret_cast<uintptr_t>(object);e.caller=caller;e.result=result;
 unsigned i=0;for(auto a:args){if(i>=9){++p.censusFailures;break;}e.args[i++]=a;}
 if(method<9){if(!obsReadable(object,0x48)){++p.censusFailures;return;}e.vt=*reinterpret_cast<uintptr_t*>(object);std::memcpy(e.fields,static_cast<char*>(object)+0x24,36);}
 else {if(!obsReadable(object,0x70)){++p.censusFailures;return;}e.vt=*reinterpret_cast<uintptr_t*>(object);std::memcpy(e.fields,static_cast<char*>(object)+0x24,0x4c);}
 ++p.callbackEvents;
}
struct OwnedResourceState {unsigned serial,frame,stage,failures;uintptr_t stream[16],indices,target,depth,decl,vs,ps,texture[4];UINT offset[16],stride[16];DWORD fvf,blend,indexed;HRESULT cooperative;};
static OwnedResourceState ownedResources[16]{};static unsigned ownedResourceCount=0;
static void ownedCensusResource(unsigned stage){
 if(!ownedProofActive)return;
 auto& p=*ownedProofActive;if(ownedResourceCount>=16){++p.censusFailures;return;}
 auto& s=ownedResources[ownedResourceCount++];s.serial=p.serial;s.frame=p.frame;s.stage=stage;auto* d=targetDevice;
 s.cooperative=d->TestCooperativeLevel();
 for(unsigned i=0;i<16;++i){IDirect3DVertexBuffer9* b=nullptr;auto hr=d->GetStreamSource(i,&b,&s.offset[i],&s.stride[i]);if(FAILED(hr))++s.failures;s.stream[i]=reinterpret_cast<uintptr_t>(b);if(b)b->Release();}
 IDirect3DIndexBuffer9* ib=nullptr;if(FAILED(d->GetIndices(&ib)))++s.failures;s.indices=reinterpret_cast<uintptr_t>(ib);if(ib)ib->Release();
 IDirect3DSurface9* rt=nullptr;IDirect3DSurface9* ds=nullptr;if(FAILED(d->GetRenderTarget(0,&rt)))++s.failures;if(FAILED(d->GetDepthStencilSurface(&ds)))++s.failures;s.target=reinterpret_cast<uintptr_t>(rt);s.depth=reinterpret_cast<uintptr_t>(ds);if(rt)rt->Release();if(ds)ds->Release();
 IDirect3DVertexDeclaration9* decl=nullptr;IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;if(FAILED(d->GetVertexDeclaration(&decl)))++s.failures;if(FAILED(d->GetVertexShader(&vs)))++s.failures;if(FAILED(d->GetPixelShader(&ps)))++s.failures;s.decl=reinterpret_cast<uintptr_t>(decl);s.vs=reinterpret_cast<uintptr_t>(vs);s.ps=reinterpret_cast<uintptr_t>(ps);if(decl)decl->Release();if(vs)vs->Release();if(ps)ps->Release();
 if(FAILED(d->GetFVF(&s.fvf)))++s.failures;if(FAILED(d->GetRenderState(D3DRS_VERTEXBLEND,&s.blend)))++s.failures;if(FAILED(d->GetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,&s.indexed)))++s.failures;
 for(unsigned i=0;i<4;++i){IDirect3DBaseTexture9* t=nullptr;if(FAILED(d->GetTexture(i,&t)))++s.failures;s.texture[i]=reinterpret_cast<uintptr_t>(t);if(t)t->Release();}
 p.censusFailures+=s.failures;
}
static void ownedCensusWrite(){
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%scallbacks-%u.csv",traceFolder,obsSerial);auto* f=std::fopen(path,"w");if(f){std::fprintf(f,"serial,frame,method,stage,object,vtable,caller,result");for(unsigned i=0;i<9;++i)std::fprintf(f,",arg_%u",i);for(unsigned i=0;i<19;++i)std::fprintf(f,",field_%x",0x24+4*i);std::fputc('\n',f);for(unsigned i=0;i<ownedCensusCount;++i){auto& e=ownedCensusEvents[i];if(e.serial!=obsSerial)continue;std::fprintf(f,"%u,%u,%u,%u,%p,%p,%p,%p",e.serial,e.frame,e.method,e.stage,reinterpret_cast<void*>(e.object),reinterpret_cast<void*>(e.vt),reinterpret_cast<void*>(e.caller),reinterpret_cast<void*>(e.result));for(auto a:e.args)std::fprintf(f,",%p",reinterpret_cast<void*>(a));for(auto a:e.fields)std::fprintf(f,",%p",reinterpret_cast<void*>(a));std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sresources-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"serial,frame,stage,failures,cooperative,indices,target,depth,declaration,vs,ps,fvf,blend,indexed");for(unsigned i=0;i<16;++i)std::fprintf(f,",stream_%u,offset_%u,stride_%u",i,i,i);for(unsigned i=0;i<4;++i)std::fprintf(f,",texture_%u",i);std::fputc('\n',f);for(unsigned i=0;i<ownedResourceCount;++i){auto& s=ownedResources[i];if(s.serial!=obsSerial)continue;std::fprintf(f,"%u,%u,%u,%u,%ld,%p,%p,%p,%p,%p,%p,%lu,%lu,%lu",s.serial,s.frame,s.stage,s.failures,long(s.cooperative),reinterpret_cast<void*>(s.indices),reinterpret_cast<void*>(s.target),reinterpret_cast<void*>(s.depth),reinterpret_cast<void*>(s.decl),reinterpret_cast<void*>(s.vs),reinterpret_cast<void*>(s.ps),s.fvf,s.blend,s.indexed);for(unsigned j=0;j<16;++j)std::fprintf(f,",%p,%u,%u",reinterpret_cast<void*>(s.stream[j]),s.offset[j],s.stride[j]);for(auto t:s.texture)std::fprintf(f,",%p",reinterpret_cast<void*>(t));std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sconsumption-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");if(f){std::fprintf(f,"serial,lookup_count,lookup_return,selected_program,selected_vtable,callback_events,census_failures,converter_pointer,resets_before,resets_after\n");for(unsigned i=0;i<ownedProofCount;++i){auto& p=ownedProofRows[i];if(p.serial==obsSerial)std::fprintf(f,"%u,%u,%p,%p,%p,%u,%u,%p,%u,%u\n",p.serial,p.lookupCount,reinterpret_cast<void*>(p.lookupResult),reinterpret_cast<void*>(p.selectedProgram),reinterpret_cast<void*>(p.selectedVtable),p.callbackEvents,p.censusFailures,reinterpret_cast<void*>(p.converterPointer),p.resetsBefore,p.resetsAfter);}std::fclose(f);}
}
#include "owned-callback-hooks.h"
