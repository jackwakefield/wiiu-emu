#include "ios_fs_fsa_ipc.h"
#include "ios_fs_fsa_request.h"
#include "ios_fs_fsa_response.h"
#include "ios_fs_fsa_types.h"
#include "ios/ios_stackobject.h"
#include "ios/kernel/ios_kernel_heap.h"
#include "ios/kernel/ios_kernel_ipc.h"

namespace ios::fs
{

#pragma pack(push, 1)

struct FSAIpcData
{
   be2_struct<FSARequest> request;
   be2_struct<FSAResponse> response;
   be2_array<IoctlVec, 4> vecs;
   be2_val<FSACommand> command;
   be2_val<kernel::ResourceHandleId> resourceHandle;
   UNKNOWN(0x828 - 0x7EB);
};
CHECK_SIZE(FSAIpcData, 0x828);

#pragma pack(pop)

static FSAStatus
allocFsaIpcData(phys_ptr<FSAIpcData> *outIpcData)
{
   auto buffer = kernel::IOS_HeapAlloc(kernel::CrossProcessHeapId,
                                       sizeof(FSAIpcData));

   if (!buffer) {
      return FSAStatus::OutOfResources;
   }

   std::memset(buffer.getRawPointer(), 0, sizeof(FSAIpcData));
   *outIpcData = phys_cast<FSAIpcData>(buffer);
   return FSAStatus::OK;
}

static void
freeFsaIpcData(phys_ptr<FSAIpcData> ipcData)
{
   kernel::IOS_HeapFree(kernel::CrossProcessHeapId, ipcData);
}

FSAStatus
FSAOpenFile(kernel::ResourceHandleId resourceHandleId,
            std::string_view name,
            std::string_view mode,
            FSAFileHandle *outHandle)
{
   phys_ptr<FSAIpcData> ipcData;

   if (name.size() == 0) {
      return FSAStatus::InvalidPath;
   }

   if (mode.size() == 0) {
      return FSAStatus::InvalidParam;
   }

   if (!outHandle) {
      return FSAStatus::InvalidBuffer;
   }

   auto status = allocFsaIpcData(&ipcData);
   if (status < FSAStatus::OK) {
      return status;
   }

   auto request = phys_addrof(ipcData->request);
   std::strncpy(phys_addrof(request->openFile.path).getRawPointer(),
                name.data(),
                request->openFile.path.size());

   std::strncpy(phys_addrof(request->openFile.mode).getRawPointer(),
                mode.data(),
                request->openFile.mode.size());

   request->openFile.unk0x290 = 0x60000u;

   auto error = kernel::IOS_Ioctl(resourceHandleId,
                                  FSACommand::OpenFile,
                                  phys_addrof(ipcData->request),
                                  sizeof(FSARequest),
                                  phys_addrof(ipcData->response),
                                  sizeof(FSAResponse));

   auto response = phys_addrof(ipcData->response);
   *outHandle = response->openFile.handle;

   freeFsaIpcData(ipcData);
   return static_cast<FSAStatus>(error);
}

FSAStatus
FSACloseFile(kernel::ResourceHandleId resourceHandleId,
             FSAFileHandle fileHandle)
{
   phys_ptr<FSAIpcData> ipcData;

   auto status = allocFsaIpcData(&ipcData);
   if (status < FSAStatus::OK) {
      return status;
   }

   auto request = phys_addrof(ipcData->request);
   request->closeFile.handle = fileHandle;

   auto error = kernel::IOS_Ioctl(resourceHandleId,
                                  FSACommand::OpenFile,
                                  phys_addrof(ipcData->request),
                                  sizeof(FSARequest),
                                  phys_addrof(ipcData->response),
                                  sizeof(FSAResponse));

   freeFsaIpcData(ipcData);
   return static_cast<FSAStatus>(error);
}

FSAStatus
FSAReadFile(kernel::ResourceHandleId resourceHandleId,
            phys_ptr<void> buffer,
            uint32_t size,
            uint32_t count,
            FSAFileHandle fileHandle,
            FSAReadFlag readFlags)
{
   phys_ptr<FSAIpcData> ipcData;

   auto status = allocFsaIpcData(&ipcData);
   if (status < FSAStatus::OK) {
      return status;
   }

   auto request = phys_addrof(ipcData->request);
   request->readFile.handle = fileHandle;
   request->readFile.size = size;
   request->readFile.count = count;
   request->readFile.readFlags = readFlags;

   auto &vecs = ipcData->vecs;
   vecs[0].paddr = request;
   vecs[0].len = static_cast<uint32_t>(sizeof(FSARequest));

   vecs[1].paddr = buffer;
   vecs[1].len = size * count;

   vecs[2].paddr = phys_addrof(ipcData->response);
   vecs[2].len = static_cast<uint32_t>(sizeof(FSAResponse));

   auto error = kernel::IOS_Ioctlv(resourceHandleId,
                                   FSACommand::ReadFile,
                                   1u,
                                   2u,
                                   phys_addrof(ipcData->vecs));

   freeFsaIpcData(ipcData);
   return static_cast<FSAStatus>(error);
}

FSAStatus
FSAWriteFile(kernel::ResourceHandleId resourceHandleId,
             phys_ptr<const void> buffer,
             uint32_t size,
             uint32_t count,
             FSAFileHandle fileHandle,
             FSAWriteFlag writeFlags)
{
   phys_ptr<FSAIpcData> ipcData;

   auto status = allocFsaIpcData(&ipcData);
   if (status < FSAStatus::OK) {
      return status;
   }

   auto request = phys_addrof(ipcData->request);
   request->writeFile.handle = fileHandle;
   request->writeFile.size = size;
   request->writeFile.count = count;
   request->writeFile.writeFlags = writeFlags;

   auto &vecs = ipcData->vecs;
   vecs[0].paddr = request;
   vecs[0].len = static_cast<uint32_t>(sizeof(FSARequest));

   vecs[1].paddr = buffer;
   vecs[1].len = size * count;

   vecs[2].paddr = phys_addrof(ipcData->response);
   vecs[2].len = static_cast<uint32_t>(sizeof(FSAResponse));

   auto error = kernel::IOS_Ioctlv(resourceHandleId,
                                   FSACommand::WriteFile,
                                   2u,
                                   1u,
                                   phys_addrof(ipcData->vecs));

   freeFsaIpcData(ipcData);
   return static_cast<FSAStatus>(error);
}

} // namespace ios::fs