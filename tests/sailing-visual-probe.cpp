#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <cassert>
static LARGE_INTEGER performanceFrequency;
static long long performanceTick(){LARGE_INTEGER tick;QueryPerformanceCounter(&tick);return tick.QuadPart;}
static std::atomic<unsigned> frames{0};
static unsigned sailingTicks=0,sailingExtras=0;
static bool sailingExtra=true;
static FILE* journal=nullptr;
#include "../src/sailing-visual-probe.h"
struct TestNode {alignas(4) unsigned char bytes[0xc4]{};};
static void initialize(TestNode& node,uintptr_t table){
 *reinterpret_cast<uintptr_t*>(node.bytes)=table;
 for(int axis=0;axis<3;++axis){reinterpret_cast<float*>(node.bytes+0x28)[axis]=100.f+axis;reinterpret_cast<float*>(node.bytes+0x90)[axis]=200.f+axis;}
}
int main(){
 SailingCameraBasis basis;basis.valid=true;
 basis.axes[0]=basis.axes[4]=basis.axes[8]=1;
 basis.frustum[0]=-1;basis.frustum[1]=1;basis.frustum[2]=1;basis.frustum[3]=-1;basis.frustum[4]=1;basis.frustum[5]=1000;
 assert(sailingCameraContinuous(basis,basis));
 auto changed=basis;changed.axes[0]=0;changed.axes[1]=1;changed.axes[3]=-1;changed.axes[4]=0;
 assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.frustum[0]=-1.1f;assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.projection=1;assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.axes[0]=NAN;assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.axes[0]=1e30f;assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.axes[0]=0;assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.valid=false;assert(!sailingCameraContinuous(changed,basis));
 changed=basis;changed.axes[0]=changed.axes[4]=std::cos(0.01f);changed.axes[1]=std::sin(0.01f);changed.axes[3]=-changed.axes[1];
 assert(sailingCameraContinuous(changed,basis));
 assert(sailingVisualOwner(-1,0,1,1)==0);
 assert(sailingVisualOwner(-1,0,1,10)==0);
 assert(sailingVisualOwner(-1,0,2,1)==-1);
 assert(sailingVisualOwner(-1,7,1,1)==-1);
 assert(sailingVisualOwner(7,0,1,1)==7);
 assert(sailingVisualOwner(-1,0,0,-1)==-1);
 assert(sailingVisualOwner(-1,0,0,36)==-1);
 QueryPerformanceFrequency(&performanceFrequency);
 alignas(4) unsigned char camera[0x200]{};
 for(int axis=0;axis<3;++axis){reinterpret_cast<float*>(camera+0x5c)[axis]=100+axis;reinterpret_cast<float*>(camera+0x90)[axis]=200+axis;}
 for(int plane=0;plane<6;++plane){auto p=reinterpret_cast<float*>(camera+0x18c+plane*16);p[plane%3]=1;p[3]=10;}
 unsigned char cameraOriginal[sizeof(camera)];std::memcpy(cameraOriginal,camera,sizeof(camera));
 SailingCameraSnapshot cameraSnapshot;const float cameraDelta[]={10,-20,30};
 assert(cameraSnapshot.begin(camera,cameraDelta));assert(!cameraSnapshot.begin(camera,cameraDelta));
 assert(reinterpret_cast<float*>(camera+0x198)[0]==20); // plane 0 distance + delta X
 assert(reinterpret_cast<float*>(camera+0x1a8)[0]==-10); // plane 1 distance + delta Y
 std::memset(camera+0xb8,0xab,76); // engine cache rebuild must also roll back
 assert(cameraSnapshot.undo());assert(!std::memcmp(camera,cameraOriginal,sizeof(camera)));
 D3DMATRIX cameraView{};cameraView._11=cameraView._22=cameraView._33=cameraView._44=1;
 cameraView._41=4;cameraView._42=5;cameraView._43=6;
 const auto translatedView=sailingCameraView(cameraView,cameraDelta);
 assert(translatedView._41==-6&&translatedView._42==25&&translatedView._43==-24&&translatedView._44==1);
 // Moving the camera and a world point equally preserves its screen position.
 for(int axis=0;axis<3;++axis)assert(100+cameraDelta[axis]+translatedView.m[3][axis]==100+cameraView.m[3][axis]);
 // A delayed pose moves monotonically within its captured interval, never
 // predicts beyond the current pose, and is continuous at the next tick.
 SailingVisualPose previous,next;
 previous.actor=next.actor=reinterpret_cast<void*>(4);
 previous.root=next.root=reinterpret_cast<unsigned char*>(8);
 previous.owner=next.owner=35;
 previous.position[0]=100;next.position[0]=140;
 assert(sailingVisualPair(next,previous,1./30));
 float last=99;
 for(int i=0;i<=4;++i){const float alpha=sailingVisualAlpha(i/120.,1./30);
  const float displayed=next.previous[0]+(next.position[0]-next.previous[0])*alpha;
  assert(displayed>=last&&displayed>=100&&displayed<=140);last=displayed;}
 assert(last==140);assert(sailingVisualAlpha(1.,1./30)==1);
 previous=next;next.position[0]=180;next.paired=false;
 assert(sailingVisualPair(next,previous,1./30));assert(next.previous[0]==last);
 next.owner=36;assert(!sailingVisualPair(next,previous,1./30));next.owner=35;
 next.root=reinterpret_cast<unsigned char*>(12);assert(!sailingVisualPair(next,previous,1./30));next.root=previous.root;
 assert(!sailingVisualPair(next,previous,0.2));
 next.position[0]=1000;assert(!sailingVisualPair(next,previous,1./30));
 next.position[0]=NAN;assert(!sailingVisualPair(next,previous,1./30));
 assert(sailingVisualAlpha(-1,1./30)==1);assert(sailingVisualAlpha(0,NAN)==1);
 TestNode parent,child;initialize(parent,0x6c0bd8);initialize(child,0x6c1a00);
 unsigned char* children[]={child.bytes};
 *reinterpret_cast<unsigned char***>(parent.bytes+0xb8)=children;
 *reinterpret_cast<unsigned*>(parent.bytes+0xc0)=1;
 auto originalParent=parent;auto originalChild=child;
 const float delta[]={10.f,-20.f,0.f};
 assert(sailingVisualShift(parent.bytes,delta,0));assert(sailingVisualRestoreCount==2);
 assert(reinterpret_cast<float*>(child.bytes+0x90)[0]==210.f);
 assert(reinterpret_cast<float*>(child.bytes+0x28)[1]==81.f);
 D3DMATRIX matrix{};matrix._41=210;matrix._42=181;matrix._43=202;
 sailingVisualMatrix(&matrix);assert(sailingVisualMatrixMatches==1);
 assert(sailingVisualUndo());assert(!std::memcmp(parent.bytes,originalParent.bytes,sizeof(parent)));
 assert(!std::memcmp(child.bytes,originalChild.bytes,sizeof(child)));
 // Unsupported descendants must roll back already touched ancestors.
 *reinterpret_cast<uintptr_t*>(child.bytes)=0x12345678;originalChild=child;
 assert(!sailingVisualShift(parent.bytes,delta,0));assert(sailingVisualUndo());
 assert(!std::memcmp(parent.bytes,originalParent.bytes,sizeof(parent)));
 assert(!std::memcmp(child.bytes,originalChild.bytes,sizeof(child)));
 // Aliased/cyclic children and oversized arrays must fail without drift.
 children[0]=parent.bytes;
 assert(!sailingVisualShift(parent.bytes,delta,0));assert(sailingVisualUndo());
 assert(!std::memcmp(parent.bytes,originalParent.bytes,sizeof(parent)));
 *reinterpret_cast<unsigned*>(parent.bytes+0xc0)=129;originalParent=parent;
 assert(!sailingVisualShift(parent.bytes,delta,0));assert(sailingVisualUndo());
 assert(!std::memcmp(parent.bytes,originalParent.bytes,sizeof(parent)));
 assert(!sailingVisualShift(reinterpret_cast<unsigned char*>(1),delta,0));
 std::puts("PASS: interpolation continuity/clamping, cache reuse and teleport rejection, subtree translation, bounds, GPU matrix matching, exact rollback, unsupported nodes, cycles, invalid pointers and child limits.");
}
