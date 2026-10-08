#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"Usage: inject.exe PID absolute-DLL-path\n");return 2;}
 DWORD pid=std::strtoul(argv[1],nullptr,10);
 HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
 if(!process){std::fprintf(stderr,"OpenProcess error %lu\n",GetLastError());return 3;}
 char path[MAX_PATH];DWORD len=sizeof(path);
 if(!QueryFullProcessImageNameA(process,0,path,&len)||_stricmp(strrchr(path,'\\')?strrchr(path,'\\')+1:path,"Pirates!.exe")){CloseHandle(process);return 4;}
 char dll[MAX_PATH]; if(!GetFullPathNameA(argv[2],MAX_PATH,dll,nullptr)||GetFileAttributesA(dll)==INVALID_FILE_ATTRIBUTES){CloseHandle(process);return 5;}
 HMODULE localKernel=GetModuleHandleA("kernel32.dll");
 auto load=GetProcAddress(localKernel,"LoadLibraryA");
 // Toolhelp supplies the remote 32-bit kernel module base; do not assume ASLR equality.
 MEMORY_BASIC_INFORMATION mbi{};VirtualQuery(reinterpret_cast<void*>(load),&mbi,sizeof(mbi));
 char owner[MAX_PATH];GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase),owner,MAX_PATH);
 std::string ownerName=strrchr(owner,'\\')+1;ULONG_PTR remoteBase=0;
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);MODULEENTRY32 me{};me.dwSize=sizeof(me);
 if(Module32First(snap,&me)){do{if(!_stricmp(me.szModule,ownerName.c_str()))remoteBase=reinterpret_cast<ULONG_PTR>(me.modBaseAddr);if(!_stricmp(me.szExePath,dll)){CloseHandle(snap);CloseHandle(process);std::fprintf(stderr,"Already loaded; restart game before rebuilding or reinjecting.\n");return 6;}}while(Module32Next(snap,&me));}CloseHandle(snap);
 if(!remoteBase){CloseHandle(process);return 7;}
 auto remoteLoad=reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteBase+reinterpret_cast<ULONG_PTR>(load)-reinterpret_cast<ULONG_PTR>(mbi.AllocationBase));
 void* mem=VirtualAllocEx(process,nullptr,std::strlen(dll)+1,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!mem||!WriteProcessMemory(process,mem,dll,std::strlen(dll)+1,nullptr)){CloseHandle(process);return 8;}
 HANDLE thread=CreateRemoteThread(process,nullptr,0,remoteLoad,mem,0,nullptr);
 if(!thread){VirtualFreeEx(process,mem,0,MEM_RELEASE);CloseHandle(process);return 9;}
 DWORD wait=WaitForSingleObject(thread,10000),result=0;
 if(wait==WAIT_OBJECT_0){GetExitCodeThread(thread,&result);VirtualFreeEx(process,mem,0,MEM_RELEASE);}
 CloseHandle(thread);CloseHandle(process);
 std::printf("LoadLibrary result=0x%lx wait=%lu; inspect PiratesWide.log for initialization status.\n",result,wait);
 return result?0:10;
}
