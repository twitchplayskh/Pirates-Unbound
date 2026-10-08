// Diagnostic boundary observation only. Native calls and waits run once.
struct StrictModeEvent {unsigned frame,kind,stage,generation,rawFlags,argument;LONGLONG qpc;uintptr_t caller,actor,root;bool worldScope;};
static std::vector<StrictModeEvent> strictModeEvents,strictModeBatch;
static unsigned strictModeGeneration=1,strictPlayerUpdateId=0;
static LONGLONG strictPlayerUpdateQpc=0;
static thread_local bool strictWorldScope=false;
static thread_local unsigned strictBoundaryArgument[4]{},strictBoundaryDepth[4]{};
static thread_local uintptr_t strictBoundaryCaller[4]{};
static void strictBoundaryEvent(unsigned kind,unsigned stage) {
 StrictModeEvent r{};r.frame=frames.load();r.kind=kind;r.stage=stage;r.generation=strictModeGeneration;
 r.rawFlags=*reinterpret_cast<unsigned*>(0x85a164);r.argument=strictBoundaryArgument[kind];r.caller=strictBoundaryCaller[kind];r.worldScope=strictWorldScope;
 auto c=obsCache(0);r.actor=c.actor;r.root=c.root;LARGE_INTEGER q{};QueryPerformanceCounter(&q);r.qpc=q.QuadPart;
 if(strictModeEvents.size()<128)strictModeEvents.push_back(r);else {++strictDrops;strictInvalidate(rigid::InvalidReason::EventLoss);}
}
extern "C" void __attribute__((cdecl)) strictBoundaryBefore(unsigned kind,unsigned argument,uintptr_t caller) {
 if(!obsSameThread()||kind>=4){strictInvalidate(rigid::InvalidReason::ThreadMismatch);return;}
 if(strictBoundaryDepth[kind]++){strictInvalidate(rigid::InvalidReason::Gap);return;}
 strictBoundaryArgument[kind]=argument;strictBoundaryCaller[kind]=caller;
 if(kind==0){++strictModeGeneration;strictInvalidate(rigid::InvalidReason::ModeChange);strictWorldScope=true;}
 if(kind==1||kind==2){++strictModeGeneration;strictInvalidate(rigid::InvalidReason::ModeChange);}
 if(kind!=3)strictBoundaryEvent(kind,0);
}
extern "C" void __attribute__((cdecl)) strictBoundaryAfter(unsigned kind) {
 if(kind>=4||!strictBoundaryDepth[kind]){strictInvalidate(rigid::InvalidReason::Gap);return;}
 if(--strictBoundaryDepth[kind])return;
 if(kind==3){
  // This is specifically the PLAYER update return at 472561, not proof of
  // completion of the entire world tick (other producers run afterwards).
  if(strictBoundaryCaller[kind]==0x472561&&strictBoundaryArgument[kind]==0){
   ++strictPlayerUpdateId;LARGE_INTEGER q{};QueryPerformanceCounter(&q);strictPlayerUpdateQpc=q.QuadPart;
  }
 }else{
  if(kind==0){strictWorldScope=false;++strictModeGeneration;strictInvalidate(rigid::InvalidReason::ModeChange);}
  strictBoundaryEvent(kind,1);
 }
}
// All four entries are verified cdecl(one argument), RET 0. Callbacks cannot
// consume arguments, change native registers/flags or leave altered FP state.
#define STRICT_BOUNDARY_WRAPPER(NAME,KIND) \
extern "C" {static void* __attribute__((used)) NAME##Trampoline=nullptr;} \
extern "C" void __attribute__((naked)) NAME(){__asm__ __volatile__( \
"pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tpushl 36(%ebp)\n\tpushl 40(%ebp)\n\tpushl $" #KIND "\n\tcall _strictBoundaryBefore\n\taddl $12,%esp\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tpushl 4(%esp)\n\tcall *_" #NAME "Trampoline\n\tleal 4(%esp),%esp\n\tpushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $544,%esp\n\tandl $-16,%esp\n\tfxsave (%esp)\n\tpushl $" #KIND "\n\tcall _strictBoundaryAfter\n\taddl $4,%esp\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tret");}
STRICT_BOUNDARY_WRAPPER(strictWorldModal,0)
STRICT_BOUNDARY_WRAPPER(strictShoreEnter,1)
STRICT_BOUNDARY_WRAPPER(strictShoreLeave,2)
STRICT_BOUNDARY_WRAPPER(strictPlayerUpdate,3)
#undef STRICT_BOUNDARY_WRAPPER
