// Development-only delayed translation interpolation. Never enabled by default.
// Native and extra draws blend the last two ship poses. Player/camera is
// separately gated; rotations, controllers and attached effects remain native.
static bool sailingVisualProbe=false;
static bool sailingVisualPlayerCamera=false;
#include "sailing-camera.h"
static bool sailingVisualRejected=false;
static unsigned sailingVisualRestoredDraws=0,sailingVisualRestoredNodes=0,sailingVisualMatrixMatches=0;
struct SailingVisualPose {void* actor=nullptr;unsigned char* root=nullptr;int owner=-1;float position[3]{},previous[3]{};bool paired=false;};
static SailingVisualPose sailingVisualPoses[20];
static SailingVisualPose sailingVisualCamera;
struct SailingCameraBasis {float axes[9]{},frustum[6]{};unsigned char projection=0;bool valid=false;};
static SailingCameraBasis sailingVisualCameraBasis;
static unsigned sailingVisualCameraCuts=0;
static bool sailingCameraContinuous(const SailingCameraBasis& next,const SailingCameraBasis& previous){
 if(!next.valid||!previous.valid||next.projection!=previous.projection)return false;
 // Translation blending cannot represent a view cut or a zoom change. Permit
 // small native rotations, but reset the paired pose for a >15 degree cut.
 for(int axis=0;axis<3;++axis){float dot=0,a=0,b=0;
  for(int c=0;c<3;++c){const float x=next.axes[axis*3+c],y=previous.axes[axis*3+c];
   if(!std::isfinite(x)||!std::isfinite(y))return false;
   dot+=x*y;a+=x*x;b+=y*y;}
  if(a<0.5f||a>1.5f||b<0.5f||b>1.5f||dot/std::sqrt(a*b)<0.9659258f)return false;
 }
 for(int i=0;i<6;++i){const float x=next.frustum[i],y=previous.frustum[i];
  if(!std::isfinite(x)||!std::isfinite(y)||std::fabs(x-y)>0.0001f*std::max(1.f,std::fabs(y)))return false;}
 return true;
}
static SailingCameraSnapshot sailingVisualCameraRestore;
static float sailingVisualCameraAdvance[3]{};
static unsigned sailingVisualCameraSamples=0,sailingVisualCameraPairs=0,sailingVisualCameraDraws=0;
static unsigned sailingVisualPlayerReports=0;
static long long sailingVisualSample=0;
static double sailingVisualInterval=0;
static FILE* sailingVisualCsv=nullptr;
// The engine's cache lookup (45aa23..45aa42) accepts previous ownership when
// current ownership has been cleared. Limit this fallback to the player and
// require its model type to match the native player ship record.
static int sailingVisualOwner(int current,int previous,int model,int playerType){
 return current==-1&&previous==0&&playerType>=0&&playerType<36&&model==playerType%9?0:current;
}
static int sailingVisualSlotOwner(int slot){
 const int current=*reinterpret_cast<int*>(0x8b97a8+slot*4);
 if(!sailingVisualPlayerCamera)return current;
 return sailingVisualOwner(current,*reinterpret_cast<int*>(0x8b9730+slot*4),
  *reinterpret_cast<int*>(0x8b9820+slot*4),*reinterpret_cast<short*>(0x8142f8));
}
static bool sailingVisualReadable(const void* p,size_t bytes){
 MEMORY_BASIC_INFORMATION m{};const auto a=reinterpret_cast<uintptr_t>(p);
 return a&&VirtualQuery(p,&m,sizeof(m))&&m.State==MEM_COMMIT&&
  !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&a+bytes>=a&&a+bytes<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
}
static bool sailingVisualPair(SailingVisualPose& next,const SailingVisualPose& previous,double dt){
 next.paired=false;
 if(next.actor!=previous.actor||next.root!=previous.root||next.owner!=previous.owner||!std::isfinite(dt)||dt<0.015||dt>0.08)return false;
 for(int axis=0;axis<3;++axis)
  if(!std::isfinite(next.position[axis])||!std::isfinite(previous.position[axis])||std::fabs(next.position[axis]-previous.position[axis])>=300)return false;
 std::memcpy(next.previous,previous.position,sizeof(next.previous));return next.paired=true;
}
static float sailingVisualAlpha(double age,double interval){
 if(!std::isfinite(age)||!std::isfinite(interval)||age<0||interval<0.015||interval>0.08)return 1.f;
 return float((age<interval?age:interval)/interval);
}
static void sailingVisualCapture(){
 if(!sailingVisualProbe)return;
 const auto now=performanceTick();const double dt=double(now-sailingVisualSample)/performanceFrequency.QuadPart;
 for(int slot=0;slot<20;++slot){
  SailingVisualPose next;next.actor=*reinterpret_cast<void**>(0x8b9870+slot*4);
  next.owner=sailingVisualSlotOwner(slot);
  if(slot==0&&sailingVisualPlayerCamera&&sailingVisualPlayerReports<5){
   ++sailingVisualPlayerReports;
   std::fprintf(journal,"SAILING player slot capture actor=%p owner=%d readable=%d dt=%.6f\n",next.actor,next.owner,sailingVisualReadable(next.actor,0x20),dt);std::fflush(journal);
  }
  if(!sailingVisualReadable(next.actor,0x20)||next.owner<0||(!sailingVisualPlayerCamera&&next.owner==0)||next.owner>=256){sailingVisualPoses[slot]={};continue;}
  next.root=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(next.actor)+0x1c);
  if(!sailingVisualReadable(next.root,0xc4)||*reinterpret_cast<uintptr_t*>(next.root)!=0x6c0bd8){sailingVisualPoses[slot]={};continue;}
  std::memcpy(next.position,next.root+0x90,sizeof(next.position));
  const auto& previous=sailingVisualPoses[slot];
  sailingVisualPair(next,previous,dt);
  sailingVisualPoses[slot]=next;
 }
 SailingVisualPose camera;
 SailingCameraBasis basis;
 if(sailingVisualPlayerCamera){
  ++sailingVisualCameraSamples;
  camera.root=*reinterpret_cast<unsigned char**>(0x8e9fd8);camera.actor=camera.root;camera.owner=0;
  if(sailingVisualReadable(camera.root,0x1f0)&&*reinterpret_cast<uintptr_t*>(camera.root)==0x70d8c0){
   std::memcpy(camera.position,camera.root+0x90,12);
   std::memcpy(basis.axes,camera.root+0x104,sizeof(basis.axes));
   std::memcpy(basis.frustum,camera.root+0x128,sizeof(basis.frustum));
   basis.projection=camera.root[0x140];basis.valid=true;
   if(sailingVisualPair(camera,sailingVisualCamera,dt)){
    if(sailingCameraContinuous(basis,sailingVisualCameraBasis))++sailingVisualCameraPairs;
    else{camera.paired=false;++sailingVisualCameraCuts;
     if(sailingVisualCameraCuts<=10){std::fprintf(journal,"SAILING camera discontinuity reset tick=%u cuts=%u\n",sailingTicks,sailingVisualCameraCuts);std::fflush(journal);}}
   }
  }else camera={};
 }
 sailingVisualCamera=camera;sailingVisualCameraBasis=basis;sailingVisualInterval=dt;sailingVisualSample=now;
}
struct SailingVisualRestore {unsigned char* node;unsigned char position[12],bound[12];};
static SailingVisualRestore sailingVisualRestore[2048];
static unsigned sailingVisualRestoreCount=0;
static bool sailingVisualShift(unsigned char* node,const float* delta,unsigned depth){
 if(!node)return true;
 if(depth>24||sailingVisualRestoreCount>=2048||!sailingVisualReadable(node,0xa0))return false;
 for(unsigned i=0;i<sailingVisualRestoreCount;++i)if(sailingVisualRestore[i].node==node)return false;
 const auto table=*reinterpret_cast<uintptr_t*>(node);
 // Observed NiNode, NiTriShape and NiTriStrips variants share this base.
 if(table!=0x6c0bd8&&table!=0x6c1a00&&table!=0x6c2478&&table!=0x6c1c88)return false;
 auto& saved=sailingVisualRestore[sailingVisualRestoreCount++];saved.node=node;
 std::memcpy(saved.position,node+0x90,12);std::memcpy(saved.bound,node+0x28,12);
 for(int axis=0;axis<2;++axis){reinterpret_cast<float*>(node+0x90)[axis]+=delta[axis];reinterpret_cast<float*>(node+0x28)[axis]+=delta[axis];}
 if(table==0x6c0bd8){
  if(!sailingVisualReadable(node,0xc4))return false;
  auto children=*reinterpret_cast<unsigned char***>(node+0xb8);const unsigned count=*reinterpret_cast<unsigned*>(node+0xc0);
  if(count>128||(count&&!sailingVisualReadable(children,count*4)))return false;
  for(unsigned child=0;child<count;++child)if(!sailingVisualShift(children[child],delta,depth+1))return false;
 }
 return true;
}
static bool sailingVisualUndo(){
 bool exact=sailingVisualCameraRestore.undo();
 std::memset(sailingVisualCameraAdvance,0,sizeof(sailingVisualCameraAdvance));
 if(sailingVisualRestoreCount){++sailingVisualRestoredDraws;sailingVisualRestoredNodes+=sailingVisualRestoreCount;}
 for(unsigned i=sailingVisualRestoreCount;i>0;--i){auto& saved=sailingVisualRestore[i-1];
  std::memcpy(saved.node+0x90,saved.position,12);std::memcpy(saved.node+0x28,saved.bound,12);
  exact=exact&&!std::memcmp(saved.node+0x90,saved.position,12)&&!std::memcmp(saved.node+0x28,saved.bound,12);
 }
 sailingVisualRestoreCount=0;return exact;
}
static void sailingVisualMatrix(const D3DMATRIX* matrix){
 if(!sailingVisualRestoreCount||!matrix)return;
 for(unsigned i=0;i<sailingVisualRestoreCount;++i){const auto p=reinterpret_cast<const float*>(sailingVisualRestore[i].node+0x90);
  if(std::fabs(p[0]-matrix->_41)<0.05f&&std::fabs(p[1]-matrix->_42)<0.05f&&std::fabs(p[2]-matrix->_43)<0.05f){++sailingVisualMatrixMatches;break;}
 }
}
static void sailingVisualBegin(){
 if(!sailingVisualProbe)return;
 const auto now=performanceTick();const double age=double(now-sailingVisualSample)/performanceFrequency.QuadPart;
 if(age<0||age>0.08)return;
 const float alpha=sailingExtra?sailingVisualAlpha(age,sailingVisualInterval):0.f;
 bool pairedPlayer=false;
 for(const auto& pose:sailingVisualPoses)if(pose.owner==0&&pose.paired)pairedPlayer=true;
 if(sailingVisualPlayerCamera&&pairedPlayer&&sailingVisualCamera.root&&sailingVisualCamera.paired&&
    *reinterpret_cast<unsigned char**>(0x8e9fd8)==sailingVisualCamera.root){
  float delta[3]{};
  for(int axis=0;axis<3;++axis){delta[axis]=(sailingVisualCamera.previous[axis]-sailingVisualCamera.position[axis])*(1-alpha);
   sailingVisualCameraAdvance[axis]=(sailingVisualCamera.position[axis]-sailingVisualCamera.previous[axis])*alpha;}
  if(!sailingVisualCameraRestore.begin(sailingVisualCamera.root,delta)){
   sailingVisualProbe=false;sailingVisualRejected=true;return;
  }
  ++sailingVisualCameraDraws;
  // This verified routine only rebuilds the cached camera matrix from its
  // existing axes/frustum/position; it never advances simulation/controllers.
  reinterpret_cast<void(__attribute__((thiscall))*)(void*)>(0x5366d0)(sailingVisualCamera.root);
 }
 for(int slot=0;slot<20;++slot){auto& pose=sailingVisualPoses[slot];if(!pose.root||!pose.paired)continue;
  if(pose.owner==0&&!sailingVisualCameraRestore.camera)continue;
  if(*reinterpret_cast<void**>(0x8b9870+slot*4)!=pose.actor||sailingVisualSlotOwner(slot)!=pose.owner)continue;
  float delta[3]={(pose.previous[0]-pose.position[0])*(1-alpha),(pose.previous[1]-pose.position[1])*(1-alpha),0};
  if(delta[0]==0&&delta[1]==0)continue;
  if(!sailingVisualShift(pose.root,delta,0)){sailingVisualUndo();sailingVisualProbe=false;sailingVisualRejected=true;
   std::fprintf(journal,"SAILING visual probe rejected unsupported subtree; disabled\n");return;}
  if(sailingVisualCsv)std::fprintf(sailingVisualCsv,"%u,%u,%d,%.9f,%.6f,%.6f,%.6f,%.6f,%u,%d,%.6f,%.9f\n",frames.load(),sailingTicks,pose.owner,age,pose.position[0],pose.position[1],delta[0],delta[1],sailingVisualRestoreCount,sailingExtra,alpha,sailingVisualInterval);
 }
 if(sailingVisualCsv&&sailingExtras%60==0)std::fflush(sailingVisualCsv);
 if(sailingExtras%600==0){std::fprintf(journal,"SAILING visual probe restoredDraws=%u restoredNodes=%u direct3DMatrixMatches=%u cameraSamples=%u cameraPairs=%u cameraDraws=%u\n",sailingVisualRestoredDraws,sailingVisualRestoredNodes,sailingVisualMatrixMatches,sailingVisualCameraSamples,sailingVisualCameraPairs,sailingVisualCameraDraws);std::fflush(journal);}
}
