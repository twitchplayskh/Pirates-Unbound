// Transient buffers cannot always be read back reliably. Observe the bytes
// supplied by the engine before Unlock, without issuing another buffer Lock.
struct SailingBufferCopy {
 IDirect3DVertexBuffer9* buffer=nullptr;
 std::vector<unsigned char> bytes;
 void* mapped=nullptr;
 unsigned begin=0,end=0,validBegin=0,validEnd=0;
};
static std::vector<SailingBufferCopy> sailingBuffers;
using SailingBufferLock=HRESULT(WINAPI*)(IDirect3DVertexBuffer9*,UINT,UINT,void**,DWORD);
using SailingBufferUnlock=HRESULT(WINAPI*)(IDirect3DVertexBuffer9*);
static SailingBufferLock sailingOriginalLock=nullptr;
static SailingBufferUnlock sailingOriginalUnlock=nullptr;
static SailingBufferCopy* sailingFindBuffer(IDirect3DVertexBuffer9* buffer){
 for(auto& copy:sailingBuffers)if(copy.buffer==buffer)return &copy;
 return nullptr;
}
static HRESULT WINAPI sailingBufferLock(IDirect3DVertexBuffer9* buffer,UINT offset,UINT size,void** data,DWORD flags){
 const HRESULT result=sailingOriginalLock(buffer,offset,size,data,flags);
 auto copy=sailingFindBuffer(buffer);
 if(copy&&SUCCEEDED(result)&&data&&*data&&!(flags&D3DLOCK_READONLY)){
  const unsigned count=size?size:(offset<=copy->bytes.size()?static_cast<unsigned>(copy->bytes.size())-offset:0);
  if(offset<=copy->bytes.size()&&count<=copy->bytes.size()-offset){
   copy->mapped=*data;copy->begin=offset;copy->end=offset+count;
   if(flags&D3DLOCK_DISCARD)copy->validBegin=copy->validEnd=0;
  }
 }
 return result;
}
static HRESULT WINAPI sailingBufferUnlock(IDirect3DVertexBuffer9* buffer){
 auto copy=sailingFindBuffer(buffer);
 if(copy&&copy->mapped){
  std::memcpy(copy->bytes.data()+copy->begin,copy->mapped,copy->end-copy->begin);
  copy->validBegin=copy->begin;copy->validEnd=copy->end;copy->mapped=nullptr;
 }
 return sailingOriginalUnlock(buffer);
}
static bool sailingObserveBuffer(IDirect3DVertexBuffer9* buffer,unsigned size){
 if(sailingFindBuffer(buffer))return true;
 size_t retained=0;for(const auto& copy:sailingBuffers)retained+=copy.bytes.size();
 if(sailingBuffers.size()>=64||size>4*1024*1024||retained+size>16*1024*1024){
  std::fprintf(journal,"SAILING buffer capture budget exceeded buffers=%u bytes=%u requested=%u\n",unsigned(sailingBuffers.size()),unsigned(retained),size);return false;
 }
 if(!sailingOriginalLock){
  auto table=*reinterpret_cast<void***>(buffer);
  if(MH_CreateHook(table[11],reinterpret_cast<void*>(sailingBufferLock),reinterpret_cast<void**>(&sailingOriginalLock))!=MH_OK)return false;
  if(MH_CreateHook(table[12],reinterpret_cast<void*>(sailingBufferUnlock),reinterpret_cast<void**>(&sailingOriginalUnlock))!=MH_OK){MH_RemoveHook(table[11]);sailingOriginalLock=nullptr;return false;}
  if(MH_EnableHook(table[11])!=MH_OK||MH_EnableHook(table[12])!=MH_OK){MH_DisableHook(table[11]);MH_DisableHook(table[12]);MH_RemoveHook(table[11]);MH_RemoveHook(table[12]);sailingOriginalLock=nullptr;sailingOriginalUnlock=nullptr;return false;}
 }
 // Retain the discovered buffer so its address cannot be reused by another
 // resource. Device reset releases these references through sailingDropBuffers.
 SailingBufferCopy copy;copy.buffer=buffer;buffer->AddRef();copy.bytes.resize(size);sailingBuffers.push_back(std::move(copy));return true;
}
static void sailingDropBuffers(){
 for(auto& copy:sailingBuffers)copy.buffer->Release();
 sailingBuffers.clear();
}
