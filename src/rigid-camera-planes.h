#pragma once
#include "rigid-presentation.h"
namespace rigid {
// Owned perspective-plane construction for the observed PC24, round-nearest
// branch only. Volatile spills preserve the native multiply/add ordering.
// This is not permission to use a sample from another FP or projection mode.
inline float planeMul(float a,float b){volatile float f=a*b;return f;}
inline float planeAdd(float a,float b){volatile float f=a+b;return f;}
inline float planeSub(float a,float b){volatile float f=a-b;return f;}
inline bool perspectivePlanes(Camera& c) {
 for(float f:c.position)if(!std::isfinite(f))return false;
 for(float f:c.frustum)if(!std::isfinite(f))return false;
 for(const auto* a:{&c.direction,&c.up,&c.right})for(float f:*a)if(!std::isfinite(f))return false;
 auto plane=[](const std::array<float,3>& n,const std::array<float,3>& p){
  Plane out{};out.normal=n;
  out.constant=planeAdd(planeAdd(planeMul(n[2],p[2]),planeMul(n[1],p[1])),planeMul(n[0],p[0]));return out;
 };
 for(unsigned side=0;side<2;++side){
  std::array<float,3> p{},n{};
  for(unsigned j=0;j<3;++j){p[j]=planeAdd(c.position[j],planeMul(c.frustum[4+side],c.direction[j]));n[j]=side?-c.direction[j]:c.direction[j];}
  c.planes[side]=plane(n,p);
 }
 for(unsigned side=0;side<4;++side){
  auto axis=side<2?c.right:c.up;auto crossAxis=side<2?c.up:c.right;
  if(side&1)for(float& f:crossAxis)f=-f;
  std::array<float,3> ray{},n{};
  for(unsigned j=0;j<3;++j)ray[j]=planeAdd(c.direction[j],planeMul(c.frustum[side],axis[j]));
  n[0]=planeSub(planeMul(ray[1],crossAxis[2]),planeMul(ray[2],crossAxis[1]));
  n[1]=planeSub(planeMul(ray[2],crossAxis[0]),planeMul(ray[0],crossAxis[2]));
  n[2]=planeSub(planeMul(ray[0],crossAxis[1]),planeMul(ray[1],crossAxis[0]));
  float squared=planeAdd(planeAdd(planeMul(n[0],n[0]),planeMul(n[1],n[1])),planeMul(n[2],n[2]));
  volatile float length=std::sqrt(squared);
  // The native normalizer's <= 1e-6 branch zeros the normal. Exclude that
  // degenerate branch rather than silently supplying a different plane.
  if(!std::isfinite(length)||length<=1.e-6f)return false;
  volatile float inverse=1.f/length;
  for(float& f:n)f=planeMul(f,inverse);
  c.planes[2+side]=plane(n,c.position);
 }
 for(const auto& p:c.planes){
  if(!std::isfinite(p.constant))return false;
  for(float n:p.normal)if(!std::isfinite(n))return false;
 }
 return true;
}
}
