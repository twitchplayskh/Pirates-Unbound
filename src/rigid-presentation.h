#pragma once
// Value-only prerequisites. No engine pointers, native writes, clocks or rendering.
#include <array>
#include <cmath>
#include <cstdint>

namespace rigid {
struct Sphere { std::array<float,3> center{}; float radius=0; };
struct Plane { std::array<float,3> normal{}; float constant=0; };
enum class Pass { TranslatedWorld, OriginRelativeSky, WaterProducer, Interface };
struct Camera {
 std::array<float,3> position{}, direction{}, up{}, right{};
 std::array<float,6> frustum{};
 std::array<float,4> viewport{};
 std::array<float,16> view{}, projection{};
 std::array<Plane,6> planes{};
 std::uint64_t sample=0;
 Pass pass=Pass::TranslatedWorld;
};
struct Sample {
 std::array<float,13> transform{};
 Sphere bound{};
 std::uint64_t sample=0;
};
struct CullResult { bool valid=false, outside=true; unsigned mask=0; };
inline bool finite(const Sphere& b) {
 return std::isfinite(b.radius)&&b.radius>=0&&std::isfinite(b.center[0])&&
  std::isfinite(b.center[1])&&std::isfinite(b.center[2]);
}
inline CullResult cull(const std::array<Plane,6>& planes,const Sphere& bound,unsigned mask) {
 if(mask&~63u||!finite(bound))return {};
 for(const auto& p:planes) {
  if(!std::isfinite(p.constant))return {};
  for(float n:p.normal)if(!std::isfinite(n))return {};
 }
 // Native x87 evaluates z + x + y - constant, retaining extended precision.
 // This deliberately does not normalize planes or reorder the arithmetic.
 for(unsigned i=0;i<6;++i)if(mask&(1u<<i)) {
  const auto& p=planes[i];
  long double distance=static_cast<long double>(p.normal[2])*bound.center[2];
  distance+=static_cast<long double>(p.normal[0])*bound.center[0];
  distance+=static_cast<long double>(p.normal[1])*bound.center[1];
  distance-=p.constant;
  if(distance<=-static_cast<long double>(bound.radius))return {true,true,mask};
  if(distance>=static_cast<long double>(bound.radius))mask&=~(1u<<i);
 }
 return {true,false,mask};
}
// A native frame is copied once; transform and bound cannot carry separate IDs.
inline bool coherent(const Sample& object,const Camera& camera) {
 if(!object.sample||object.sample!=camera.sample||camera.pass!=Pass::TranslatedWorld||!finite(object.bound))return false;
 for(float f:object.transform)if(!std::isfinite(f))return false;
 return object.transform[12]>0;
}
enum class InvalidReason { Initial, Gap, EventLoss, ThreadMismatch, IdentityChange,
 Destruction, UnknownMode, ModeChange, SceneChange, Load, DeviceReset, BadSample };
// This is an invalidation mechanism, NOT a qualified native lifetime detector.
// Only a separately qualified observer may explicitly certify continuity.
struct HistoryGate {
 std::uint64_t epoch=1;
 bool valid=false;
 InvalidReason reason=InvalidReason::Initial;
 void invalidate(InvalidReason why) { ++epoch; valid=false; reason=why; }
 void accept(bool lifetimeQualified,bool modeQualified,bool completeSample) {
  valid=lifetimeQualified&&modeQualified&&completeSample;
  if(!completeSample)reason=InvalidReason::BadSample;
 }
};
}
