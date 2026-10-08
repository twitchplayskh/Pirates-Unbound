// Included only by the diagnostic observer. Does not enable presentation.
#include "rigid-presentation.h"
struct StrictChain {
 int slot=-1,model=-1;
 uintptr_t actor=0,root=0,geometry=0,parent=0;
 std::array<uintptr_t,26> ancestors{};
 unsigned ancestorCount=0;
};
static StrictChain strictChain{};
static rigid::HistoryGate strictHistory{};
static unsigned strictIgnoredDestruction=0,strictDrops=0,strictMismatch=0;
static unsigned strictOwnerFlips=0,strictEntryCalls=0,strictCullCalls=0;
static bool strictTargeted=true;
static bool strictWatches(uintptr_t p) {
 if(!strictTargeted||!p)return false;
 if(p==strictChain.actor||p==strictChain.geometry)return true;
 for(unsigned i=0;i<strictChain.ancestorCount;++i)if(p==strictChain.ancestors[i])return true;
 return false;
}
static void strictInvalidate(rigid::InvalidReason why) {strictHistory.invalidate(why);}
static bool obsStrictCacheChange(int slot,const ObsCache& before,const ObsCache& after) {
 if(slot!=strictChain.slot)return false;
 if(before.actor==after.actor&&before.root==after.root&&before.model==after.model){++strictOwnerFlips;return false;}
 strictInvalidate(rigid::InvalidReason::IdentityChange);
 if(obsEvents.size()>=2048){++strictDrops;strictInvalidate(rigid::InvalidReason::EventLoss);}
 return true;
}
static void strictSelect(int slot) {
 strictChain={};strictInvalidate(rigid::InvalidReason::Gap);
 if(slot<0||slot>=20)return;
 const auto cache=obsCache(slot);
 unsigned matches=0;const ObsNode* selected=nullptr;
 for(const auto& n:obsNodes)if(n.slot==slot&&!std::strcmp(n.name,"hull:6")){++matches;selected=&n;}
 if(matches!=1||!cache.actor||!cache.root||!selected)return;
 StrictChain chain{};chain.slot=slot;chain.actor=cache.actor;chain.root=cache.root;chain.model=cache.model;
 chain.geometry=selected->node;chain.parent=selected->parent;
 if(!obsReadable(reinterpret_cast<void*>(chain.geometry),0xc4)||*reinterpret_cast<uintptr_t*>(chain.geometry)!=0x6c1a00)return;
 uintptr_t at=chain.parent;
 while(at&&chain.ancestorCount<chain.ancestors.size()) {
  if(!obsReadable(reinterpret_cast<void*>(at),0xc4)||*reinterpret_cast<uintptr_t*>(at)!=0x6c0bd8)return; // excludes billboard
  chain.ancestors[chain.ancestorCount++]=at;
  if(at==cache.root){strictChain=chain;return;}
  uintptr_t parent=0;for(const auto& n:obsNodes)if(n.node==at){parent=n.parent;break;}
  at=parent;
 }
}
struct StrictCullRow {
 unsigned frame,epoch,maskBefore,maskNative,maskOwned;
 LONGLONG qpc;
 uintptr_t node,camera;
 bool outsideNative,outsideOwned,valid,comparisonAvailable;
 unsigned flags,modeFlags;
 unsigned stage; // 0 entry, 1 native sphere test, 2 late draw, 3 independent cache capture
 float transform[13],rootTransform[13];
 float position[3],axes[9],frustum[6],viewport[4];
 rigid::Sphere bound;
 std::array<rigid::Plane,6> planes;
};
static std::vector<StrictCullRow> strictCullRows;
struct StrictSummary {StrictChain chain;std::uint64_t epoch;bool valid;unsigned ignored,drops,mismatches,ownerFlips,entryCalls,cullCalls;};
static StrictSummary strictSummary{};
#include "strict-native-boundaries.h"
static void strictFreeze(){strictModeBatch.clear();strictModeBatch.swap(strictModeEvents);strictSummary={strictChain,strictHistory.epoch,strictHistory.valid,strictIgnoredDestruction,strictDrops,strictMismatch,strictOwnerFlips,strictEntryCalls,strictCullCalls};}
using StrictNativeCull=bool(__attribute__((thiscall))*)(void*,void*);
static StrictNativeCull strictOriginalCull=nullptr;
static StrictCullRow strictRead(void* node,void* camera) {
 StrictCullRow row{};
 row.frame=frames.load();row.epoch=unsigned(strictHistory.epoch);row.node=reinterpret_cast<uintptr_t>(node);row.camera=reinterpret_cast<uintptr_t>(camera);
 LARGE_INTEGER timestamp{};QueryPerformanceCounter(&timestamp);row.qpc=timestamp.QuadPart;
 std::memcpy(&row.bound,static_cast<char*>(node)+0x28,16);
 std::memcpy(row.planes.data(),static_cast<char*>(camera)+0x18c,96);
 std::memcpy(&row.maskBefore,static_cast<char*>(camera)+0x1ec,4);
 std::memcpy(row.position,static_cast<char*>(camera)+0x90,12);
 std::memcpy(row.axes,static_cast<char*>(camera)+0x104,36);
 std::memcpy(row.frustum,static_cast<char*>(camera)+0x128,24);
 std::memcpy(row.viewport,static_cast<char*>(camera)+0x144,16);
 std::memcpy(row.transform,static_cast<char*>(node)+0x6c,52);
 if(obsReadable(reinterpret_cast<void*>(strictChain.root+0x6c),52))std::memcpy(row.rootTransform,reinterpret_cast<void*>(strictChain.root+0x6c),52);
 row.flags=*reinterpret_cast<unsigned*>(static_cast<char*>(node)+0x20);
 row.modeFlags=*reinterpret_cast<unsigned*>(0x85a164); // raw evidence; NOT a qualified mode detector
 return row;
}
static void strictAppend(const StrictCullRow& row) {
 if(strictCullRows.size()<512)strictCullRows.push_back(row);
 else {++strictDrops;strictInvalidate(rigid::InvalidReason::EventLoss);}
}
// The first word is a verified return address. Remaining words are bounded raw
// evidence, not an unwound call chain: the native game omits frame pointers.
struct StrictDispatchRow {
 unsigned frame,stage;LONGLONG qpc;uintptr_t geometry,renderer,rendererVtable;
 unsigned short controlWord;std::array<uintptr_t,128> stack;
};
static std::vector<StrictDispatchRow> strictDispatchRows;
struct StrictCameraRow {
 unsigned frame;LONGLONG qpc;uintptr_t renderer,caller,camera;unsigned short cw;unsigned mode;
 float inputs[22],view[16],projection[16],planes[24];
};
static std::vector<StrictCameraRow> strictCameraRows;
using StrictCameraBuild=void(__attribute__((thiscall))*)(void*,const void*,const void*,const void*,const void*,const void*,const void*);
static StrictCameraBuild strictOriginalCameraBuild=nullptr;
static void __attribute__((fastcall,noinline)) strictCameraBuild(void* renderer,void*,const void* position,const void* direction,const void* up,const void* right,const void* frustum,const void* viewport) {
 StrictCameraRow r{};bool capture=obsRecording();
 if(capture){r.frame=frames.load();LARGE_INTEGER t{};QueryPerformanceCounter(&t);r.qpc=t.QuadPart;
  r.renderer=reinterpret_cast<uintptr_t>(renderer);r.caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  r.camera=*reinterpret_cast<uintptr_t*>(0x8e9fd8);
  capture=reinterpret_cast<uintptr_t>(position)==r.camera+0x90&&obsReadable(renderer,0x840)&&obsReadable(reinterpret_cast<void*>(r.camera),0x1f0)&&obsReadable(frustum,25);
  if(capture){__asm__ __volatile__("fnstcw %0":"=m"(r.cw));
   const void* ptrs[]={position,direction,up,right,frustum,viewport};const unsigned sizes[]={3,3,3,3,6,4};unsigned at=0;
   for(unsigned i=0;i<6;++i){if(!obsReadable(ptrs[i],sizes[i]*4)){capture=false;break;}std::memcpy(r.inputs+at,ptrs[i],sizes[i]*4);at+=sizes[i];}
   r.mode=*reinterpret_cast<const unsigned char*>(static_cast<const char*>(frustum)+24);
   std::memcpy(r.planes,reinterpret_cast<void*>(r.camera+0x18c),96);
  }
 }
 strictOriginalCameraBuild(renderer,position,direction,up,right,frustum,viewport); // No camera-builder replay.
 if(capture){std::memcpy(r.view,static_cast<char*>(renderer)+0x7c0,64);std::memcpy(r.projection,static_cast<char*>(renderer)+0x800,64);
  if(strictCameraRows.size()<16)strictCameraRows.push_back(r);else {++strictDrops;strictInvalidate(rigid::InvalidReason::EventLoss);}}
}
struct StrictShipVisibilityRow {
 unsigned frame,epoch;LONGLONG qpc;uintptr_t actor,root,camera,boundPointer,caller;
 unsigned passSerial,passMode,passFilter;uintptr_t passCaller,list,geometry;bool passKnown;
 unsigned modeGeneration,playerUpdateId,rawModeFlags;LONGLONG playerUpdateQpc;bool worldScope;
 unsigned short controlWord;bool nativeVisible,ownedVisible,valid,member;
 rigid::Sphere bound;std::array<rigid::Plane,6> planes;float rootTransform[13];
 float hullTransform[13],hullBound[4],cameraInputs[22];
};
static std::vector<StrictShipVisibilityRow> strictShipVisibilityRows;
struct StrictShipPass {unsigned serial=0,mode=0,filter=0;uintptr_t caller=0;bool active=false;};
static thread_local StrictShipPass strictShipPass{};
static unsigned strictShipPassSerial=0;
extern "C" {static void* strictShipPassTrampoline=nullptr;}
extern "C" void __attribute__((cdecl)) strictShipPassBefore(unsigned mode,unsigned filter,uintptr_t caller) {
 if(strictShipPass.active){strictInvalidate(rigid::InvalidReason::Gap);strictShipPass={};return;}
 if(!obsSameThread()){strictInvalidate(rigid::InvalidReason::ThreadMismatch);return;}
 strictShipPass={++strictShipPassSerial,mode,filter,caller,true};
}
extern "C" void __attribute__((cdecl)) strictShipPassAfter() {strictShipPass={};}
// 4d0ab0 is cdecl(mode, filter). Copy its arguments onto the trampoline's
// stack; preserve the native result, flags and FP state around both callbacks.
extern "C" void __attribute__((naked)) strictShipPassEntry(){__asm__ __volatile__("pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tpushl 36(%ebp)\n\tpushl 44(%ebp)\n\tpushl 40(%ebp)\n\tcall _strictShipPassBefore\n\taddl $12,%esp\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tpushl 8(%esp)\n\tpushl 8(%esp)\n\tcall *_strictShipPassTrampoline\n\tleal 8(%esp),%esp\n\tpushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tcall _strictShipPassAfter\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tret");}
static thread_local bool strictShipPending=false;
static thread_local StrictShipVisibilityRow strictShipRow{};
// This route uses all planes, no traversal mask, Z+Y+X, and inclusive outside.
// Its observed PC=24 arithmetic is distinct from the generic NiAV culler.
static bool strictShipOwnedVisible(const StrictShipVisibilityRow& r) {
 for(const auto& p:r.planes){
  volatile float z=p.normal[2]*r.bound.center[2],y=p.normal[1]*r.bound.center[1],x=p.normal[0]*r.bound.center[0];
  volatile float zy=z+y,zyx=zy+x,d=zyx-p.constant;
  if(d <= -r.bound.radius)return false;
 }return true;
}
extern "C" {static void* strictShipVisibilityTrampoline=nullptr;}
extern "C" void __attribute__((cdecl)) strictShipVisibilityBefore(uintptr_t camera,uintptr_t bound,uintptr_t actor,uintptr_t caller) {
 strictShipPending=false;
 if(!obsRecording()||caller!=0x4d0ba4||!obsReadable(reinterpret_cast<void*>(actor),0xa0))return;
 auto root=*reinterpret_cast<uintptr_t*>(actor+0x1c);
 if(root!=strictChain.root||!strictChain.geometry)return;
 if(!obsReadable(reinterpret_cast<void*>(camera),0x1f0)||!obsReadable(reinterpret_cast<void*>(bound),16)||!obsReadable(reinterpret_cast<void*>(root+0x6c),52)||!obsReadable(reinterpret_cast<void*>(strictChain.geometry),0xc4)){
  strictInvalidate(rigid::InvalidReason::BadSample);return;
 }
 auto& r=strictShipRow;r={};r.frame=frames.load();r.epoch=unsigned(strictHistory.epoch);
 LARGE_INTEGER t{};QueryPerformanceCounter(&t);r.qpc=t.QuadPart;
 r.actor=actor;r.root=root;r.camera=camera;r.boundPointer=bound;r.caller=caller;
 r.passSerial=strictShipPass.serial;r.passMode=strictShipPass.mode;r.passFilter=strictShipPass.filter;r.passCaller=strictShipPass.caller;r.passKnown=strictShipPass.active;
 r.modeGeneration=strictModeGeneration;r.playerUpdateId=strictPlayerUpdateId;r.playerUpdateQpc=strictPlayerUpdateQpc;r.worldScope=strictWorldScope;r.rawModeFlags=*reinterpret_cast<unsigned*>(0x85a164);
 r.geometry=strictChain.geometry;
 __asm__ __volatile__("fnstcw %0":"=m"(r.controlWord));
 std::memcpy(&r.bound,reinterpret_cast<void*>(bound),16);std::memcpy(r.planes.data(),reinterpret_cast<void*>(camera+0x18c),96);
 std::memcpy(r.rootTransform,reinterpret_cast<void*>(root+0x6c),52);
 std::memcpy(r.hullTransform,reinterpret_cast<void*>(r.geometry+0x6c),52);std::memcpy(r.hullBound,reinterpret_cast<void*>(r.geometry+0x28),16);
 std::memcpy(r.cameraInputs,reinterpret_cast<void*>(camera+0x90),12);std::memcpy(r.cameraInputs+3,reinterpret_cast<void*>(camera+0x104),36);
 std::memcpy(r.cameraInputs+12,reinterpret_cast<void*>(camera+0x128),24);std::memcpy(r.cameraInputs+18,reinterpret_cast<void*>(camera+0x144),16);
 auto list=(r.passKnown&&r.passMode<4&&obsReadable(reinterpret_cast<void*>(actor),0xa4))?*reinterpret_cast<uintptr_t*>(actor+0x94+4*r.passMode):0;r.list=list;
 if(obsReadable(reinterpret_cast<void*>(list),0x10)){
  auto data=*reinterpret_cast<uintptr_t*>(list+4);auto count=*reinterpret_cast<unsigned*>(list+0xc);
  if(count<=2500&&obsReadable(reinterpret_cast<void*>(data),count*4))
   for(unsigned i=0;i<count;++i)if(*reinterpret_cast<uintptr_t*>(data+i*4)==strictChain.geometry)r.member=true;
 }
 r.valid=(r.controlWord&0xf00)==0&&rigid::cull(r.planes,r.bound,63).valid&&actor==strictChain.actor&&bound==root+0x28&&r.member&&r.passKnown;
 r.ownedVisible=strictShipOwnedVisible(r);strictShipPending=true;
}
extern "C" void __attribute__((cdecl)) strictShipVisibilityAfter(unsigned result) {
 if(!strictShipPending)return;
 strictShipPending=false;
 auto r=strictShipRow;r.nativeVisible=(result&255)!=0;
 if(r.nativeVisible!=r.ownedVisible){++strictMismatch;strictInvalidate(rigid::InvalidReason::BadSample);}
 else if(!r.valid)strictInvalidate(rigid::InvalidReason::BadSample); // excluded pass/list is not an arithmetic mismatch
 if(strictShipVisibilityRows.size()<16)strictShipVisibilityRows.push_back(r);
 else {++strictDrops;strictInvalidate(rigid::InvalidReason::EventLoss);}
}
// Verified custom ABI: EAX camera, ESI sphere, EDI actor in the selected caller.
// RET 0; preserve all native registers/flags and x87/SSE state around observers.
extern "C" void __attribute__((naked)) strictShipVisibilityEntry(){__asm__ __volatile__("pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tpushl 36(%ebp)\n\tpushl 0(%ebp)\n\tpushl 4(%ebp)\n\tpushl 28(%ebp)\n\tcall _strictShipVisibilityBefore\n\taddl $16,%esp\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tcall *_strictShipVisibilityTrampoline\n\tpushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tpushl 28(%ebp)\n\tcall _strictShipVisibilityAfter\n\taddl $4,%esp\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tret");}
static void strictDispatchTrace(void* geometry,void* renderer,unsigned stage,const uintptr_t* stack) {
 if(reinterpret_cast<uintptr_t>(geometry)!=strictChain.geometry||!obsRecording())return;
 if(strictDispatchRows.size()>=16||!obsReadable(stack,512)||!obsReadable(renderer,4)) {
  ++strictDrops;strictInvalidate(rigid::InvalidReason::EventLoss);return;
 }
 StrictDispatchRow row{};row.frame=frames.load();row.stage=stage;
 LARGE_INTEGER t{};QueryPerformanceCounter(&t);row.qpc=t.QuadPart;
 row.geometry=reinterpret_cast<uintptr_t>(geometry);row.renderer=reinterpret_cast<uintptr_t>(renderer);
 row.rendererVtable=*reinterpret_cast<uintptr_t*>(renderer);
 __asm__ __volatile__("fnstcw %0":"=m"(row.controlWord));
 std::memcpy(row.stack.data(),stack,512);strictDispatchRows.push_back(row);
}
using StrictRigidDispatch=void(__attribute__((thiscall))*)(void*,void*);
static StrictRigidDispatch strictOriginalRigidDispatch=nullptr;
static void __attribute__((fastcall,noinline)) strictRigidDispatch(void* geometry,void*,void* renderer) {
 strictDispatchTrace(geometry,renderer,0,reinterpret_cast<const uintptr_t*>(__builtin_frame_address(0))+1);
 strictOriginalRigidDispatch(geometry,renderer); // One native invocation; no traversal replay.
}
static void strictCaptureValues(unsigned stage) {
 if(!strictChain.geometry)return;
 auto* node=reinterpret_cast<void*>(strictChain.geometry);
 auto* camera=*reinterpret_cast<void**>(0x8e9fd8);
 if(!obsReadable(node,0xc4)||!obsReadable(camera,0x1f0)){strictInvalidate(rigid::InvalidReason::BadSample);return;}
 auto row=strictRead(node,camera);row.stage=stage;
 auto own=rigid::cull(row.planes,row.bound,row.maskBefore);
 row.maskOwned=own.mask;row.outsideOwned=own.outside;row.valid=own.valid;
 strictAppend(row);
}
using StrictNativeEntry=void(__attribute__((thiscall))*)(void*,void*);
static StrictNativeEntry strictOriginalEntry=nullptr;
static void __attribute__((fastcall)) strictEntry(void* node,void*,void* camera) {
 if(obsSameThread())++strictEntryCalls;
 if(strictTargeted&&obsRecording()&&reinterpret_cast<uintptr_t>(node)==strictChain.geometry) {
  if(obsReadable(node,0xc4)&&obsReadable(camera,0x1f0)) {
   auto row=strictRead(node,camera);auto own=rigid::cull(row.planes,row.bound,row.maskBefore);
   row.maskOwned=own.mask;row.outsideOwned=own.outside;row.valid=own.valid;row.comparisonAvailable=false;
   strictAppend(row);
  }else strictInvalidate(rigid::InvalidReason::BadSample);
 }
 strictOriginalEntry(node,camera);
}
static bool __attribute__((fastcall)) strictCull(void* node,void*,void* camera) {
 if(obsSameThread())++strictCullCalls;
 const bool selected=strictTargeted&&obsRecording()&&strictWatches(reinterpret_cast<uintptr_t>(node));
 StrictCullRow row{};rigid::CullResult owned{};
 bool captured=selected&&obsReadable(node,0xc4)&&obsReadable(camera,0x1f0);
 if(captured) {
  row=strictRead(node,camera);row.comparisonAvailable=true;row.stage=1;
  owned=rigid::cull(row.planes,row.bound,row.maskBefore);
 }
 // Exactly one original invocation. Its native mask writes are engine behavior.
 const bool native=strictOriginalCull(node,camera);
 if(captured) {
  std::memcpy(&row.maskNative,static_cast<char*>(camera)+0x1ec,4);
  row.maskOwned=owned.mask;row.outsideNative=native;row.outsideOwned=owned.outside;row.valid=owned.valid;
  if(!owned.valid||native!=owned.outside||row.maskNative!=owned.mask){++strictMismatch;strictInvalidate(rigid::InvalidReason::BadSample);}
  strictAppend(row);
 }else if(selected)strictInvalidate(rigid::InvalidReason::BadSample);
 return native;
}
static void strictWrite() {
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%sstrict-mode-%u.csv",traceFolder,obsSerial);FILE* f=std::fopen(path,"w");
 if(f){std::fprintf(f,"frame,qpc,kind,stage,generation,raw_flags,argument,caller,actor,root,world_scope\n");for(const auto& r:strictModeBatch)std::fprintf(f,"%u,%lld,%u,%u,%u,%u,%u,%p,%p,%p,%d\n",r.frame,r.qpc,r.kind,r.stage,r.generation,r.rawFlags,r.argument,reinterpret_cast<void*>(r.caller),reinterpret_cast<void*>(r.actor),reinterpret_cast<void*>(r.root),r.worldScope);std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sstrict-camera-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");
 if(f){std::fprintf(f,"frame,qpc,renderer,caller,camera,control_word,projection_mode");
  for(unsigned i=0;i<22;++i){std::fprintf(f,",input_%u",i);}for(unsigned i=0;i<16;++i){std::fprintf(f,",view_%u",i);}for(unsigned i=0;i<16;++i){std::fprintf(f,",projection_%u",i);}for(unsigned i=0;i<24;++i){std::fprintf(f,",plane_%u",i);}std::fputc('\n',f);
  for(const auto& r:strictCameraRows){std::fprintf(f,"%u,%lld,%p,%p,%p,%u,%u",r.frame,r.qpc,reinterpret_cast<void*>(r.renderer),reinterpret_cast<void*>(r.caller),reinterpret_cast<void*>(r.camera),unsigned(r.cw),r.mode);
   obsFloats(f,r.inputs,22);obsFloats(f,r.view,16);obsFloats(f,r.projection,16);obsFloats(f,r.planes,24);std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sstrict-ship-visibility-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");
 if(f){std::fprintf(f,"frame,epoch,qpc,actor,root,camera,bound_pointer,caller,control_word,native_visible,owned_visible,valid,member,pass_serial,pass_mode,pass_filter,pass_caller,pass_known,list,geometry,mode_generation,player_update_id,player_update_qpc,raw_mode_flags,world_scope");
  for(unsigned i=0;i<4;++i){std::fprintf(f,",bound_%u",i);}for(unsigned i=0;i<24;++i){std::fprintf(f,",plane_%u",i);}for(unsigned i=0;i<13;++i){std::fprintf(f,",root_%u",i);}
  for(unsigned i=0;i<13;++i){std::fprintf(f,",hull_%u",i);}for(unsigned i=0;i<4;++i){std::fprintf(f,",hull_bound_%u",i);}for(unsigned i=0;i<22;++i){std::fprintf(f,",camera_input_%u",i);}std::fputc('\n',f);
  for(const auto& r:strictShipVisibilityRows){std::fprintf(f,"%u,%u,%lld,%p,%p,%p,%p,%p,%u,%d,%d,%d,%d",r.frame,r.epoch,r.qpc,reinterpret_cast<void*>(r.actor),reinterpret_cast<void*>(r.root),reinterpret_cast<void*>(r.camera),reinterpret_cast<void*>(r.boundPointer),reinterpret_cast<void*>(r.caller),unsigned(r.controlWord),r.nativeVisible,r.ownedVisible,r.valid,r.member);
   std::fprintf(f,",%u,%u,%u,%p,%d,%p,%p",r.passSerial,r.passMode,r.passFilter,reinterpret_cast<void*>(r.passCaller),r.passKnown,reinterpret_cast<void*>(r.list),reinterpret_cast<void*>(r.geometry));
   std::fprintf(f,",%u,%u,%lld,%u,%d",r.modeGeneration,r.playerUpdateId,r.playerUpdateQpc,r.rawModeFlags,r.worldScope);
   obsFloats(f,reinterpret_cast<const float*>(&r.bound),4);obsFloats(f,reinterpret_cast<const float*>(r.planes.data()),24);obsFloats(f,r.rootTransform,13);obsFloats(f,r.hullTransform,13);obsFloats(f,r.hullBound,4);obsFloats(f,r.cameraInputs,22);std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sstrict-dispatch-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");
 if(f){std::fprintf(f,"frame,stage,qpc,geometry,renderer,renderer_vtable,control_word");
  for(unsigned i=0;i<128;++i){std::fprintf(f,",stack_%u",i);}std::fputc('\n',f);
  for(const auto& r:strictDispatchRows){std::fprintf(f,"%u,%u,%lld,%p,%p,%p,%u",r.frame,r.stage,r.qpc,reinterpret_cast<void*>(r.geometry),reinterpret_cast<void*>(r.renderer),reinterpret_cast<void*>(r.rendererVtable),unsigned(r.controlWord));
   for(auto v:r.stack){std::fprintf(f,",%p",reinterpret_cast<void*>(v));}std::fputc('\n',f);}std::fclose(f);}
 std::snprintf(path,sizeof(path),"%sstrict-cull-%u.csv",traceFolder,obsSerial);f=std::fopen(path,"w");
 if(f) {
  std::fprintf(f,"frame,history_epoch,qpc,node,camera,mask_before,mask_native,mask_owned,outside_native,outside_owned,valid,comparison_available,node_flags,raw_mode_flags,stage");
  for(const auto label:{"position","axes","frustum","viewport","bound","planes"}) {
   unsigned count=!std::strcmp(label,"position")?3:!std::strcmp(label,"axes")?9:!std::strcmp(label,"frustum")?6:!std::strcmp(label,"planes")?24:4;
   for(unsigned i=0;i<count;++i)std::fprintf(f,",%s_%u",label,i);
  }for(const auto label:{"transform","root_transform"})for(unsigned i=0;i<13;++i)std::fprintf(f,",%s_%u",label,i);
  std::fputc('\n',f);
  for(const auto& row:strictCullRows) {
   std::fprintf(f,"%u,%u,%lld,%p,%p,%u,%u,%u,%d,%d,%d,%d,%u,%u,%u",row.frame,row.epoch,row.qpc,reinterpret_cast<void*>(row.node),reinterpret_cast<void*>(row.camera),row.maskBefore,row.maskNative,row.maskOwned,row.comparisonAvailable?int(row.outsideNative):-1,row.outsideOwned,row.valid,row.comparisonAvailable,row.flags,row.modeFlags,row.stage);
   obsFloats(f,row.position,3);obsFloats(f,row.axes,9);obsFloats(f,row.frustum,6);obsFloats(f,row.viewport,4);
   obsFloats(f,reinterpret_cast<const float*>(&row.bound),4);obsFloats(f,reinterpret_cast<const float*>(row.planes.data()),24);obsFloats(f,row.transform,13);obsFloats(f,row.rootTransform,13);std::fputc('\n',f);
  }std::fclose(f);
 }
 std::snprintf(path,sizeof(path),"%sstrict-chain-%u.txt",traceFolder,obsSerial);f=std::fopen(path,"w");
 if(f) {
  const auto& c=strictSummary.chain;
  std::fprintf(f,"targeted=1\nslot=%d\nactor=%p\nroot=%p\ngeometry=%p\nparent=%p\nmodel=%d\nancestors=%u\nhistory_valid=%d\nhistory_epoch=%llu\nignored_destructions=%u\ndropped=%u\ncull_mismatches=%u\nlifetime_qualified=0\nmode_qualified=0\n",c.slot,reinterpret_cast<void*>(c.actor),reinterpret_cast<void*>(c.root),reinterpret_cast<void*>(c.geometry),reinterpret_cast<void*>(c.parent),c.model,c.ancestorCount,strictSummary.valid,static_cast<unsigned long long>(strictSummary.epoch),strictSummary.ignored,strictSummary.drops,strictSummary.mismatches);
  std::fprintf(f,"owner_bookkeeping_count=%u\nglobal_entry_calls=%u\nglobal_cull_calls=%u\n",strictSummary.ownerFlips,strictSummary.entryCalls,strictSummary.cullCalls);
  std::fclose(f);
 }
}
