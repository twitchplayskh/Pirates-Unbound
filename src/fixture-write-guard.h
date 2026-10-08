#pragma once
#include <windows.h>
#include <cstdint>
#include <array>
// Process-wide CPU write trap for a bounded, synchronous diagnostic operation.
// No write is emulated. Any write (including the same value) fails the proof,
// restores native page permissions and resumes the existing native operation.
struct FixtureWriteGuard {
 struct Range {uintptr_t start;size_t bytes;};
 struct Page {uintptr_t start;DWORD old;bool protectedNow;DWORD readOnly;};
 struct Violation {uintptr_t address,writer;DWORD thread;bool tracked;};
 std::array<Range,16> ranges{};std::array<Page,32> pages{};
 unsigned rangeCount=0,pageCount=0;DWORD ownerThread=0;PVOID handler=nullptr;
 volatile LONG violations=0;Violation first{};bool armed=false,coverage=false,restoreFailed=false;
 static FixtureWriteGuard* volatile& current(){static FixtureWriteGuard* volatile g=nullptr;return g;}
 bool add(const void* p,size_t bytes){
  auto a=reinterpret_cast<uintptr_t>(p);if(!a||!bytes||a+bytes<a||rangeCount==ranges.size())return false;
  ranges[rangeCount++]={a,bytes};return true;
 }
 bool restore(){
  bool ok=true;for(unsigned i=0;i<pageCount;++i)if(pages[i].protectedNow){DWORD ignored=0;
   if(!VirtualProtect(reinterpret_cast<void*>(pages[i].start),4096,pages[i].old,&ignored))ok=false;
   else pages[i].protectedNow=false;
  }restoreFailed|=!ok;return ok;
 }
 static LONG CALLBACK exception(EXCEPTION_POINTERS* e){
  auto* g=current();auto* r=e->ExceptionRecord;
  if(!g||!g->armed||r->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION||r->NumberParameters<2||r->ExceptionInformation[0]!=1)return EXCEPTION_CONTINUE_SEARCH;
  auto at=uintptr_t(r->ExceptionInformation[1]);bool ours=false;
  for(unsigned i=0;i<g->pageCount;++i)if(g->pages[i].protectedNow&&at>=g->pages[i].start&&at<g->pages[i].start+4096)ours=true;
  if(!ours)return EXCEPTION_CONTINUE_SEARCH;
  if(InterlockedIncrement(&g->violations)==1){
   g->first.address=at;g->first.writer=reinterpret_cast<uintptr_t>(r->ExceptionAddress);g->first.thread=GetCurrentThreadId();
   for(unsigned i=0;i<g->rangeCount;++i)if(at>=g->ranges[i].start&&at<g->ranges[i].start+g->ranges[i].bytes)g->first.tracked=true;
  }
  g->coverage=false;
  return g->restore()?EXCEPTION_CONTINUE_EXECUTION:EXCEPTION_CONTINUE_SEARCH;
 }
 bool begin(){
  if(current()||!rangeCount)return false;
  SYSTEM_INFO s{};GetSystemInfo(&s);if(s.dwPageSize!=4096)return false;
  for(unsigned i=0;i<rangeCount;++i)for(auto page=ranges[i].start&~uintptr_t(4095);page<ranges[i].start+ranges[i].bytes;page+=4096){
   bool found=false;for(unsigned j=0;j<pageCount;++j)if(pages[j].start==page)found=true;if(found)continue;
   if(pageCount==pages.size())return false;
   MEMORY_BASIC_INFORMATION m{};
   if(!VirtualQuery(reinterpret_cast<void*>(page),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect!=PAGE_READWRITE&&m.Protect!=PAGE_EXECUTE_READWRITE))return false;
   pages[pageCount++]={page,m.Protect,false,DWORD(m.Protect==PAGE_EXECUTE_READWRITE?PAGE_EXECUTE_READ:PAGE_READONLY)};
  }
  handler=AddVectoredExceptionHandler(1,exception);if(!handler)return false;
  ownerThread=GetCurrentThreadId();current()=this;armed=true;
  for(unsigned i=0;i<pageCount;++i){DWORD old=0;
   if(!VirtualProtect(reinterpret_cast<void*>(pages[i].start),4096,pages[i].readOnly,&old)){end();return false;}
   pages[i].protectedNow=true;
   if(old!=pages[i].old||violations){pages[i].old=old;end();return false;}
  }coverage=true;return true;
 }
 bool end(){
  if(armed){for(unsigned i=0;i<pageCount;++i)if(pages[i].protectedNow){MEMORY_BASIC_INFORMATION m{};
   if(!VirtualQuery(reinterpret_cast<void*>(pages[i].start),&m,sizeof(m))||m.Protect!=pages[i].readOnly)coverage=false;
  }}
  bool ok=restore();armed=false;if(current()==this)current()=nullptr;
  if(handler){ok=RemoveVectoredExceptionHandler(handler)!=0&&ok;handler=nullptr;}
  return ok&&coverage&&violations==0&&!restoreFailed;
 }
 ~FixtureWriteGuard(){if(armed||handler)end();}
};
