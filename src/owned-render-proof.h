// Fixture-local diagnostic only. No retained history, offsets or extra draws.
#include "fixture-write-guard.h"
struct OwnedProofRow {
 unsigned serial=0,frame=0,mode=0,draws=0,updatesBefore=0,updatesAfter=0,pages=0;
 uintptr_t geometry=0,actor=0,root=0,camera=0,renderer=0,program=0,vtable=0,programToken=0;
 uintptr_t callbacks[9]{};uintptr_t writer=0,address=0;DWORD writerThread=0;
 uintptr_t drawPass=0,drawPassVtable=0,drawPassCallbacks[3]{},drawProducers[3]{};
 uintptr_t nativeInput=0,nativeSphere=0,ownedInput=0,ownedSphere=0;
 uintptr_t lookupResult=0,selectedProgram=0,selectedVtable=0,converterPointer=0;
 unsigned lookupCount=0,callbackEvents=0,censusFailures=0,resetsBefore=0,resetsAfter=0;
 unsigned writes=0;bool attempted=false,substituted=false,guardPass=false,unchanged=false,identity=false;
 char reason[96]{};float transform[13]{},bound[4]{},nativeTransform[13]{},nativeBound[4]{};
 unsigned char geometryBytes[0xd0]{},actorBytes[0xc0]{},rootBytes[0xc4]{},cameraBytes[0x1f0]{};
};
static OwnedProofRow ownedProofRows[4]{};static unsigned ownedProofCount=0,ownedProofTried=0;
static OwnedProofRow* ownedProofActive=nullptr;
#include "owned-render-census.h"
// Zero geometry token alone does not prove the registry lookup returns null.
// These program/pass observations read renderer+898; resolved-program identity
// and nested resource branches must be certified before enabling substitution.
static bool ownedProofCallbacksQualified(uintptr_t vtable){(void)vtable;return false;}
static OwnedProofRow* ownedProofPrepare(void* renderer,void* geometry,void* data,void* skin,void* transform,void* bound,uintptr_t caller){
 if(!obsRecording()||obsContext.geometry!=strictChain.geometry||ownedProofTried==obsSerial||ownedProofCount>=4)return nullptr;
 char ini[MAX_PATH];std::snprintf(ini,sizeof(ini),"%sobservation.ini",traceFolder);
 unsigned mode=GetPrivateProfileIntA("Observation","OwnedProof",0,ini);if(!mode)return nullptr;
 ownedProofTried=obsSerial;auto& p=ownedProofRows[ownedProofCount++];p={};p.serial=obsSerial;p.frame=frames.load();p.mode=mode;
 p.geometry=reinterpret_cast<uintptr_t>(geometry);p.renderer=reinterpret_cast<uintptr_t>(renderer);p.actor=strictChain.actor;p.root=strictChain.root;p.camera=*reinterpret_cast<uintptr_t*>(0x8e9fd8);
 auto reject=[&](const char* s)->OwnedProofRow*{std::snprintf(p.reason,sizeof(p.reason),"%s",s);return nullptr;};
 if(mode>2||caller!=0x5c5c89||p.geometry!=strictChain.geometry||skin||obsContext.alternate)return reject("unsupported lower boundary, mode, skin or alternate");
 if(!obsReadable(geometry,0xd0)||!obsReadable(renderer,0x8a0)||!obsReadable(transform,52)||!obsReadable(bound,16)||!obsReadable(reinterpret_cast<void*>(p.actor),0xc0)||!obsReadable(reinterpret_cast<void*>(p.root),0xc4)||!obsReadable(reinterpret_cast<void*>(p.camera),0x1f0))return reject("unreadable local identity/value");
 auto cache=obsCache(0);if(strictChain.slot!=0||cache.actor!=p.actor||cache.root!=p.root||cache.previous!=0||*reinterpret_cast<unsigned*>(0x85a164)!=4||!strictWorldScope)return reject("not paused player sailing fixture");
 if(*reinterpret_cast<uintptr_t*>(geometry)!=0x6c1a00||reinterpret_cast<uintptr_t>(data)!=*reinterpret_cast<uintptr_t*>(static_cast<char*>(geometry)+0xbc)||*reinterpret_cast<uintptr_t*>(static_cast<char*>(geometry)+0xc0))return reject("geometry type/data/skin mismatch");
 const StrictShipVisibilityRow* vis=nullptr;for(const auto& v:strictShipVisibilityRows)if(v.frame==p.frame&&v.geometry==p.geometry&&v.passCaller==0x4d14ce&&v.passMode==0&&v.passFilter==0&&v.nativeVisible&&v.valid&&v.member&&v.camera==p.camera)vis=&v;
 if(!vis||std::memcmp(vis->hullTransform,transform,52)||std::memcmp(vis->hullBound,bound,16))return reject("no identical current main-pass hull sample");
 const StrictCameraRow* cam=nullptr;for(const auto& c:strictCameraRows)if(c.frame==p.frame&&c.camera==p.camera&&c.renderer==p.renderer&&c.caller==0x4d135c&&c.cw==127&&c.mode==0)cam=&c;
 if(!cam||std::memcmp(cam->planes,vis->planes.data(),96)||std::memcmp(cam->view,static_cast<char*>(renderer)+0x7c0,64)||std::memcmp(cam->projection,static_cast<char*>(renderer)+0x800,64))return reject("unqualified current camera/cache");
 for(unsigned i=0;i<13;++i)if(!std::isfinite(static_cast<float*>(transform)[i]))return reject("nonfinite transform");
 if(static_cast<float*>(transform)[12]<=0||!rigid::finite(*static_cast<rigid::Sphere*>(bound)))return reject("invalid scale/bound");
 unsigned short cw=0;__asm__ __volatile__("fnstcw %0":"=m"(cw));if(cw!=127)return reject("unsupported FP control");
 p.programToken=*reinterpret_cast<uintptr_t*>(static_cast<char*>(geometry)+0xc8);p.program=*reinterpret_cast<uintptr_t*>(static_cast<char*>(renderer)+0x898);
 if(!obsReadable(reinterpret_cast<void*>(p.program),4))return reject("unreadable renderer program");
 p.vtable=*reinterpret_cast<uintptr_t*>(p.program);
 if(!obsReadable(reinterpret_cast<void*>(p.vtable+0x14),36))return reject("unreadable program callbacks");
 std::memcpy(p.callbacks,reinterpret_cast<void*>(p.vtable+0x14),36);
 if(p.programToken)return reject("custom program selection not audited");
 if(mode==2&&!ownedProofCallbacksQualified(p.vtable))return reject("identical substitution gated by callback audit");
 std::memcpy(p.transform,transform,52);std::memcpy(p.nativeTransform,transform,52);std::memcpy(p.bound,bound,16);std::memcpy(p.nativeBound,bound,16);
 p.nativeInput=reinterpret_cast<uintptr_t>(transform);p.nativeSphere=reinterpret_cast<uintptr_t>(bound);p.ownedInput=reinterpret_cast<uintptr_t>(p.transform);p.ownedSphere=reinterpret_cast<uintptr_t>(p.bound);
 std::memcpy(p.geometryBytes,geometry,sizeof(p.geometryBytes));std::memcpy(p.actorBytes,reinterpret_cast<void*>(p.actor),sizeof(p.actorBytes));std::memcpy(p.rootBytes,reinterpret_cast<void*>(p.root),sizeof(p.rootBytes));std::memcpy(p.cameraBytes,reinterpret_cast<void*>(p.camera),sizeof(p.cameraBytes));
 p.updatesBefore=strictPlayerUpdateId;p.resetsBefore=ownedResetCount;p.identity=true;return &p;
}
static void ownedProofAtDraw(){
 if(!ownedProofActive)return;
 auto& p=*ownedProofActive;++p.draws;
 ownedCensusResource(2);
 if(!obsReadable(reinterpret_cast<void*>(p.program),0x48))return;
 p.drawPass=*reinterpret_cast<uintptr_t*>(p.program+0x3c);
 std::memcpy(p.drawProducers,reinterpret_cast<void*>(p.program+0x28),12);
 if(obsReadable(reinterpret_cast<void*>(p.drawPass),4)){
  p.drawPassVtable=*reinterpret_cast<uintptr_t*>(p.drawPass);
  if(obsReadable(reinterpret_cast<void*>(p.drawPassVtable+4),12))std::memcpy(p.drawPassCallbacks,reinterpret_cast<void*>(p.drawPassVtable+4),12);
 }
}
static bool ownedProofGuardBegin(OwnedProofRow& p,FixtureWriteGuard& g){
 bool ok=g.add(reinterpret_cast<void*>(p.geometry),sizeof(p.geometryBytes))&&g.add(reinterpret_cast<void*>(p.actor),sizeof(p.actorBytes))&&g.add(reinterpret_cast<void*>(p.root),sizeof(p.rootBytes))&&g.add(reinterpret_cast<void*>(p.camera),sizeof(p.cameraBytes))&&g.add(reinterpret_cast<void*>(0x8b9870),4)&&g.add(reinterpret_cast<void*>(0x8e9fd8),4);
 if(!ok||!g.begin()){std::snprintf(p.reason,sizeof(p.reason),"guard setup rejected; native fallback");return false;}
 p.attempted=true;p.pages=g.pageCount;ownedProofActive=&p;ownedCensusResource(1);return true;
}
static void ownedProofFinish(OwnedProofRow& p,FixtureWriteGuard& g){
 ownedCensusResource(3);p.resetsAfter=ownedResetCount;
 ownedProofActive=nullptr;p.guardPass=g.end();p.writes=g.violations;p.writer=g.first.writer;p.address=g.first.address;p.writerThread=g.first.thread;p.updatesAfter=strictPlayerUpdateId;
 p.unchanged=!std::memcmp(p.geometryBytes,reinterpret_cast<void*>(p.geometry),sizeof(p.geometryBytes))&&!std::memcmp(p.actorBytes,reinterpret_cast<void*>(p.actor),sizeof(p.actorBytes))&&!std::memcmp(p.rootBytes,reinterpret_cast<void*>(p.root),sizeof(p.rootBytes))&&!std::memcmp(p.cameraBytes,reinterpret_cast<void*>(p.camera),sizeof(p.cameraBytes));
 auto c=obsCache(0);p.identity=c.actor==p.actor&&c.root==p.root&&*reinterpret_cast<uintptr_t*>(0x8e9fd8)==p.camera&&frames.load()==p.frame;
 std::snprintf(p.reason,sizeof(p.reason),"%s",p.guardPass&&p.unchanged&&p.identity&&p.draws==1&&p.updatesBefore==p.updatesAfter?(p.substituted?"identical owned draw completed; compare downstream":"native guard control completed; audit callbacks"):"local proof failed; no further operation");
}
static void ownedProofWrite(){ownedCensusWrite();char path[MAX_PATH];std::snprintf(path,sizeof(path),"%sowned-proof-%u.csv",traceFolder,obsSerial);auto* f=std::fopen(path,"w");if(!f)return;
 std::fprintf(f,"serial,frame,mode,attempted,substituted,guard_pass,unchanged,identity,pages,writes,writer,address,writer_thread,draws,updates_before,updates_after,geometry,actor,root,camera,renderer,program,program_vtable,program_token,reason");for(unsigned i=0;i<9;++i)std::fprintf(f,",callback_%u",i);std::fprintf(f,",draw_pass,draw_pass_vtable,pass_callback_0,pass_callback_1,pass_callback_2,producer_0,producer_1,producer_2,native_input,native_bound,owned_input,owned_bound\n");
 for(unsigned i=0;i<ownedProofCount;++i){auto& p=ownedProofRows[i];if(p.serial!=obsSerial)continue;std::fprintf(f,"%u,%u,%u,%d,%d,%d,%d,%d,%u,%u,%p,%p,%lu,%u,%u,%u,%p,%p,%p,%p,%p,%p,%p,%p,%s",p.serial,p.frame,p.mode,p.attempted,p.substituted,p.guardPass,p.unchanged,p.identity,p.pages,p.writes,reinterpret_cast<void*>(p.writer),reinterpret_cast<void*>(p.address),static_cast<unsigned long>(p.writerThread),p.draws,p.updatesBefore,p.updatesAfter,reinterpret_cast<void*>(p.geometry),reinterpret_cast<void*>(p.actor),reinterpret_cast<void*>(p.root),reinterpret_cast<void*>(p.camera),reinterpret_cast<void*>(p.renderer),reinterpret_cast<void*>(p.program),reinterpret_cast<void*>(p.vtable),reinterpret_cast<void*>(p.programToken),p.reason);for(auto cb:p.callbacks)std::fprintf(f,",%p",reinterpret_cast<void*>(cb));std::fprintf(f,",%p,%p",reinterpret_cast<void*>(p.drawPass),reinterpret_cast<void*>(p.drawPassVtable));for(auto cb:p.drawPassCallbacks)std::fprintf(f,",%p",reinterpret_cast<void*>(cb));for(auto cb:p.drawProducers)std::fprintf(f,",%p",reinterpret_cast<void*>(cb));for(auto cb:{p.nativeInput,p.nativeSphere,p.ownedInput,p.ownedSphere})std::fprintf(f,",%p",reinterpret_cast<void*>(cb));std::fputc('\n',f);}std::fclose(f);
}
