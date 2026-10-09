// Stock real-time cinematic masks, observed during new-game recruitment.
// These are overlay strips, not a cropped camera or a prerecorded movie.
struct CinematicBarVertex {float x,y,z;DWORD color;float u,v;};
static bool cinematicBarNear(float a,float b){return std::isfinite(a)&&std::fabs(a-b)<0.0002f;}
static bool cinematicBarGeometry(const CinematicBarVertex*vertices,const D3DMATRIX&world){
 // Exact unit transform with the native top/bottom placement. Reject other
 // black panels and all scaled, rotated or translated dialogue elements.
 for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
  if(r==3&&c==1)continue;
  if(r==3&&c==2){if(!cinematicBarNear(world.m[r][c],221)&&!cinematicBarNear(world.m[r][c],-222))return false;continue;}
  if(!cinematicBarNear(world.m[r][c],r==c?1.f:0.f))return false;
 }
 if(!std::isfinite(world._42)||world._42>0||world._42<-.05f)return false;
 unsigned corners=0;
 for(unsigned i=0;i<4;++i){const auto&v=vertices[i];
  if(!cinematicBarNear(std::fabs(v.x),321)||!cinematicBarNear(v.y,0)||!cinematicBarNear(std::fabs(v.z),20))return false;
  // Allow native alpha animation but only pure black diffuse colour.
  if(v.color&0x00ffffffu)return false;
  corners|=1u<<((v.x>0?1:0)+(v.z>0?2:0));
 }return corners==15;
}
