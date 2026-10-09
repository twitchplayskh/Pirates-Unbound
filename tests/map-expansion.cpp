#include "../src/widescreen.cpp"
#include <cassert>
static std::vector<unsigned char> fixture(){
 std::vector<unsigned char>b(1078+64*64);
 auto word=[&](unsigned p,WORD v){std::memcpy(b.data()+p,&v,2);};
 auto dword=[&](unsigned p,DWORD v){std::memcpy(b.data()+p,&v,4);};
 word(0,0x4d42);dword(10,1078);dword(14,40);dword(18,64);dword(22,64);word(26,1);word(28,8);
 for(unsigned i=0;i<256;++i)dword(54+4*i,i*0x010101);
 for(unsigned row=0;row<64;++row)for(unsigned x=0;x<64;++x)b[1078+row*64+x]=(row*3+x)&255;
 return b;
}
int main(){
 MapDecorationVertex original[4]={{-512,0,-256,0xffffffff,0,1},{512,0,-256,0xffffffff,1,1},{-512,0,256,0xffffffff,0,0},{512,0,256,0xffffffff,1,0}},mesh[12]{};
 for(float extra:{64.f,170.66667f,512.f}){
  assert(mapDecorationMesh(original,extra,mesh));
  assert(!std::memcmp(mesh+4,original,sizeof(original)));
  // Shared boundary vertices are identical; mirrored wings preserve UV density.
  for(unsigned row=0;row<2;++row){
   assert(!std::memcmp(&mesh[row*2+1],&original[row*2],sizeof(*mesh)));
   assert(!std::memcmp(&mesh[8+row*2],&original[row*2+1],sizeof(*mesh)));
   assert(std::fabs((mesh[row*2].u-mesh[row*2+1].u)/extra-1.f/1024.f)<1e-7f);
   assert(std::fabs(mesh[row*2].x-(original[row*2].x-extra))<1e-4f);
   assert(std::fabs(mesh[9+row*2].x-(original[row*2+1].x+extra))<1e-4f);
  }
 }
 assert(!mapDecorationMesh(original,0,mesh));assert(!mapDecorationMesh(original,513,mesh));
 original[2].u=.1f;assert(!mapDecorationMesh(original,100,mesh));original[2].u=0;
 original[1].z=0;assert(!mapDecorationMesh(original,100,mesh));
 MapBitmap m;m.bytes=fixture();assert(m.decode(2));
 // Check the native reverse-row convention with independently known texels.
 assert(m.pixel(5,64)==0xff050505);assert(m.pixel(5,63)==0xff080808);
 assert(m.pixel(63,1)==0xfffcfcfc);
 // Out-of-image reads use paper, never scanline wrap or invalid memory.
 for(int x:{-100000,-1,64,100000})for(int y:{-100000,0,65,100000})assert((m.pixel(x,y)&0xff000000)==0xff000000);
 auto good=fixture();
 for(unsigned position:{0u,10u,14u,18u,22u,26u,28u,30u}){
  m.bytes=good;m.bytes[position]^=0xff;assert(!m.decode(2));
 }
 m.bytes=good;m.bytes.pop_back();assert(!m.decode(2));
 m.bytes=good;assert(!m.decode(3));
 m.bytes.resize(100);assert(!m.decode(4));
 for(int zoom:{2,4,6}){m.bytes=good;assert(m.decode(zoom));assert(m.zoom==zoom);}
 std::puts("PASS: map palette, rows, chart edges, BMP rejection, zooms, unchanged decoration center, seamless mirrored joins and native UV density.");
}
