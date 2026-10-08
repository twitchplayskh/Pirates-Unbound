// Render-only translation of the verified NiCamera layout. Rotation stays native.
struct SailingCameraSnapshot {
 unsigned char* camera=nullptr;
 unsigned char local[12]{},world[12]{},cache[76]{},planes[100]{};
 bool begin(unsigned char* value,const float* delta){
  if(camera)return false;
  camera=value;
  std::memcpy(local,value+0x5c,12);std::memcpy(world,value+0x90,12);
  std::memcpy(cache,value+0xb8,76);std::memcpy(planes,value+0x18c,100);
  for(int axis=0;axis<3;++axis){reinterpret_cast<float*>(value+0x5c)[axis]+=delta[axis];reinterpret_cast<float*>(value+0x90)[axis]+=delta[axis];}
  // Plane convention is dot(normal, position) - distance.
  for(int plane=0;plane<6;++plane){auto p=reinterpret_cast<float*>(value+0x18c+plane*16);
   p[3]+=p[0]*delta[0]+p[1]*delta[1]+p[2]*delta[2];}
  return true;
 }
 bool undo(){
  if(!camera)return true;
  std::memcpy(camera+0x5c,local,12);std::memcpy(camera+0x90,world,12);
  std::memcpy(camera+0xb8,cache,76);std::memcpy(camera+0x18c,planes,100);
  const bool exact=!std::memcmp(camera+0x5c,local,12)&&!std::memcmp(camera+0x90,world,12)&&!std::memcmp(camera+0xb8,cache,76)&&!std::memcmp(camera+0x18c,planes,100);
  camera=nullptr;return exact;
 }
};
static D3DMATRIX sailingCameraView(const D3DMATRIX& captured,const float* displacement){
 auto view=captured;
 for(int col=0;col<3;++col)for(int axis=0;axis<3;++axis)view.m[3][col]-=displacement[axis]*captured.m[axis][col];
 return view;
}
