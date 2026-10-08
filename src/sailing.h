// Opt-in sailing redraw experiment. Preserve the original simulation loop.
// Repeated poses are intentional until render-only interpolation is verified.
static bool sailingSeparate=false,sailingExtra=false,sailingHealthy=true;
static bool sailingFrameActive=false,sailingCleanupPending=false;
#ifdef PIRATES_TIMING_DIAGNOSTIC
static bool sailingCaptureOverlay=true;
#endif
static bool sailingLabelPass=false,sailingEffectPass=false;
extern "C" void __attribute__((cdecl)) sailingEffectBegin(){sailingEffectPass=true;}
extern "C" void __attribute__((cdecl)) sailingEffectEnd(){sailingEffectPass=false;}
extern "C" void __attribute__((cdecl)) sailingLabelBegin(){sailingLabelPass=true;}
extern "C" void __attribute__((cdecl)) sailingLabelEnd(){sailingLabelPass=false;}
extern "C" bool __attribute__((cdecl)) sailingSkipLabelNode(){return sailingExtra;}
// Extra frames must not invoke transient nodes after native cleanup.
extern "C" void __attribute__((naked)) sailingLabels(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tcall _sailingSkipLabelNode\n\ttest %al,%al\n\tjz 1f\n\tpopal\n\tpopfl\n\tpush $0x4d17b8\n\tret\n1:\n\tcall _sailingEffectBegin\n\tpopal\n\tpopfl\n\tmovl (%esi),%edx\n\tpush %eax\n\tmovl %esi,%ecx\n\tcall *0x60(%edx)\n\tpushfl\n\tpushal\n\tcall _sailingEffectEnd\n\tpopal\n\tpopfl\n\tpush $0x4d17b8\n\tret");
}
extern "C" {static void* volatile sailingTextOriginal __attribute__((used))=reinterpret_cast<void*>(0x50b760);}
// The text queue routine takes its object in EAX and one callee-popped argument.
extern "C" void __attribute__((naked)) sailingTextEntry(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tcall _sailingSkipLabelNode\n\ttest %al,%al\n\tjz 1f\n\tpopal\n\tpopfl\n\tadd $4,%esp\n\tpush $0x4d1805\n\tret\n1:\n\tcall _sailingLabelBegin\n\tpopal\n\tpopfl\n\tcall *_sailingTextOriginal\n\tpushfl\n\tpushal\n\tcall _sailingLabelEnd\n\tpopal\n\tpopfl\n\tpush $0x4d1805\n\tret");
}
static int sailingTarget=120;
static unsigned sailingTicks=0,sailingExtras=0,sailingMutationFailures=0;
static long long sailingTickBegin=0,sailingPresented=0,sailingDeadline=0;
static FILE* sailingCsv=nullptr;
#ifdef PIRATES_TIMING_DIAGNOSTIC
#include "sailing-visual-probe.h"
#endif
using SailingCleanup=void(__attribute__((cdecl))*)(int);
static SailingCleanup originalSailingCleanup=nullptr;
extern "C" {static void* sailingQueueContinuation=nullptr;}
extern "C" bool __attribute__((cdecl)) sailingSkipQueue(){return sailingExtra;}
extern "C" void __attribute__((naked)) sailingQueueEntry(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tcall _sailingSkipQueue\n\ttest %al,%al\n\tjz 1f\n\tpopal\n\tpopfl\n\tpush $0x4d19f9\n\tret\n1:\n\tpopal\n\tpopfl\n\tjmp *_sailingQueueContinuation");
}
#include "sailing-transient.h"
#include "sailing-ui.h"
extern "C" void __attribute__((cdecl)) sailingReplayEffects(){
 if(sailingExtra)sailingUiReplay(targetDevice,false,true,true);
}
extern "C" void __attribute__((naked)) sailingEffectsFinish(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tcall _sailingReplayEffects\n\tpopal\n\tpopfl\n\tmovl (%edi),%edx\n\tmovl %edi,%ecx\n\tcall *0x38(%edx)\n\tpush $0x4d17d1\n\tret");
}
extern "C" void __attribute__((cdecl)) sailingReplayLabels(){
 if(sailingExtra)sailingUiReplay(targetDevice,true,true);
}
extern "C" void __attribute__((naked)) sailingLabelsFinish(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tcall _sailingReplayLabels\n\tpopal\n\tpopfl\n\tmovl (%edi),%edx\n\tmovl %edi,%ecx\n\tcall *0x38(%edx)\n\tpush $0x4d180c\n\tret");
}
static void sailingFinishCleanup(){
 sailingUiClear();sailingCleanupPending=false;
}
static void __attribute__((cdecl)) sailingCleanup(int argument){
 // Native cleanup also prepares the next text generation. Keep its original
 // timing; retain only our owned D3D drawing copies through the current wait.
 if(sailingFrameActive&&sailingSeparate&&sailingHealthy&&argument==0){
  originalSailingCleanup(argument);sailingCleanupPending=true;return;
 }
 sailingFinishCleanup();originalSailingCleanup(argument);
}
static unsigned sailingHash(const void* p,size_t bytes){
 auto data=static_cast<const unsigned char*>(p);unsigned hash=2166136261u;
 for(size_t i=0;i<bytes;++i)hash=(hash^data[i])*16777619u;
 return hash;
}
extern "C" void __attribute__((cdecl,noinline)) sailingTickMarker(){
 sailingFinishCleanup();sailingTickBegin=performanceTick();++sailingTicks;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 char diagnosticPath[MAX_PATH];std::snprintf(diagnosticPath,sizeof(diagnosticPath),"%stiming-test.ini",traceFolder);
 const int liveTarget=GetPrivateProfileIntA("Diagnostic","TargetFPS",sailingTarget,diagnosticPath);
 sailingCaptureOverlay=GetPrivateProfileIntA("Diagnostic","CaptureOverlay",1,diagnosticPath)!=0;
 if(liveTarget==30||liveTarget==60||liveTarget==120)sailingTarget=liveTarget;
#endif
 // City labels are emitted during the tick before the main frame function.
 sailingFrameActive=sailingSeparate&&sailingHealthy&&sailingTarget>=60;
}
extern "C" void __attribute__((naked)) sailingTickEntry(){
 __asm__ __volatile__("pushfl\n\tpushal\n\tcall _sailingTickMarker\n\tpopal\n\tpopfl\n\tpush $0x4741b0\n\tret");
}
static void __attribute__((cdecl)) sailingFrame(int a,int b,int c){
 sailingFrameActive=sailingSeparate&&sailingHealthy&&sailingTarget>=60;
 using Frame=void(__attribute__((cdecl))*)(int,int,int);
 reinterpret_cast<Frame>(0x4d0cd0)(a,b,c);
 sailingFrameActive=false;
}
static void __attribute__((cdecl)) sailingVisualNativeDraw(){
 // This call is after native scene/controller updates and before culling/drawing.
 // Capture the current pose first, render a delayed pose, then restore before
 // the native present, cleanup, or next gameplay update can observe it.
 if(sailingFrameActive&&sailingSeparate&&sailingHealthy){
#ifdef PIRATES_TIMING_DIAGNOSTIC
  sailingVisualCapture();sailingVisualBegin();
#endif
 }
 reinterpret_cast<void(__attribute__((cdecl))*)()>(0x4d1240)();
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(!sailingVisualUndo()){sailingHealthy=false;std::fprintf(journal,"SAILING visual restoration failed; experiment disabled\n");}
#endif
}
static int __attribute__((cdecl)) sailingAfterWait(const char* action){
 // This call executes once even when drawing exceeded the native wait.
 // Cleanup before input can leave sailing or start another scene.
 sailingFinishCleanup();
 using Input=int(__attribute__((cdecl))*)(const char*);
 return reinterpret_cast<Input>(0x42e1d0)(action);
}
static void beginSailingPresent(){
 if(!sailingSeparate||!sailingHealthy||!(sailingFrameActive||sailingExtra)||sailingTarget<=0){sailingDeadline=0;return;}
 const auto step=performanceFrequency.QuadPart/sailingTarget;
 const auto now=performanceTick();
 if(!sailingDeadline)sailingDeadline=sailingPresented?sailingPresented+step:now;
 else sailingDeadline+=step;
 if(now>sailingDeadline+step)sailingDeadline=now;
 while(performanceTick()<sailingDeadline){
  if(sailingDeadline-performanceTick()>performanceFrequency.QuadPart/500)Sleep(1);else SwitchToThread();
 }
}
static void endSailingPresent(HRESULT result){
 if(SUCCEEDED(result))sailingPresented=performanceTick();
 else sailingDeadline=0;
}
static void WINAPI sailingWait(DWORD milliseconds){
#ifdef PIRATES_TIMING_DIAGNOSTIC
 char diagnosticPath[MAX_PATH];std::snprintf(diagnosticPath,sizeof(diagnosticPath),"%stiming-test.ini",traceFolder);
 if(GetPrivateProfileIntA("Diagnostic","SkipExtra",0,diagnosticPath)){Sleep(milliseconds);return;}
#endif
 if(!sailingSeparate||!sailingHealthy||!sailingUiReady()||sailingTarget<60||!sailingPresented||!sailingCleanupPending||
    GetCurrentThreadId()!=renderThread.load()||!IsWindowVisible(gameWindow)||
    GetForegroundWindow()!=gameWindow||FAILED(targetDevice->TestCooperativeLevel())){
  Sleep(milliseconds);return;
 }
 const auto step=performanceFrequency.QuadPart/sailingTarget;
 const auto next=sailingDeadline?sailingDeadline+step:sailingPresented+step;
 if(next>sailingTickBegin+performanceFrequency.QuadPart*29/1000){Sleep(milliseconds);return;}
#ifdef PIRATES_TIMING_DIAGNOSTIC
 const unsigned sailingGuardShipCount=256;
#else
 const unsigned sailingGuardShipCount=16;
#endif
 const unsigned beforeShips=sailingHash(reinterpret_cast<void*>(0x8142f8),sailingGuardShipCount*0x45c);
 unsigned char clock[48];std::memcpy(clock,reinterpret_cast<void*>(0x8c8490),sizeof(clock));
 const auto beforeFlags=*reinterpret_cast<unsigned*>(0x85a164);
 const auto beforeRate=*reinterpret_cast<int*>(0x725684);
 const auto beforeDt=*reinterpret_cast<int*>(0x8b98c0);
 auto flags=reinterpret_cast<unsigned*>(0x7263bc);const auto renderFlags=*flags;
 // Keep transient quads visible; suppress the simulation/controller path.
 SailingTransientSnapshot transient;
 auto transientAges=reinterpret_cast<unsigned short*>(0x8c97d8);
 auto transientPointers=reinterpret_cast<unsigned*>(0x8c99d8);
 const bool redrawTransient=(renderFlags&0x800u)!=0;
 if(redrawTransient)transient.begin(transientAges,transientPointers);
 // Replay the captured UI without consuming labels queued for the next tick.
 *flags&=~0x1u;sailingExtra=true;
#ifdef PIRATES_TIMING_DIAGNOSTIC
 sailingVisualBegin();
#endif
 reinterpret_cast<void(__attribute__((cdecl))*)()>(0x4d1240)();
 const bool overlayReady=sailingUiReplay(targetDevice);
 using EnginePresent=void(__attribute__((thiscall))*)(void*);
 if(overlayReady)reinterpret_cast<EnginePresent>(0x535e10)(*reinterpret_cast<void**>(0x8e9fd8));
#ifdef PIRATES_TIMING_DIAGNOSTIC
 if(!sailingVisualUndo()){sailingHealthy=false;std::fprintf(journal,"SAILING visual restoration failed; experiment disabled\n");}
#endif
 if(redrawTransient)transient.undo(transientAges,transientPointers);
 sailingExtra=false;*flags=renderFlags;++sailingExtras;
 const auto afterShips=sailingHash(reinterpret_cast<void*>(0x8142f8),sailingGuardShipCount*0x45c);
 const bool clean=beforeShips==afterShips&&!std::memcmp(clock,reinterpret_cast<void*>(0x8c8490),sizeof(clock))&&
  beforeFlags==*reinterpret_cast<unsigned*>(0x85a164)&&beforeRate==*reinterpret_cast<int*>(0x725684)&&beforeDt==*reinterpret_cast<int*>(0x8b98c0);
 if(!clean){++sailingMutationFailures;sailingHealthy=false;
  std::fprintf(journal,"SAILING extra draw changed tracked gameplay state; disabled\n");std::fflush(journal);}
 if(sailingCsv){std::fprintf(sailingCsv,"%u,%.9f,%u,%d,%u,%u,%u,%d\n",frames.load(),
  double(performanceTick())/performanceFrequency.QuadPart,sailingTicks,sailingTarget,sailingExtras,beforeShips,afterShips,clean);
  if(sailingExtras%60==0||!clean)std::fflush(sailingCsv);}
 if(sailingExtras%600==1){std::fprintf(journal,"SAILING ticks=%u extraFrames=%u trackedMutationFailures=%u deferredCleanup=%d\n",sailingTicks,sailingExtras,sailingMutationFailures,sailingCleanupPending);std::fflush(journal);}
}
static bool sailingWrite(uintptr_t address,const void* bytes,size_t length){
 DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(address),length,PAGE_EXECUTE_READWRITE,&old))return false;
 std::memcpy(reinterpret_cast<void*>(address),bytes,length);DWORD ignored;
 VirtualProtect(reinterpret_cast<void*>(address),length,old,&ignored);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),length);return true;
}
static bool sailingPatchCall(uintptr_t address,size_t length,void* target,bool jump=false){
 unsigned char bytes[11];std::memset(bytes,0x90,sizeof(bytes));bytes[0]=static_cast<unsigned char>(jump?0xe9:0xe8);
 const int relative=int(intptr_t(target)-intptr_t(address+5));std::memcpy(bytes+1,&relative,4);
 return sailingWrite(address,bytes,length);
}
static void initSailing(){
 struct Site{uintptr_t address;size_t size;unsigned char expected[11];void* target;bool jump=false;};
 const Site sites[]={
  {0x4d1063,5,{0xe8,0xd8,1,0,0},reinterpret_cast<void*>(sailingVisualNativeDraw)},
  {0x472502,5,{0xe8,0xa9,0x1c,0,0},reinterpret_cast<void*>(sailingTickEntry)},
  {0x472675,5,{0xe8,0x56,0xe6,5,0},reinterpret_cast<void*>(sailingFrame)},
  {0x472897,6,{0xff,0x15,0x7c,1,0x6c,0},reinterpret_cast<void*>(sailingWait)},
  {0x4728a4,5,{0xe8,0x27,0xb9,0xfb,0xff},reinterpret_cast<void*>(sailingAfterWait)},
  {0x4d1794,10,{0x8b,0x16,0x50,0x8b,0xce,0xff,0x52,0x60,0xeb,0x1a},reinterpret_cast<void*>(sailingLabels),true},
  {0x4d17ca,7,{0x8b,0x17,0x8b,0xcf,0xff,0x52,0x38},reinterpret_cast<void*>(sailingEffectsFinish),true},
  {0x4d1800,5,{0xe8,0x5b,0x9f,3,0},reinterpret_cast<void*>(sailingTextEntry),true},
  {0x4d1805,7,{0x8b,0x17,0x8b,0xcf,0xff,0x52,0x38},reinterpret_cast<void*>(sailingLabelsFinish),true}};
 const unsigned char transientDraw[]={0x66,0x8b,0x45,0,0x66,0x85,0xc0,0x75,0x1f};
 const unsigned char transientAge[]={0x66,0xff,0x45,0,0x83,0xc3,4,0x83,0xc5,2,0x81,0xfb,0xd8,0x9d,0x8c,0};
 const unsigned char draw[]={0x8b,0x0d,0xd8,0x9f,0x8e,0};
 const unsigned char present[]={0x8b,0x89,0x88,1,0,0};
 const unsigned char cleanup[]={0x51,0x8b,0x0d,8,0xa0,0x8e,0};
 const unsigned char queue[]={0x8b,0x0d,0xd0,0x97,0x8c,0,0x8b,0x41,0x20};
 bool valid=!std::memcmp(reinterpret_cast<void*>(0x4d1776),transientDraw,sizeof(transientDraw))&&
  !std::memcmp(reinterpret_cast<void*>(0x4d19d1),queue,sizeof(queue))&&
  !std::memcmp(reinterpret_cast<void*>(0x4d17b8),transientAge,sizeof(transientAge))&&
  !std::memcmp(reinterpret_cast<void*>(0x4d1240),draw,sizeof(draw))&&
  !std::memcmp(reinterpret_cast<void*>(0x535e10),present,sizeof(present))&&
  !std::memcmp(reinterpret_cast<void*>(0x4d1130),cleanup,sizeof(cleanup));
#ifdef PIRATES_TIMING_DIAGNOSTIC
 const unsigned char cameraCache[]={0x83,0xec,0x1c,0xd9,0x81,0x1c,1,0,0};
 valid=valid&&!std::memcmp(reinterpret_cast<void*>(0x5366d0),cameraCache,sizeof(cameraCache));
#endif
 for(const auto& site:sites)valid=valid&&!std::memcmp(reinterpret_cast<void*>(site.address),site.expected,site.size);
 if(!valid){sailingHealthy=false;std::fprintf(journal,"SAILING signatures rejected; experiment disabled\n");return;}
 sailingHealthy=MH_CreateHook(reinterpret_cast<void*>(0x4d1130),reinterpret_cast<void*>(sailingCleanup),reinterpret_cast<void**>(&originalSailingCleanup))==MH_OK&&MH_EnableHook(reinterpret_cast<void*>(0x4d1130))==MH_OK;
 sailingHealthy=sailingHealthy&&MH_CreateHook(reinterpret_cast<void*>(0x4d19d1),reinterpret_cast<void*>(sailingQueueEntry),&sailingQueueContinuation)==MH_OK&&MH_EnableHook(reinterpret_cast<void*>(0x4d19d1))==MH_OK;
 unsigned changed=0;
 if(sailingHealthy){for(const auto& site:sites){if(!sailingPatchCall(site.address,site.size,site.target,site.jump)){sailingHealthy=false;break;}++changed;}}
 if(!sailingHealthy){for(unsigned i=0;i<changed;++i)sailingWrite(sites[i].address,sites[i].expected,sites[i].size);MH_DisableHook(reinterpret_cast<void*>(0x4d1130));MH_DisableHook(reinterpret_cast<void*>(0x4d19d1));}
#ifdef PIRATES_TIMING_DIAGNOSTIC
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%ssailing-decoupling.csv",traceFolder);
 sailingCsv=std::fopen(path,"w");if(sailingCsv)std::fprintf(sailingCsv,"frame,wall_seconds,simulation_ticks,target_fps,extra_frames,ships_before_hash,ships_after_hash,tracked_state_unchanged\n");
#endif
 std::fprintf(journal,"SAILING experimental hooks=%d enabled=%d target=%d cleanup=native-timing-owned-overlay\n",sailingHealthy,sailingSeparate,sailingTarget);std::fflush(journal);
}
