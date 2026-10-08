#include "../src/fixture-write-guard.h"
#include <cstdio>
static DWORD WINAPI foreign(void* p){*reinterpret_cast<volatile unsigned*>(p)=19;return 0;}
int main(){auto* p=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!p)return 2;
 auto* x=reinterpret_cast<volatile unsigned*>(p+4092);*x=19;unsigned assertions=0;
 {FixtureWriteGuard g;if(!g.add(p+4088,16)||!g.begin())return 3;volatile unsigned read=*x;(void)read;if(!g.end()||g.violations)return 4;++assertions;}
 {FixtureWriteGuard g;g.add(p+4088,16);if(!g.begin())return 5;*x=19;if(g.end()||g.violations!=1||!g.first.tracked||!g.first.writer||*x!=19)return 6;++assertions;}
 {FixtureWriteGuard g;g.add(p+4088,16);if(!g.begin())return 7;p[4096]=7;if(g.end()||g.violations!=1||!g.first.tracked||p[4096]!=7)return 8;++assertions;}
 {FixtureWriteGuard g;g.add(p+4088,16);if(!g.begin())return 9;p[200]=1;if(g.end()||g.violations!=1||g.first.tracked)return 10;++assertions;}
 {FixtureWriteGuard g;g.add(p+4088,16);if(!g.begin())return 11;auto t=CreateThread(nullptr,0,foreign,const_cast<unsigned*>(x),0,nullptr);if(!t)return 12;WaitForSingleObject(t,5000);CloseHandle(t);if(g.end()||g.violations!=1||g.first.thread==g.ownerThread)return 13;++assertions;}
 MEMORY_BASIC_INFORMATION m{};VirtualQuery(p,&m,sizeof(m));if(m.Protect!=PAGE_READWRITE)return 14;++assertions;
 DWORD old=0;if(!VirtualProtect(p,8192,PAGE_EXECUTE_READWRITE,&old))return 15;
 {FixtureWriteGuard g;g.add(p+4088,16);if(!g.begin())return 16;VirtualQuery(p,&m,sizeof(m));if(m.Protect!=PAGE_EXECUTE_READ)return 17;*x=19;if(g.end()||g.violations!=1||!g.first.tracked)return 18;VirtualQuery(p,&m,sizeof(m));if(m.Protect!=PAGE_EXECUTE_READWRITE)return 19;++assertions;}
 VirtualFree(p,0,MEM_RELEASE);std::printf("write_guard_assertions=%u read_only_same_value_cross_page_ambiguous_foreign_thread_execute_restoration=PASS\n",assertions);return 0;}
