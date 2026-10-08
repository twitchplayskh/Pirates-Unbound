// The native effect loop draws only age-zero entries, then increments their
// 16-bit ages. Extra presentations must redraw that tick's entries without
// aging or freeing them. These arrays live only on the extra draw's stack.
struct SailingTransientSnapshot {
 unsigned short ages[256];
 unsigned pointers[256];
 void begin(unsigned short* liveAges,unsigned* livePointers){
  std::memcpy(ages,liveAges,sizeof(ages));std::memcpy(pointers,livePointers,sizeof(pointers));
  for(unsigned i=0;i<256;++i){
   // Age one means the original draw just rendered the age-zero entry.
   // Hide all other ages so the native destruction path cannot run twice.
   livePointers[i]=ages[i]==1?pointers[i]:0;
   liveAges[i]=0;
  }
 }
 void undo(unsigned short* liveAges,unsigned* livePointers)const{
  std::memcpy(liveAges,ages,sizeof(ages));std::memcpy(livePointers,pointers,sizeof(pointers));
 }
};
