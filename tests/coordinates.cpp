#include "../src/widescreen.cpp"
#include <cassert>
int main(){
 const int sizes[][2]={{640,480},{1280,720},{1920,1080},{2560,1440},{3840,2160},{3440,1440}};
 unsigned checks=0;
 // Client coordinates can differ from rendering pixels in borderless mode.
 for(auto& render:sizes)for(int x:{0,480,1920,3360,3840})for(int y:{0,1080,2160}){
  const unsigned packet=(static_cast<unsigned>(y)<<16)|x;
  const auto scaled=scaleMousePacket(packet,render[0],render[1],3840,2160);
  assert(static_cast<short>(scaled)==x*render[0]/3840);
  assert(static_cast<short>(scaled>>16)==y*render[1]/2160);++checks;
 }
 assert(scaleMousePacket(0xfffefffeu,1920,1080,3840,2160)==0xffffffffu);++checks;
 assert(scaleMousePacket(123,1920,1080,0,0)==123);++checks;
 for(auto& size:sizes){
  width=size[0];height=size[1];const int w=size[0],h=size[1];
  // Generate a displayed position from each canonical UI position, then
  // run the actual input hook's inverse and the stock game's 640-unit picker.
  for(int canonical=0;canonical<=640;++canonical){
   const double displayed=w/2.0+(canonical-320)*(h/480.0);
   const int physical=static_cast<int>(std::round(displayed));
   const double picked=remapUiX(physical)*640.0/w;
   assert(std::fabs(picked-canonical)<=1.01);++checks;
   unsigned packet=(237u<<16)|(physical&65535u);
   assert((remapUiPacked(packet,0x201)>>16)==237);++checks;
   assert(remapUiPacked(packet,0x20a)==packet);++checks;
  }
  Frustum original{-0.8f,0.8f,0.6f,-0.6f,10,10000,0};
  const Frustum saved=original;
  auto fixed=*fixFrustum(&original);
  const float aspect=static_cast<float>(w)/h;
  assert(std::fabs((fixed.right-fixed.left)/(fixed.top-fixed.bottom)-aspect)<0.001f);
  assert(fixed.top==original.top&&fixed.bottom==original.bottom);
  assert(fixed.nearPlane==original.nearPlane&&fixed.farPlane==original.farPlane);
  assert(std::memcmp(&original,&saved,25)==0);
  // Applying the correction to an already widened frustum must be a no-op.
  auto twice=*fixFrustum(&fixed);assert(twice.left==fixed.left&&twice.right==fixed.right);
  Frustum ui{-1,1,0.75f,-0.75f,960,1600,0};assert(fixFrustum(&ui)==&ui);
  Frustum screen{-0.5f,0.5f,float(h)/(2*w),-float(h)/(2*w),921.6f,1536.f,0};
  const auto screenFixed=*fixFrustum(&screen);
  assert(std::fabs(screenFixed.top-0.375f)<0.0002f&&std::fabs(screenFixed.bottom+0.375f)<0.0002f);
  assert(std::fabs(screenFixed.right-3.f*float(w)/(8*h))<0.0002f);
  assert(screenFixed.nearPlane==screen.nearPlane&&screenFixed.farPlane==screen.farPlane);
  const auto screenTwice=*fixFrustum(&screenFixed);assert(screenTwice.left==screenFixed.left&&screenTwice.top==screenFixed.top);
  // Border-piece bounds high above the old widescreen culling limit remain
  // inside the corrected camera; the centre text keeps the same safe area.
  assert(screenFixed.top*1280>=236.f);checks+=5;
  original.orthographic=1;assert(fixFrustum(&original)==&original);checks+=7;
 }
 width=3840;height=2160;centerUi=false;widenWorld=false;
 for(int x: {0,480,1920,3360,3840}){assert(remapUiX(x)==x);++checks;}
 Frustum disabled{-0.8f,0.8f,0.6f,-0.6f,10,10000,0};
 assert(fixFrustum(&disabled)==&disabled);++checks;
 centerUi=true;widenWorld=true;
 for(auto& size:sizes){
  D3DVIEWPORT9 viewport{0,0,static_cast<DWORD>(size[0]),static_cast<DWORD>(size[1]),0,1};
  const auto safe=uiClipRect(size[0],size[1],viewport);
  assert(safe.left==(size[0]-4*size[1]/3)/2&&safe.right-safe.left==4*size[1]/3);++checks;
 }
 D3DVIEWPORT9 capture{0,0,512,512,0,1};auto safe=uiClipRect(3840,2160,capture);
 assert(safe.left==64&&safe.right==448&&safe.top==0&&safe.bottom==512);++checks;
 capture.X=37;capture.Y=19;safe=uiClipRect(3840,2160,capture);
 assert(safe.left==101&&safe.right==485&&safe.top==19&&safe.bottom==531);++checks;
 // A screen-space translation must preserve every projected pairwise distance,
 // including perspective planes at unequal depths, at arbitrary aspect ratios.
 for(auto& size:sizes){
  D3DMATRIX projection{};projection._11=2.59259f*size[1]/size[0];projection._22=2.59259f;
  projection._33=1.0001f;projection._34=1;projection._43=-1;
  float a[4]={-700,100,0,1500},b[4]={800,-100,0,1600},pa[4],pb[4],qa[4],qb[4];
  townMultiply(a,projection,pa);townMultiply(b,projection,pb);
  // Separate test uses genuine homogeneous points (local w=1, z=depth).
  a[2]=1500;a[3]=1;b[2]=1600;b[3]=1;
  townMultiply(a,projection,pa);townMultiply(b,projection,pb);
  const float shift=1-pb[0]/pb[3];const auto moved=townTranslatedProjection(projection,shift);
  townMultiply(a,moved,qa);townMultiply(b,moved,qb);
  assert(std::fabs(qb[0]/qb[3]-1)<0.00001f);
  assert(std::fabs((qb[0]/qb[3]-qa[0]/qa[3])-(pb[0]/pb[3]-pa[0]/pa[3]))<0.00001f);
  for(unsigned k=1;k<4;++k){assert(qa[k]==pa[k]);assert(qb[k]==pb[k]);checks+=2;}
  assert(std::memcmp(&projection,&moved,sizeof(projection))!=0);checks+=3;
 }
 std::printf("Passed %u coordinate/frustum/town checks, including 3840x2160, ultrawide and disabled options.\n",checks);
}
