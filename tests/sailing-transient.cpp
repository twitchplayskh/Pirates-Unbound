#include <cassert>
#include <cstdio>
#include <cstring>
#include "../src/sailing-transient.h"
int main(){
 unsigned short ages[256];unsigned pointers[256];
 for(unsigned i=0;i<256;++i){ages[i]=static_cast<unsigned short>(i);pointers[i]=0x1000+4*i;}
 pointers[17]=0;
 SailingTransientSnapshot snapshot;snapshot.begin(ages,pointers);
 unsigned draws=0,destroys=0;
 // Mirror the native draw/expire/age branches, including empty slots.
 for(unsigned i=0;i<256;++i){
  if(pointers[i]){if(ages[i]==0)++draws;else if(ages[i]>5)++destroys;}
  ++ages[i];
 }
 assert(draws==1&&destroys==0);
 snapshot.undo(ages,pointers);
 for(unsigned i=0;i<256;++i){assert(ages[i]==i);assert(pointers[i]==(i==17?0:0x1000+4*i));}
 // Several presentations within one tick must have identical results.
 for(unsigned repeat=0;repeat<4;++repeat){snapshot.begin(ages,pointers);snapshot.undo(ages,pointers);}
 assert(ages[1]==1&&pointers[1]==0x1004&&ages[255]==255);
 std::puts("PASS: current-tick transient redraw, expired-entry suppression and exact restoration.");
}
