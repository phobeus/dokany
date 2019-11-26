#include "MemoryFSOperations.h"
#include "FileNode.h"
#include "FileNodes.h"
#include "MemoryFSHelper.h"

#include <sddl.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <unordered_map>

static NTSTATUS DOKAN_CALLBACK
ZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext,
             ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess,
             ULONG CreateDisposition, ULONG CreateOptions,
             PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  ACCESS_MASK genericDesiredAccess;
  DWORD creationDisposition;
  DWORD fileAttributesAndFlags;

  DokanMapKernelToUserCreateFileFlags(
      DesiredAccess, FileAttributes, CreateOptions, CreateDisposition,
      &genericDesiredAccess, &fileAttributesAndFlags, &creationDisposition);

  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);

  spdlog::info(L"CreateFile: {} with node: {}", fileNameStr,
               (fileNode != nullptr));

  if (fileNode && fileNode->IsDirectory) {
    if (CreateOptions & FILE_NON_DIRECTORY_FILE)
      return STATUS_FILE_IS_A_DIRECTORY;
    DokanFileInfo->IsDirectory = true;
  }

  // TODO Use AccessCheck to check security rights

  if (DokanFileInfo->IsDirectory) {
    spdlog::info(L"CreateFile: {} is a Directory", fileNameStr);

    if (creationDisposition == CREATE_NEW ||
        creationDisposition == OPEN_ALWAYS) {
      if (fileNode) return STATUS_OBJECT_NAME_COLLISION;

      auto newfileNode = std::make_shared<FileNode>(
          fileNameStr, true, FILE_ATTRIBUTE_DIRECTORY, SecurityContext);
      return fileNodes->Add(newfileNode);
    }

    if (fileNode && !fileNode->IsDirectory) return STATUS_NOT_A_DIRECTORY;
    if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  } else {
    spdlog::info(L"CreateFile: {} is a File", fileNameStr);

    if (fileNode && (((!(fileAttributesAndFlags & FILE_ATTRIBUTE_HIDDEN) &&
                       (fileNode->Attributes & FILE_ATTRIBUTE_HIDDEN)) ||
                      (!(fileAttributesAndFlags & FILE_ATTRIBUTE_SYSTEM) &&
                       (fileNode->Attributes & FILE_ATTRIBUTE_SYSTEM))) &&
                     (creationDisposition == TRUNCATE_EXISTING ||
                      creationDisposition == CREATE_ALWAYS)))
      return STATUS_ACCESS_DENIED;

    if ((fileNode && (fileNode->Attributes & FILE_ATTRIBUTE_READONLY) ||
         (fileAttributesAndFlags & FILE_ATTRIBUTE_READONLY)) &&
        (fileAttributesAndFlags & FILE_FLAG_DELETE_ON_CLOSE))
      return STATUS_CANNOT_DELETE;

    // CREATE_NEW, CREATE_ALWAYS, or OPEN_ALWAYS
    // Combines the file attributes and flags specified by dwFlagsAndAttributes
    // with FILE_ATTRIBUTE_ARCHIVE All other file attributes override
    // FILE_ATTRIBUTE_NORMAL
    fileAttributesAndFlags &= ~FILE_ATTRIBUTE_NORMAL;
    fileAttributesAndFlags |= FILE_ATTRIBUTE_ARCHIVE;

    switch (creationDisposition) {
      case CREATE_ALWAYS: {
        spdlog::info(L"CreateFile: {} CREATE_ALWAYS", fileNameStr);
        /*
         * Creates a new file, always.
         */
        auto n = fileNodes->Add(std::make_shared<FileNode>(
            fileNameStr, false, fileAttributesAndFlags, SecurityContext));
        if (n != STATUS_SUCCESS) return n;

        /*
         * If the specified file exists and is writable, the function overwrites the file,
         * the function succeeds, and last-error code is set to ERROR_ALREADY_EXISTS
         */
        if (fileNode) return STATUS_OBJECT_NAME_COLLISION;
      } break;
      case CREATE_NEW: {
        spdlog::info(L"CreateFile: {} CREATE_ALWAYS", fileNameStr);
        /*
         * Creates a new file, only if it does not already exist.
         */
        if (fileNode) return STATUS_OBJECT_NAME_COLLISION;
        auto n = fileNodes->Add(std::make_shared<FileNode>(
            fileNameStr, false, fileAttributesAndFlags, SecurityContext));
        if (n != STATUS_SUCCESS) return n;
      } break;
      case OPEN_ALWAYS: {
        spdlog::info(L"CreateFile: {} OPEN_ALWAYS", fileNameStr);
        /*
         * Opens a file, always.
         */
        if (!fileNode) {
          auto n = fileNodes->Add(std::make_shared<FileNode>(
              fileNameStr, false, fileAttributesAndFlags, SecurityContext));
          if (n != STATUS_SUCCESS) return n;
        }
      } break;
      case OPEN_EXISTING: {
        spdlog::info(L"CreateFile: {} OPEN_EXISTING", fileNameStr);
        /*
         * Opens a file or device, only if it exists.
         * If the specified file or device does not exist, the function fails
         * and the last-error code is set to ERROR_FILE_NOT_FOUND
         */
        if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
      } break;
      case TRUNCATE_EXISTING: {
        spdlog::info(L"CreateFile: {} TRUNCATE_EXISTING", fileNameStr);
        /*
         * Opens a file and truncates it so that its size is zero bytes, only if
         * it exists. If the specified file does not exist, the function fails
         * and the last-error code is set to ERROR_FILE_NOT_FOUND
         */
        if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
        fileNode->setEndOfFile(0);
        fileNode->Attributes = FILE_ATTRIBUTE_ARCHIVE;
      } break;
    }
  }

  /*
   * CREATE_NEW && OPEN_ALWAYS
   * If the specified file exists, the function fails and the last-error code is
   * set to ERROR_FILE_EXISTS
   */
  if (fileNode &&
      (creationDisposition == CREATE_NEW || creationDisposition == OPEN_ALWAYS))
    return STATUS_OBJECT_NAME_COLLISION;

  return STATUS_SUCCESS;
}

static void DOKAN_CALLBACK Cleanup(LPCWSTR FileName,
                                   PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"Cleanup: {}", fileNameStr);
  if (DokanFileInfo->DeleteOnClose) {
    spdlog::info(L"\tDeleteOnClose: {}", fileNameStr);
    fileNodes->Remove(fileNameStr);
  }
}

static void DOKAN_CALLBACK CloseFile(LPCWSTR FileName,
                                     PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"CloseFile: {}", fileNameStr);
}

static NTSTATUS DOKAN_CALLBACK ReadFile(LPCWSTR FileName, LPVOID Buffer,
                                        DWORD BufferLength, LPDWORD ReadLength,
                                        LONGLONG Offset,
                                        PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"ReadFile: {}", fileNameStr);
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  *ReadLength = fileNode->Read(Buffer, BufferLength, Offset);
  spdlog::info(L"\tBufferLength: {} Offset: {} ReadLength: {}", BufferLength,
               Offset, *ReadLength);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK WriteFile(LPCWSTR FileName, LPCVOID Buffer,
                                         DWORD NumberOfBytesToWrite,
                                         LPDWORD NumberOfBytesWritten,
                                         LONGLONG Offset,
                                         PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"WriteFile: {}", fileNameStr);
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  auto fileSize = fileNode->getFileSize();

  if (DokanFileInfo->PagingIo) {
    if (Offset >= fileSize) {
      spdlog::info(L"\tPagingIo Outside Offset: {} FileSize: {}", Offset,
                   fileSize);
      *NumberOfBytesWritten = 0;
      return STATUS_SUCCESS;
    }

    if ((Offset + NumberOfBytesToWrite) > fileSize) {
      LONGLONG bytes = fileSize - Offset;
      if (bytes >> 32) {
        NumberOfBytesToWrite = static_cast<DWORD>(bytes & 0xFFFFFFFFUL);
      } else {
        NumberOfBytesToWrite = static_cast<DWORD>(bytes);
      }
    }
    spdlog::info(L"\tPagingIo NumberOfBytesToWrite: {}", NumberOfBytesToWrite);
  }

  *NumberOfBytesWritten = fileNode->Write(Buffer, NumberOfBytesToWrite, Offset);
  spdlog::info(L"\tNumberOfBytesToWrite {} Offset: {} NumberOfBytesWritten: {}",
               NumberOfBytesToWrite, Offset, *NumberOfBytesWritten);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
FlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"FlushFileBuffers: {}", fileNameStr);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
GetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer,
                   PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"GetFileInformation: {}", fileNameStr);
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  Buffer->dwFileAttributes = fileNode->Attributes;
  MemoryFSHelper::LlongToFileTime(fileNode->Times.Creation,
                                  Buffer->ftCreationTime);
  MemoryFSHelper::LlongToFileTime(fileNode->Times.LastAccess,
                                  Buffer->ftLastAccessTime);
  MemoryFSHelper::LlongToFileTime(fileNode->Times.LastWrite,
                                  Buffer->ftLastWriteTime);
  auto strLength = fileNode->getFileSize();
  MemoryFSHelper::LlongToDwLowHigh(strLength, Buffer->nFileSizeLow,
                                   Buffer->nFileSizeHigh);
  MemoryFSHelper::LlongToDwLowHigh(fileNode->FileIndex, Buffer->nFileIndexLow,
                                   Buffer->nFileIndexHigh);
  Buffer->nNumberOfLinks = 1;
  Buffer->dwVolumeSerialNumber = 0x19831116;

  spdlog::info(
      L"GetFileInformation: {} Attributes: {} Times: Creation {} "
      L"LastAccess {} LastWrite {} FileSize {} NumberOfLinks {} "
      L"VolumeSerialNumber {}",
      fileNameStr, fileNode->Attributes, fileNode->Times.Creation,
      fileNode->Times.LastAccess, fileNode->Times.LastWrite, strLength,
      Buffer->nNumberOfLinks, Buffer->dwVolumeSerialNumber);

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK FindFiles(LPCWSTR FileName,
                                         PFillFindData FillFindData,
                                         PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto files = fileNodes->ListFolder(fileNameStr);
  WIN32_FIND_DATAW findData;
  spdlog::info(L"FindFiles: {}", fileNameStr);
  ZeroMemory(&findData, sizeof(WIN32_FIND_DATAW));
  for (const auto& fileNode : files) {
    const auto fileNodeName = fileNode->getFileName();
    auto fileName = std::filesystem::path(fileNodeName).filename().wstring();
    if (fileName.length() > MAX_PATH) continue;
    std::copy(fileName.begin(), fileName.end(), std::begin(findData.cFileName));
    findData.cFileName[fileName.length()] = '\0';
    findData.dwFileAttributes = fileNode->Attributes;
    MemoryFSHelper::LlongToFileTime(fileNode->Times.Creation,
                                    findData.ftCreationTime);
    MemoryFSHelper::LlongToFileTime(fileNode->Times.LastAccess,
                                    findData.ftLastAccessTime);
    MemoryFSHelper::LlongToFileTime(fileNode->Times.LastWrite,
                                    findData.ftLastWriteTime);
    auto strLength = fileNode->getFileSize();
    MemoryFSHelper::LlongToDwLowHigh(strLength, findData.nFileSizeLow,
                                     findData.nFileSizeHigh);
    spdlog::info(
        L"FindFiles: {} fileNode: {} Attributes: {} Times: Creation {} "
        L"LastAccess {} LastWrite {} FileSize {}",
        fileNameStr, fileNodeName, findData.dwFileAttributes,
        fileNode->Times.Creation, fileNode->Times.LastAccess,
        fileNode->Times.LastWrite, strLength);
    FillFindData(&findData, DokanFileInfo);
  }
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK FindFilesWithPattern(
    LPCWSTR PathName, LPCWSTR SearchPattern, PFillFindData FillFindData,
    PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK SetFileAttributes(
    LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  spdlog::info(L"SetFileAttributes: {} FileAttributes {}", fileNameStr,
               FileAttributes);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  // No attributes need to be changed
  if (FileAttributes == 0) return STATUS_SUCCESS;

  // FILE_ATTRIBUTE_NORMAL is override if any other attribute is set
  if (FileAttributes & FILE_ATTRIBUTE_NORMAL &&
      (FileAttributes & (FileAttributes - 1)))
    FileAttributes &= ~FILE_ATTRIBUTE_NORMAL;

  fileNode->Attributes = FileAttributes;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK SetFileTime(LPCWSTR FileName,
                                           CONST FILETIME* CreationTime,
                                           CONST FILETIME* LastAccessTime,
                                           CONST FILETIME* LastWriteTime,
                                           PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  spdlog::info(L"SetFileTime: {}", fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  if (CreationTime && !FileTimes::isEmpty(CreationTime))
    fileNode->Times.Creation = MemoryFSHelper::FileTimeToLlong(*CreationTime);
  if (LastAccessTime && !FileTimes::isEmpty(LastAccessTime))
    fileNode->Times.LastAccess =
        MemoryFSHelper::FileTimeToLlong(*LastAccessTime);
  if (LastWriteTime && !FileTimes::isEmpty(LastWriteTime))
    fileNode->Times.LastWrite = MemoryFSHelper::FileTimeToLlong(*LastWriteTime);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK DeleteFile(LPCWSTR FileName,
                                          PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  spdlog::info(L"DeleteFile: {}", fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  if (fileNode->IsDirectory) return STATUS_ACCESS_DENIED;

  // Here prepare and check if the file can be deleted
  // or if delete is canceled when DokanFileInfo->DeleteOnClose false

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK DeleteDirectory(LPCWSTR FileName,
                                               PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"DeleteDirectory: {}", fileNameStr);

  if (fileNodes->ListFolder(fileNameStr).size())
    return STATUS_DIRECTORY_NOT_EMPTY;

  // Here prepare and check if the directory can be deleted
  // or if delete is canceled when DokanFileInfo->DeleteOnClose false

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK MoveFile(LPCWSTR FileName, LPCWSTR NewFileName,
                                        BOOL ReplaceIfExisting,
                                        PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto newFileNameStr = std::wstring(NewFileName);
  spdlog::info(L"MoveFile: {} to {}", fileNameStr, newFileNameStr);
  return fileNodes->Move(fileNameStr, newFileNameStr, ReplaceIfExisting);
}

static NTSTATUS DOKAN_CALLBACK SetEndOfFile(LPCWSTR FileName,
                                            LONGLONG ByteOffset,
                                            PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"SetEndOfFile: {} ByteOffset {}", fileNameStr, ByteOffset);
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  fileNode->setEndOfFile(ByteOffset);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK SetAllocationSize(
    LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"SetAllocationSize: {} AllocSize {}", fileNameStr, AllocSize);
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  fileNode->setEndOfFile(AllocSize);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK LockFile(LPCWSTR FileName, LONGLONG ByteOffset,
                                        LONGLONG Length,
                                        PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"LockFile: {} ByteOffset {} Length {}", fileNameStr, ByteOffset,
               Length);
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK UnlockFile(LPCWSTR FileName, LONGLONG ByteOffset,
                                          LONGLONG Length,
                                          PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"UnlockFile: {} ByteOffset {} Length {}", fileNameStr,
               ByteOffset, Length);
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK GetDiskFreeSpace(
    PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes,
    PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
  spdlog::info(L"GetDiskFreeSpace");
  *FreeBytesAvailable = (ULONGLONG)(512 * 1024 * 1024);
  *TotalNumberOfBytes = 9223372036854775807;
  *TotalNumberOfFreeBytes = 9223372036854775807;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
GetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize,
                     LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength,
                     LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer,
                     DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
  spdlog::info(L"GetVolumeInformation");
  wcscpy_s(VolumeNameBuffer, VolumeNameSize, L"Dokan MemFS");
  *VolumeSerialNumber = 0x19831116;
  *MaximumComponentLength = 255;
  *FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES |
                     FILE_SUPPORTS_REMOTE_STORAGE | FILE_UNICODE_ON_DISK;
  // FILE_PERSISTENT_ACLS | FILE_NAMED_STREAMS;

  wcscpy_s(FileSystemNameBuffer, FileSystemNameSize, L"NTFS");
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK Mounted(PDOKAN_FILE_INFO DokanFileInfo) {
  spdlog::info(L"Mounted");
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK Unmounted(PDOKAN_FILE_INFO DokanFileInfo) {
  spdlog::info(L"Unmounted");
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
GetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation,
                PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength,
                PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"GetFileSecurity: {}", fileNameStr);
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  std::lock_guard<std::mutex> lockFile(fileNode->Security);

  // This will make dokan library return a default security descriptor
  if (!fileNode->Security.Descriptor) return STATUS_NOT_IMPLEMENTED;

  // We have a Security Descriptor but we need to extract only informations requested
  // 1 - Convert the Security Descriptor to SDDL string with the informations requested
  LPTSTR pStringBuffer = NULL;
  if (!ConvertSecurityDescriptorToStringSecurityDescriptor(
          fileNode->Security.Descriptor, SDDL_REVISION_1, *SecurityInformation,
          &pStringBuffer, NULL)) {
    return STATUS_NOT_IMPLEMENTED;
  }

  // 2 - Convert the SDDL string back to Security Descriptor 
  PSECURITY_DESCRIPTOR SecurityDescriptorTmp = NULL;
  ULONG Size = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
          pStringBuffer, SDDL_REVISION_1, &SecurityDescriptorTmp, &Size)) {
    LocalFree(pStringBuffer);
    return STATUS_NOT_IMPLEMENTED;
  }
  LocalFree(pStringBuffer);

  *LengthNeeded = Size;
  if (Size > BufferLength) {
    LocalFree(SecurityDescriptorTmp);
    return STATUS_BUFFER_OVERFLOW;
  }

  // 3 - Copy the new SecurityDescriptor to destination
  memcpy(SecurityDescriptor, SecurityDescriptorTmp, Size);
  LocalFree(SecurityDescriptorTmp);

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
SetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation,
                PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength,
                PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"SetFileSecurity: {}", fileNameStr);
  static GENERIC_MAPPING MemFSMapping = {FILE_GENERIC_READ, FILE_GENERIC_WRITE,
                                         FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  std::lock_guard<std::mutex> securityLock(fileNode->Security);

  // SetPrivateObjectSecurity - ObjectsSecurityDescriptor
  // The memory for the security descriptor must be allocated from the process
  // heap (GetProcessHeap) with the HeapAlloc function.
  // https://devblogs.microsoft.com/oldnewthing/20170727-00/?p=96705
  HANDLE pHeap = GetProcessHeap();
  PSECURITY_DESCRIPTOR heapSecurityDescriptor =
      HeapAlloc(pHeap, 0, fileNode->Security.DescriptorSize);
  if (!heapSecurityDescriptor) return STATUS_INSUFFICIENT_RESOURCES;
  // Copy our current descriptor into heap memory
  memcpy(heapSecurityDescriptor, fileNode->Security.Descriptor,
         fileNode->Security.DescriptorSize);

  if (!SetPrivateObjectSecurity(*SecurityInformation, SecurityDescriptor,
                                &heapSecurityDescriptor, &MemFSMapping, 0)) {
    HeapFree(pHeap, 0, heapSecurityDescriptor);
    return DokanNtStatusFromWin32(GetLastError());
  }

  fileNode->Security.SetDescriptor(heapSecurityDescriptor);
  HeapFree(pHeap, 0, heapSecurityDescriptor);

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
FindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData,
            PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  spdlog::info(L"FindStreams: {}", fileNameStr);
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  return STATUS_NOT_IMPLEMENTED;
}

DOKAN_OPERATIONS MemoryFSOperations = {
    ZwCreateFile,
    Cleanup,
    CloseFile,
    ReadFile,
    WriteFile,
    FlushFileBuffers,
    GetFileInformation,
    FindFiles,
    nullptr,  // FindFilesWithPattern
    SetFileAttributes,
    SetFileTime,
    DeleteFile,
    DeleteDirectory,
    MoveFile,
    SetEndOfFile,
    SetAllocationSize,
    LockFile,
    UnlockFile,
    GetDiskFreeSpace,
    GetVolumeInformation,
    Mounted,
    Unmounted,
    GetFileSecurity,  // GetFileSecurity
    SetFileSecurity,  // SetFileSecurity
};
