// Local, opt-in CPU/frame measurements. No texture readbacks or GPU queries.
static bool profileEnabled=false;
static LARGE_INTEGER performanceFrequency{};
static long long performanceStart=0,previousPresent=0,quadReadTicks=0;
static unsigned measuredFrames=0,quadReads=0;
static double frameTimes[8192]{};
static std::atomic<DWORD> renderThread{0};
using GameSleep=void(WINAPI*)(DWORD);
static GameSleep originalGameSleep;
struct SleepSample{uintptr_t caller=0;unsigned calls=0;long long ticks=0;};
static SleepSample sleepSamples[8]{};
static long long performanceTick(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);return t.QuadPart;}
static void WINAPI measuredGameSleep(DWORD milliseconds){
 if(GetCurrentThreadId()!=renderThread.load()||!profileEnabled){originalGameSleep(milliseconds);return;}
 const auto caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));const auto begin=performanceTick();
 originalGameSleep(milliseconds);
 const auto elapsed=performanceTick()-begin;
 for(auto& sample:sleepSamples)if(!sample.caller||sample.caller==caller){sample.caller=caller;++sample.calls;sample.ticks+=elapsed;break;}
}
static bool installSleepMeasurements(){
 // Change this process's imported Sleep pointer only. Never hook other apps.
 auto slot=reinterpret_cast<GameSleep*>(0x6c017c);
 const auto systemSleep=reinterpret_cast<GameSleep>(GetProcAddress(GetModuleHandleA("kernel32.dll"),"Sleep"));
 if(!systemSleep||*slot!=systemSleep)return false;
 DWORD protection=0;if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&protection))return false;
 originalGameSleep=*slot;*slot=measuredGameSleep;DWORD ignored=0;VirtualProtect(slot,sizeof(*slot),protection,&ignored);return true;
}
static void recordPresent(){
 renderThread=GetCurrentThreadId();
 if(!profileEnabled){previousPresent=0;performanceStart=0;measuredFrames=0;quadReadTicks=0;quadReads=0;for(auto& sample:sleepSamples)sample={};return;}
 const auto now=performanceTick();
 if(!performanceStart)performanceStart=now;
 if(previousPresent&&measuredFrames<8192)frameTimes[measuredFrames++]=1000.0*(now-previousPresent)/performanceFrequency.QuadPart;
 previousPresent=now;
 const double seconds=double(now-performanceStart)/performanceFrequency.QuadPart;
 if(seconds<5.0&&measuredFrames<8192)return;
 if(measuredFrames){
  std::sort(frameTimes,frameTimes+measuredFrames);
  std::fprintf(journal,"PERF frames=%u seconds=%.3f fps=%.2f medianMs=%.3f p95Ms=%.3f p99Ms=%.3f quadReads=%u quadReadMs=%.3f trace=%d\n",measuredFrames,seconds,measuredFrames/seconds,frameTimes[measuredFrames/2],frameTimes[(measuredFrames-1)*95/100],frameTimes[(measuredFrames-1)*99/100],quadReads,1000.0*quadReadTicks/performanceFrequency.QuadPart,traceEnabled);
  std::fflush(journal);
  for(auto& sample:sleepSamples)if(sample.calls)std::fprintf(journal,"PERF_SLEEP caller=%08lx calls=%u elapsedMs=%.3f\n",static_cast<unsigned long>(sample.caller),sample.calls,1000.0*sample.ticks/performanceFrequency.QuadPart);
  std::fflush(journal);
 }
 performanceStart=now;measuredFrames=0;quadReadTicks=0;quadReads=0;for(auto& sample:sleepSamples)sample={};
}
