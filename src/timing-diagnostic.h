// Measurement harness, excluded from normal builds. No game clock rewriting.
// Rate controls are temporary experiments; never a supported high-FPS feature.
static FILE* timingCsv=nullptr;
static FILE* timingAnimationCsv=nullptr;
static FILE* timingShipCsv=nullptr;
static FILE* timingNavalCsv=nullptr;
static FILE* timingTextCsv=nullptr;
// Record the submitted camera matrices only in the bounded pixel window.
// This distinguishes a moving render camera from a lighting/effect mismatch.
static void timingCameraMatrix(D3DTRANSFORMSTATETYPE type,const D3DMATRIX* matrix){
 if(!sailingSeparate||(type!=D3DTS_VIEW&&type!=D3DTS_PROJECTION))return;
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%stiming-test.ini",traceFolder);
 const unsigned start=GetPrivateProfileIntA("Diagnostic","PixelStartTick",0,path);
 if(!start||sailingTicks<start||sailingTicks-start>5)return;
 static FILE* file=nullptr;
 if(!file){std::snprintf(path,sizeof(path),"%ssailing-camera-matrices.csv",traceFolder);file=std::fopen(path,"w");
  if(file){std::fprintf(file,"frame,tick,is_extra,type");for(int i=0;i<16;++i)std::fprintf(file,",m%d",i);std::fprintf(file,"\n");}}
 if(!file)return;
 std::fprintf(file,"%u,%u,%d,%u",frames.load(),sailingTicks,sailingExtra,unsigned(type));
 for(int i=0;i<16;++i)std::fprintf(file,",%.9g",reinterpret_cast<const float*>(matrix)[i]);
 std::fprintf(file,"\n");std::fflush(file);
}
// Point-sample eight scene regions into one small atlas before Present. Compare
// extra frames with their own native tick; readback is limited to six ticks.
static void timingHudPixels(IDirect3DDevice9* device){
 if(device!=targetDevice||!sailingSeparate)return;
 char probePath[MAX_PATH];std::snprintf(probePath,sizeof(probePath),"%stiming-test.ini",traceFolder);
 const unsigned start=GetPrivateProfileIntA("Diagnostic","PixelStartTick",100,probePath);
 if(!start||sailingTicks<start||sailingTicks-start>5)return;
 struct Region{const char* name;float left,top,right,bottom;};
 static const Region regions[]={
  {"buttons",.73f,.73f,.87f,.99f},{"fame",.13f,.67f,.31f,.99f},
  {"pause",.47f,.48f,.54f,.53f},{"player_ship",.40f,.40f,.58f,.85f},
  {"ship_label",.36f,.54f,.57f,.59f},{"santa_marta",.80f,.46f,.87f,.49f},
  {"player_water",.43f,.75f,.58f,.90f},{"water_control",.60f,.18f,.65f,.23f}};
 IDirect3DSurface9 *back=nullptr,*small=nullptr,*cpu=nullptr;D3DSURFACE_DESC desc{};
 bool ok=SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))&&SUCCEEDED(back->GetDesc(&desc));
 ok=ok&&(desc.Format==D3DFMT_A8R8G8B8||desc.Format==D3DFMT_X8R8G8B8);
 if(ok)ok=SUCCEEDED(device->CreateRenderTarget(128,64,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&small,nullptr))&&SUCCEEDED(device->CreateOffscreenPlainSurface(128,64,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr));
 for(unsigned i=0;ok&&i<8;++i){
  const auto& region=regions[i];RECT source{LONG(desc.Width*region.left),LONG(desc.Height*region.top),LONG(desc.Width*region.right),LONG(desc.Height*region.bottom)};
  RECT tile{LONG(i%4*32),LONG(i/4*32),LONG(i%4*32+32),LONG(i/4*32+32)};
  ok=SUCCEEDED(device->StretchRect(back,&source,small,&tile,D3DTEXF_POINT));
 }
 if(ok)ok=SUCCEEDED(device->GetRenderTargetData(small,cpu));
 static unsigned nativeTick=0;static std::vector<unsigned char> native[8];
 static FILE* file=nullptr;
 if(!file){char path[MAX_PATH];std::snprintf(path,sizeof(path),"%ssailing-hud-pixels.csv",traceFolder);file=std::fopen(path,"w");if(file)std::fprintf(file,"frame,tick,is_extra,region,ok,native_tick,different_rgb,total_rgb,absolute_rgb_error,sample_start,paused\n");}
 D3DLOCKED_RECT lock{};if(ok)ok=SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY));
 if(ok&&!sailingExtra)nativeTick=sailingTicks;
 for(unsigned i=0;i<8;++i){
  unsigned different=0,total=0,error=0;
  if(ok){std::vector<unsigned char> bytes(32*32*3);
   for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x)for(unsigned c=0;c<3;++c)bytes[(y*32+x)*3+c]=static_cast<unsigned char*>(lock.pBits)[(i/4*32+y)*lock.Pitch+(i%4*32+x)*4+c];
   if(!sailingExtra)native[i]=bytes;
   else if(nativeTick==sailingTicks&&native[i].size()==bytes.size()){total=bytes.size();for(unsigned j=0;j<total;++j){different+=bytes[j]!=native[i][j];error+=unsigned(std::abs(int(bytes[j])-int(native[i][j])));}}
  }
  if(file)std::fprintf(file,"%u,%u,%d,%s,%d,%u,%u,%u,%u,%u,%d\n",frames.load(),sailingTicks,sailingExtra,regions[i].name,ok,nativeTick,different,total,error,start,(*reinterpret_cast<unsigned*>(0x85a164)&4)!=0);
 }
 if(ok)cpu->UnlockRect();
 if(file)std::fflush(file);
 if(back)back->Release();
 if(small)small->Release();
 if(cpu)cpu->Release();
}
struct TimingAnimation { void* controller=nullptr;float input=0,phase=0,start=0,end=0,frequency=0;unsigned calls=0,lastFrame=0; };
static TimingAnimation timingAnimations[32]{};
using TimingKeyframe=void(__attribute__((thiscall))*)(void*,float);
static TimingKeyframe originalTimingKeyframe=nullptr;
static unsigned timingExtraControllerCalls=0;
static unsigned timingTransientCount(bool ageOne){
 auto ages=reinterpret_cast<unsigned short*>(0x8c97d8);auto pointers=reinterpret_cast<unsigned*>(0x8c99d8);unsigned count=0;
 for(unsigned i=0;i<256;++i)if(pointers[i]&&(!ageOne||ages[i]==1))++count;
 return count;
}
static void timingTextDraw(IDirect3DDevice9* device,const char* kind,unsigned primitives,void* caller){
 if(device!=targetDevice||sailingTicks<50||sailingTicks>55)return;
 static FILE* file=nullptr;
 if(!file){char path[MAX_PATH];std::snprintf(path,sizeof(path),"%ssailing-text-draws.csv",traceFolder);file=std::fopen(path,"w");
  if(file)std::fprintf(file,"frame,tick,is_extra,kind,primitives,ui,fvf,texture,caller,label_pass,target_width,target_height\n");}
 if(!file)return;
 DWORD fvf=0;device->GetFVF(&fvf);IDirect3DBaseTexture9* texture=nullptr;device->GetTexture(0,&texture);
 IDirect3DSurface9* surface=nullptr;D3DSURFACE_DESC size{};
 if(SUCCEEDED(device->GetRenderTarget(0,&surface))&&surface){surface->GetDesc(&size);surface->Release();}
 std::fprintf(file,"%u,%u,%d,%s,%u,%d,%lx,%p,%p,%d,%u,%u\n",frames.load(),sailingTicks,sailingExtra,kind,primitives,interfaceView,static_cast<unsigned long>(fvf),texture,caller,sailingLabelPass,size.Width,size.Height);
 if(texture)texture->Release();
 std::fflush(file);
}
static void __attribute__((fastcall)) timingKeyframe(void* controller,void*,float time){
 if(sailingExtra&&GetCurrentThreadId()==renderThread.load()){
  ++timingExtraControllerCalls;
  if(timingExtraControllerCalls<=10){std::fprintf(journal,"TIMING controller call during extra draw controller=%p caller=%p time=%g\n",controller,__builtin_return_address(0),time);std::fflush(journal);}
 }
 originalTimingKeyframe(controller,time);
 if(GetCurrentThreadId()!=renderThread.load())return;
 auto memory=static_cast<unsigned char*>(controller);
 if(!(*reinterpret_cast<unsigned short*>(memory+0xc)&8))return;
 for(auto& entry:timingAnimations)if(!entry.controller||entry.controller==controller){
  entry.controller=controller;entry.input=time;entry.phase=*reinterpret_cast<float*>(memory+0x2c);
  entry.start=*reinterpret_cast<float*>(memory+0x18);entry.end=*reinterpret_cast<float*>(memory+0x1c);entry.frequency=*reinterpret_cast<float*>(memory+0x10);entry.lastFrame=frames.load();++entry.calls;break;
 }
}
static unsigned timingFrames=0,timingUpdates=0;
static uintptr_t timingCaller=0;
static long long timingOrigin=0,timingPrevious=0,timingPresented=0,timingDeadline=0;
static int timingTarget=0;
static bool timingTavernCapsAvailable=false;
static long long timingFixtureBegin=0;
static int timingFixturePause=-1;
static int timingFixtureRunMs=0;
static void timingSetTavernCaps(bool bypass){
 // Naval combat's selectable 31/100 ms wait. Keep its computation intact;
 // compare the elapsed value with itself only during the measurement run.
 auto naval=reinterpret_cast<unsigned char*>(0x47f289);
 if(naval[0]==0x3b&&(naval[1]==0xca||naval[1]==0xc9)&&naval[2]==0x0f&&naval[3]==0x8d&&naval[4]==0xf1&&naval[5]==4&&naval[6]==0&&naval[7]==0){
  DWORD old=0;if(VirtualProtect(naval+1,1,PAGE_EXECUTE_READWRITE,&old)){
   naval[1]=bypass?0xc9:0xca;DWORD ignored;VirtualProtect(naval+1,1,old,&ignored);FlushInstructionCache(GetCurrentProcess(),naval+1,1);
  }
 }
 // Both paths belong to the same scene update family. Exact instruction checks
 // precede writes; the render thread edits immediates after returning from them.
 for(uintptr_t address:{uintptr_t(0x484b58),uintptr_t(0x485338)}){
  auto code=reinterpret_cast<unsigned char*>(address);
  if(code[0]!=0x83||code[1]!=0xf8||code[3]!=0x72||code[4]!=0xd3||(code[2]!=0x1e&&code[2]!=0))continue;
  DWORD protection=0;if(!VirtualProtect(code+2,1,PAGE_EXECUTE_READWRITE,&protection))continue;
  code[2]=bypass?0:0x1e;DWORD ignored;VirtualProtect(code+2,1,protection,&ignored);FlushInstructionCache(GetCurrentProcess(),code+2,1);
 }
 // Sailing loop: elapsed milliseconds, cmp 31 followed by Sleep(2) retry.
 // Governor loop: fixed Sleep(10). These are measurement-only local waits.
 for(uintptr_t address:{uintptr_t(0x472890),uintptr_t(0x43108f)}){
  auto code=reinterpret_cast<unsigned char*>(address);
  const bool sail=address==0x472890;
  const bool matches=sail?(code[0]==0x83&&code[1]==0xf8&&code[3]==0x7d&&code[4]==0x0a):
    (code[0]==0x6a&&code[2]==0xff&&code[3]==0x15&&code[4]==0x7c&&code[5]==1&&code[6]==0x6c&&code[7]==0);
  const unsigned offset=sail?2:1;const unsigned char native=sail?0x1f:0x0a;
  if(!matches||(code[offset]!=native&&code[offset]!=0))continue;
  DWORD protection=0;if(!VirtualProtect(code+offset,1,PAGE_EXECUTE_READWRITE,&protection))continue;
  code[offset]=bypass?0:native;DWORD ignored;VirtualProtect(code+offset,1,protection,&ignored);FlushInstructionCache(GetCurrentProcess(),code+offset,1);
 }
}
using TimingUpdate=void(__attribute__((cdecl))*)(int);
static TimingUpdate originalTimingUpdate=nullptr;
static void __attribute__((cdecl)) timingUpdate(int paused){
 originalTimingUpdate(paused);++timingUpdates;
 timingCaller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
}
static void initTimingDiagnostic(){
 char path[MAX_PATH];std::snprintf(path,sizeof(path),"%stiming.csv",traceFolder);
 timingCsv=std::fopen(path,"w");if(!timingCsv)return;
 std::snprintf(path,sizeof(path),"%ssailing-text.csv",traceFolder);timingTextCsv=std::fopen(path,"w");
 if(timingTextCsv)std::fprintf(timingTextCsv,"frame,tick,is_extra,transient_age_one,transient_entries,render_flags,extra_controller_calls,overlay_draws,overlay_replays\n");
 std::snprintf(path,sizeof(path),"%stiming-animation.csv",traceFolder);timingAnimationCsv=std::fopen(path,"w");
 std::snprintf(path,sizeof(path),"%stiming-ships.csv",traceFolder);timingShipCsv=std::fopen(path,"w");
 std::snprintf(path,sizeof(path),"%stiming-naval.csv",traceFolder);timingNavalCsv=std::fopen(path,"w");
 if(timingNavalCsv)std::fprintf(timingNavalCsv,"frame,wall_seconds,target_fps,flags,slot,x,y,heading,field_10,field_1c,field_3c,field_4c,field_5c\n");
 if(timingShipCsv)std::fprintf(timingShipCsv,"frame,wall_seconds,target_fps,sailing_flags,previous_dt_ms,world_rate,ship,x,y,heading,turn_input,speed_candidate,turn_rate_candidate\n");
 if(timingAnimationCsv)std::fprintf(timingAnimationCsv,"frame,wall_seconds,target_fps,controller,input_seconds,phase_seconds,start_seconds,end_seconds,frequency,update_calls\n");
 std::fprintf(timingCsv,"frame,wall_seconds,frame_ms,present_ms,target_fps,updates,timer_caller,game_seconds,scaled_delta,raw_delta,timer_counter,clock_mode,sailing_ticks,extra_frames,is_extra,mutation_failures\n");
 timingOrigin=performanceTick();
 const unsigned char cap[]={0x83,0xf8,0x1e,0x72,0xd3};
 timingTavernCapsAvailable=!std::memcmp(reinterpret_cast<void*>(0x484b58),cap,5)&&!std::memcmp(reinterpret_cast<void*>(0x485338),cap,5);
 std::fprintf(journal,"TIMING verified scene pacing immediates=%d; untouched until a test target is selected\n",timingTavernCapsAvailable);
 IDirect3DSwapChain9* swap=nullptr;D3DPRESENT_PARAMETERS p{};
 if(SUCCEEDED(targetDevice->GetSwapChain(0,&swap))){auto hr=swap->GetPresentParameters(&p);swap->Release();
  std::fprintf(journal,"TIMING parameters result=%lx windowed=%d interval=%lx swap=%d refresh=%u buffers=%u thread=%lu\n",static_cast<unsigned long>(hr),p.Windowed,static_cast<unsigned long>(p.PresentationInterval),p.SwapEffect,p.FullScreen_RefreshRateInHz,p.BackBufferCount,GetCurrentThreadId());}
 const unsigned char expected[]={0xe8,0xeb,0x1c,0,0};
 if(!std::memcmp(reinterpret_cast<void*>(0x4aafd0),expected,5)&&MH_CreateHook(reinterpret_cast<void*>(0x4aafd0),reinterpret_cast<void*>(timingUpdate),reinterpret_cast<void**>(&originalTimingUpdate))==MH_OK){
  auto hr=MH_EnableHook(reinterpret_cast<void*>(0x4aafd0));std::fprintf(journal,"TIMING timer hook=%d\n",hr);
 }
 const unsigned char animationExpected[]={0x83,0xec,0x10,0x56,0x57};
 if(!std::memcmp(reinterpret_cast<void*>(0x580770),animationExpected,5)&&MH_CreateHook(reinterpret_cast<void*>(0x580770),reinterpret_cast<void*>(timingKeyframe),reinterpret_cast<void**>(&originalTimingKeyframe))==MH_OK){auto hr=MH_EnableHook(reinterpret_cast<void*>(0x580770));std::fprintf(journal,"TIMING NiKeyframeController hook=%d\n",hr);}
 std::fflush(journal);
 initSailing();
}
static long long beginTimingPresent(){
 if(!timingCsv)return 0;
 if(!sailingExtra&&(timingFrames%30==0||sailingSeparate)){char path[MAX_PATH];std::snprintf(path,sizeof(path),"%stiming-test.ini",traceFolder);
  int next=GetPrivateProfileIntA("Diagnostic","TargetFPS",0,path);
  // Test fixture control uses the game's existing world pause flag. This is
  // not a time-step patch; exclude paused intervals from gameplay measures.
  const int pause=GetPrivateProfileIntA("Diagnostic","PauseState",-1,path);
  const bool separate=GetPrivateProfileIntA("Diagnostic","SailingSeparate",0,path)!=0;
  const bool visualRequested=GetPrivateProfileIntA("Diagnostic","SailingVisualProbe",0,path)!=0;
  sailingVisualPlayerCamera=GetPrivateProfileIntA("Diagnostic","SailingPlayerCamera",0,path)!=0;
  if(!visualRequested)sailingVisualRejected=false;
  const bool visual=visualRequested&&!sailingVisualRejected;
  if(visual!=sailingVisualProbe){sailingVisualProbe=visual;sailingVisualSample=0;
   for(auto& pose:sailingVisualPoses)pose={};
   if(visual&&!sailingVisualCsv){char visualPath[MAX_PATH];std::snprintf(visualPath,sizeof(visualPath),"%ssailing-visual-probe.csv",traceFolder);
    sailingVisualCsv=std::fopen(visualPath,"w");if(sailingVisualCsv)std::fprintf(sailingVisualCsv,"frame,tick,owner,forecast_seconds,x,y,offset_x,offset_y,nodes,is_extra,alpha,interval_seconds\n");}
  }
  if(separate!=sailingSeparate){sailingSeparate=separate;timingSetTavernCaps(false);timingTarget=-1;}
  timingFixtureRunMs=GetPrivateProfileIntA("Diagnostic","RunMilliseconds",0,path);
  if(pause!=timingFixturePause){timingFixturePause=pause;timingFixtureBegin=pause==0?performanceTick():0;}
  auto worldFlags=reinterpret_cast<unsigned*>(0x85a164);
  const bool ended=timingFixtureBegin&&timingFixtureRunMs>0&&performanceTick()-timingFixtureBegin>=performanceFrequency.QuadPart*timingFixtureRunMs/1000;
  if(pause==1||ended)*worldFlags|=4;else if(pause==0)*worldFlags&=~4u;
  int selected=(next==30||next==60||next==120||next==144||next==165||next==240)?next:0;
  if(selected!=timingTarget){timingTarget=selected;sailingTarget=selected;sailingDeadline=0;timingDeadline=0;if(timingTavernCapsAvailable)timingSetTavernCaps(!sailingSeparate&&selected!=0);std::fprintf(journal,"TIMING experiment target=%d scene pacing bypass=%d sailingSeparate=%d\n",selected,timingTavernCapsAvailable&&!sailingSeparate&&selected!=0,sailingSeparate);std::fflush(journal);}
 }
 // Slow the presentation boundary only. Never manufacture extra updates/frames.
 if(!sailingSeparate&&timingTarget&&timingPresented){const auto step=performanceFrequency.QuadPart/timingTarget;
  const auto current=performanceTick();
  if(!timingDeadline)timingDeadline=timingPresented+step;
  else timingDeadline+=step;
  // A slow mode is already pacing itself. Drop a missed deadline instead of
  // adding another complete frame of delay on top of its native waits.
  if(current>timingDeadline+step)timingDeadline=current;
  while(performanceTick()<timingDeadline){auto left=timingDeadline-performanceTick();if(left>performanceFrequency.QuadPart/500)Sleep(1);else SwitchToThread();}
 }
 return performanceTick();
}
static void endTimingPresent(long long begin,HRESULT result){
 if(!timingCsv||!begin)return;
 const auto now=performanceTick();timingPresented=now;
 if(timingTextCsv&&(sailingFrameActive||sailingExtra)){
  std::fprintf(timingTextCsv,"%u,%u,%d,%u,%u,%u,%u,%u,%u\n",timingFrames,sailingTicks,sailingExtra,timingTransientCount(true),timingTransientCount(false),*reinterpret_cast<unsigned*>(0x7263bc),timingExtraControllerCalls,static_cast<unsigned>(sailingUiDraws.size()),sailingUiReplays);
  if(timingFrames%60==0)std::fflush(timingTextCsv);
 }
 if(timingFrames%600==0){std::fprintf(journal,"TIMING call-time extra controller calls=%u\n",timingExtraControllerCalls);std::fflush(journal);}
 if(SUCCEEDED(result)&&timingFrames<200000){
  const double factor=1000.0/performanceFrequency.QuadPart;
  std::fprintf(timingCsv,"%u,%.9f,%.6f,%.6f,%d,%u,%08lx,%.9f,%.9f,%.9f,%u,%u,%u,%u,%d,%u\n",timingFrames,double(now-timingOrigin)/performanceFrequency.QuadPart,timingPrevious?(now-timingPrevious)*factor:0.,(now-begin)*factor,timingTarget,timingUpdates,static_cast<unsigned long>(timingCaller),*reinterpret_cast<double*>(0x8c84b0),*reinterpret_cast<double*>(0x8c8498),*reinterpret_cast<double*>(0x8c84a0),*reinterpret_cast<unsigned*>(0x8c84b8),*reinterpret_cast<unsigned*>(0x8c84bc),sailingTicks,sailingExtras,sailingExtra,sailingMutationFailures);
  if(timingFrames%60==0)std::fflush(timingCsv);
  if(timingAnimationCsv){for(auto& entry:timingAnimations)if(entry.calls){std::fprintf(timingAnimationCsv,"%u,%.9f,%d,%p,%.9f,%.9f,%.9f,%.9f,%.9f,%u\n",timingFrames,double(now-timingOrigin)/performanceFrequency.QuadPart,timingTarget,entry.controller,entry.input,entry.phase,entry.start,entry.end,entry.frequency,entry.calls);entry.calls=0;}if(timingFrames%60==0)std::fflush(timingAnimationCsv);}
  if(timingShipCsv){for(int ship=0;ship<16;++ship){const uintptr_t base=0x8142f8+ship*0x45c;if(*reinterpret_cast<short*>(base)==-1)continue;
   std::fprintf(timingShipCsv,"%u,%.9f,%d,%u,%d,%d,%d,%d,%d,%u,%d,%d,%d\n",timingFrames,double(now-timingOrigin)/performanceFrequency.QuadPart,timingTarget,*reinterpret_cast<unsigned*>(0x85a164),*reinterpret_cast<int*>(0x8b98c0),*reinterpret_cast<int*>(0x725684),ship,*reinterpret_cast<int*>(base+0xc),*reinterpret_cast<int*>(base+0x10),*reinterpret_cast<unsigned*>(base+0x14),*reinterpret_cast<short*>(base+0x24),*reinterpret_cast<int*>(base+0x18),*reinterpret_cast<int*>(base+0x1c));
  }if(timingFrames%60==0)std::fflush(timingShipCsv);}
  if(timingNavalCsv){for(int ship=0;ship<8;++ship){const uintptr_t base=0x8bc468+ship*0x4a8;
   std::fprintf(timingNavalCsv,"%u,%.9f,%d,%u,%d,%d,%d,%u,%d,%d,%d,%d,%d\n",timingFrames,double(now-timingOrigin)/performanceFrequency.QuadPart,timingTarget,*reinterpret_cast<unsigned*>(0x85a164),ship,*reinterpret_cast<int*>(base+4),*reinterpret_cast<int*>(base+8),*reinterpret_cast<unsigned*>(base+12),*reinterpret_cast<int*>(base+0x10),*reinterpret_cast<int*>(base+0x1c),*reinterpret_cast<int*>(base+0x3c),*reinterpret_cast<int*>(base+0x4c),*reinterpret_cast<int*>(base+0x5c));
  }if(timingFrames%60==0)std::fflush(timingNavalCsv);}
  for(auto& entry:timingAnimations)if(entry.controller&&frames.load()-entry.lastFrame>2)entry={};
 }
 timingPrevious=now;++timingFrames;timingUpdates=0;
}
