// Runs the extracted, verified native routine against private fixture buffers.
// This executable never opens Pirates!, hooks the game, or borrows native state.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include "../src/rigid-presentation.h"
#include "native-cull.inc"
#include "captured-cull.inc"
using NativeCull=bool(__attribute__((thiscall))*)(void*,void*);
static unsigned total=0, failures=0;
static void check(bool ok,const char* label) {
 ++total;
 if(!ok) { if(failures<12)std::printf("FAIL %s test=%u\n",label,total); ++failures; }
}
static void compare(NativeCull fn,const std::array<rigid::Plane,6>& planes,rigid::Sphere bound,unsigned mask) {
 alignas(16) unsigned char node[0x38]{},camera[0x1f0]{};
 std::memcpy(node+0x28,&bound,16);
 std::memcpy(camera+0x18c,planes.data(),96);
 std::memcpy(camera+0x1ec,&mask,4);
 auto beforePlanes=planes;auto beforeBound=bound;
 auto own=rigid::cull(planes,bound,mask);
 bool native=fn(node,camera);unsigned finalMask=0;
 std::memcpy(&finalMask,camera+0x1ec,4);
 check(own.valid&&own.outside==native&&own.mask==finalMask,"native visibility/mask");
 check(!std::memcmp(&beforePlanes,&planes,sizeof(planes))&&!std::memcmp(&beforeBound,&bound,sizeof(bound)),"owned input isolation");
}
int main() {
 static_assert(sizeof(rigid::Sphere)==16&&sizeof(rigid::Plane)==16,"native fixture layout");
 static_assert(sizeof(long double)==12,"requires MinGW x86 extended precision");
 void* code=VirtualAlloc(nullptr,sizeof(nativeCullBytes),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!code)return 2;
 std::memcpy(code,nativeCullBytes,sizeof(nativeCullBytes));DWORD old=0;
 if(!VirtualProtect(code,sizeof(nativeCullBytes),PAGE_EXECUTE_READ,&old))return 3;
 FlushInstructionCache(GetCurrentProcess(),code,sizeof(nativeCullBytes));
 auto fn=reinterpret_cast<NativeCull>(code);
 std::array<rigid::Plane,6> cube{{{{1,0,0},-100},{{-1,0,0},-100},{{0,1,0},-100},{{0,-1,0},-100},{{0,0,1},-100},{{0,0,-1},-100}}};
 for(unsigned mask=0;mask<64;++mask)for(float x:{-102.f,-101.f,-100.f,-99.f,-98.f,0.f,98.f,99.f,100.f,101.f,102.f})
  for(unsigned axis=0;axis<3;++axis) { rigid::Sphere b{{0,0,0},1};b.center[axis]=x;compare(fn,cube,b,mask); }
 // Cancellation at world coordinates, rotated/translated planes, partial masks.
 std::mt19937 random(0x533df0);std::uniform_real_distribution<float> unit(-1,1),world(-500000,500000),radius(0,500);
 for(unsigned test=0;test<40000;++test) {
  auto planes=cube;rigid::Sphere b{{world(random),world(random),world(random)},radius(random)};
  for(auto& p:planes) { p.normal={unit(random),unit(random),unit(random)};
   const long double dot=static_cast<long double>(p.normal[2])*b.center[2]+static_cast<long double>(p.normal[0])*b.center[0]+static_cast<long double>(p.normal[1])*b.center[1];
   p.constant=static_cast<float>(dot+unit(random)*b.radius*2); }
  compare(fn,planes,b,test%64);
 }
 auto tangent=rigid::cull(cube,{{101,0,0},1},63);check(tangent.outside,"external tangent rejected");
 for(const auto& fixture:capturedFixtures)for(unsigned mask=0;mask<64;++mask)compare(fn,fixture.planes,fixture.bound,mask);
 unsigned short savedControl=0;__asm__ __volatile__("fnstcw %0":"=m"(savedControl));
 for(unsigned precision:{0u,0x200u,0x300u}) {
  unsigned short control=static_cast<unsigned short>((savedControl&~0x300u)|precision);
  __asm__ __volatile__("fldcw %0"::"m"(control));
  for(const auto& fixture:capturedFixtures)for(unsigned mask=0;mask<64;++mask)compare(fn,fixture.planes,fixture.bound,mask);
  for(unsigned mask=0;mask<64;++mask)for(float x:{-101.f,-99.f,99.f,101.f})compare(fn,cube,{{x,0,0},1},mask);
 }
 __asm__ __volatile__("fldcw %0"::"m"(savedControl));
 auto inside=rigid::cull(cube,{{99,0,0},1},63);check(!inside.outside&&inside.mask==0,"internal tangent clears bit");
 auto bad=cube;bad[0].constant=std::numeric_limits<float>::quiet_NaN();
 check(!rigid::cull(bad,{{0,0,0},1},63).valid,"NaN fails closed");
 check(!rigid::cull(cube,{{0,0,0},-1},63).valid,"negative radius fails closed");
 check(!rigid::cull(cube,{{0,0,0},1},64).valid,"unknown mask fails closed");
 rigid::Camera camera;rigid::Sample sample;camera.sample=sample.sample=9;sample.transform[12]=1;
 check(rigid::coherent(sample,camera),"same sample");camera.sample=10;
 check(!rigid::coherent(sample,camera),"mixed samples rejected");camera.sample=9;camera.pass=rigid::Pass::WaterProducer;
 check(!rigid::coherent(sample,camera),"wrong pass rejected");
 for(auto why:{rigid::InvalidReason::Gap,rigid::InvalidReason::EventLoss,rigid::InvalidReason::ThreadMismatch,rigid::InvalidReason::IdentityChange,rigid::InvalidReason::Destruction,rigid::InvalidReason::UnknownMode,rigid::InvalidReason::ModeChange,rigid::InvalidReason::SceneChange,rigid::InvalidReason::Load,rigid::InvalidReason::DeviceReset,rigid::InvalidReason::BadSample}) {
  rigid::HistoryGate gate;gate.accept(true,true,true);auto epoch=gate.epoch;gate.invalidate(why);
  check(!gate.valid&&gate.epoch==epoch+1&&gate.reason==why,"invalidation clears history");
  gate.accept(false,true,true);check(!gate.valid,"uncertified lifetime stays native");
  gate.accept(true,false,true);check(!gate.valid,"uncertified mode stays native");
 }
 VirtualFree(code,0,MEM_RELEASE);
 std::printf("{\"assertions\":%u,\"failures\":%u,\"native_bytes\":%u,\"random_fixtures\":40000,\"boundary_fixtures\":2112}\n",total,failures,unsigned(sizeof(nativeCullBytes)));
 return failures?1:0;
}
