#include "MemoryFSOperations.h"
#include "FileNode.h"
#include "FileNodes.h"

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

  std::wcout << "CreateFile: [" << fileNameStr << "] with node ? "
             << (fileNode != nullptr) << std::endl;

  if (fileNode && fileNode->IsDirectory) {
    if (CreateOptions & FILE_NON_DIRECTORY_FILE)
      return STATUS_FILE_IS_A_DIRECTORY;
    DokanFileInfo->IsDirectory = true;
  }

  if (DokanFileInfo->IsDirectory) {
    std::wcout << "CreateFile: [" << fileNameStr << "] is Directory"
               << std::endl;

    if (creationDisposition == CREATE_NEW ||
        creationDisposition == OPEN_ALWAYS) {
      if (fileNode) return STATUS_OBJECT_NAME_COLLISION;

      auto newfileNode = std::make_shared<FileNode>(fileNameStr, true);
      return fileNodes->Add(newfileNode);
    }

    if (fileNode && !fileNode->IsDirectory) return STATUS_NOT_A_DIRECTORY;
    if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  } else {
    std::wcout << "CreateFile: [" << fileNameStr << "] is file" << std::endl;

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
        std::cout << "CREATE_ALWAYS" << std::endl;
        /*
         * Creates a new file, always.
         */
        auto n = fileNodes->Add(std::make_shared<FileNode>(
            fileNameStr, false, fileAttributesAndFlags, SecurityContext));
        if (n != STATUS_SUCCESS) return n;

        if (fileNode) return STATUS_OBJECT_NAME_COLLISION;
      } break;
      case CREATE_NEW: {
        std::cout << "CREATE_NEW" << std::endl;
        /*
         * Creates a new file, only if it does not already exist.
         */
        if (fileNode) return STATUS_OBJECT_NAME_COLLISION;
        auto n = fileNodes->Add(std::make_shared<FileNode>(
            fileNameStr, false, fileAttributesAndFlags, SecurityContext));
        if (n != STATUS_SUCCESS) return n;
      } break;
      case OPEN_ALWAYS: {
        std::cout << "OPEN_ALWAYS" << std::endl;
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
        std::cout << "OPEN_EXISTING" << std::endl;
        /*
         * Opens a file or device, only if it exists.
         * If the specified file or device does not exist, the function fails
         * and the last-error code is set to ERROR_FILE_NOT_FOUND
         */
        if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
      } break;
      case TRUNCATE_EXISTING: {
        std::cout << "TRUNCATE_EXISTING" << std::endl;
        /*
         * Opens a file and truncates it so that its size is zero bytes, only if
         * it exists. If the specified file does not exist, the function fails
         * and the last-error code is set to ERROR_FILE_NOT_FOUND
         */
        if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
        fileNode->setEndOfFile(0);
        fileNode->Attributes = FILE_ATTRIBUTE_ARCHIVE;
        // Todo reset fileNode buffer and attributes
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
  std::wcout << "Cleanup: " << fileNameStr << std::endl;
  if (DokanFileInfo->DeleteOnClose) {
    std::wcout << "\tDeleteOnClose: " << fileNameStr << std::endl;
    fileNodes->Remove(fileNameStr);
  }
}

static void DOKAN_CALLBACK CloseFile(LPCWSTR FileName,
                                     PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "CloseFile: " << fileNameStr << std::endl;
}

static NTSTATUS DOKAN_CALLBACK ReadFile(LPCWSTR FileName, LPVOID Buffer,
                                        DWORD BufferLength, LPDWORD ReadLength,
                                        LONGLONG Offset,
                                        PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "ReadFile: " << fileNameStr << std::endl;
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  *ReadLength = fileNode->Read(Buffer, BufferLength, Offset);
  std::wcout << "\t BufferLength: " << BufferLength << " Offset: " << Offset
             << " ReadLength: " << *ReadLength << std::endl;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK WriteFile(LPCWSTR FileName, LPCVOID Buffer,
                                         DWORD NumberOfBytesToWrite,
                                         LPDWORD NumberOfBytesWritten,
                                         LONGLONG Offset,
                                         PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "WriteFile: " << fileNameStr << std::endl;
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  auto fileSize = fileNode->getFileSize();

  if (DokanFileInfo->PagingIo) {
    if (Offset >= fileSize) {
      std::wcout << "\t PagingIo Outside Offset " << Offset
                 << " fileSize: " << fileSize << std::endl;
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
    std::wcout << "\t PagingIo NumberOfBytesToWrite: " << NumberOfBytesToWrite
               << std::endl;
  }

  *NumberOfBytesWritten = fileNode->Write(Buffer, NumberOfBytesToWrite, Offset);
  std::wcout << "\t NumberOfBytesToWrite: " << NumberOfBytesToWrite
             << " Offset: " << Offset
             << " NumberOfBytesWritten: " << *NumberOfBytesWritten << std::endl;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
FlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "FlushFileBuffers: " << fileNameStr << std::endl;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
GetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer,
                   PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "GetFileInformation: " << fileNameStr << std::endl;
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  Buffer->dwFileAttributes = fileNode->Attributes;
  Buffer->ftCreationTime = fileNode->Times.Creation;
  Buffer->ftLastAccessTime = fileNode->Times.LastAccess;
  Buffer->ftLastWriteTime = fileNode->Times.LastWrite;
  auto strLength = fileNode->getFileSize();
  Buffer->nFileSizeHigh = strLength >> 32;
  Buffer->nFileSizeLow = static_cast<DWORD>(strLength);
  std::wcout << "\tstrLength " << strLength
             << " FileSize: " << Buffer->nFileSizeHigh << Buffer->nFileSizeLow
             << std::endl;
  Buffer->nFileIndexHigh = fileNode->FileIndex >> 32;
  Buffer->nFileIndexLow = fileNode->FileIndex & 0xffffffff;

  // Improve this
  Buffer->nNumberOfLinks = 0;
  // Buffer->dwVolumeSerialNumber;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK FindFiles(LPCWSTR FileName,
                                         PFillFindData FillFindData,
                                         PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto files = fileNodes->ListFolder(fileNameStr);
  WIN32_FIND_DATAW findData;
  std::wcout << "FindFiles: " << fileNameStr << std::endl;
  ZeroMemory(&findData, sizeof(WIN32_FIND_DATAW));
  for (const auto& fileNode : files) {
    const auto fileNodeName = fileNode->getFileName();
    std::wcout << "FindFiles: " << fileNameStr << " fileNode " << fileNodeName
               << std::endl;
    auto fileName = std::filesystem::path(fileNodeName).filename().wstring();
    if (fileName.length() > MAX_PATH) continue;
    std::copy(fileName.begin(), fileName.end(), std::begin(findData.cFileName));
    findData.dwFileAttributes = fileNode->Attributes;
    findData.ftCreationTime = fileNode->Times.Creation;
    findData.ftLastAccessTime = fileNode->Times.LastAccess;
    findData.ftLastWriteTime = fileNode->Times.LastWrite;
    auto strLength = fileNode->getFileSize();
    findData.nFileSizeHigh = strLength >> 32;
    findData.nFileSizeLow = static_cast<DWORD>(strLength);
    std::wcout << "\tstrLength " << strLength
               << " FileSize: " << findData.nFileSizeHigh
               << findData.nFileSizeLow << std::endl;
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
  std::wcout << "SetFileAttributes: " << fileNameStr << " " << FileAttributes
             << std::endl;
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

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
  std::wcout << "SetFileTime: " << fileNameStr << std::endl;
  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  if (CreationTime && !FileTimes::isEmpty(CreationTime))
    fileNode->Times.Creation = *CreationTime;
  if (LastAccessTime && !FileTimes::isEmpty(LastAccessTime))
    fileNode->Times.LastAccess = *LastAccessTime;
  if (LastWriteTime && !FileTimes::isEmpty(LastWriteTime))
    fileNode->Times.LastWrite = *LastWriteTime;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK DeleteFile(LPCWSTR FileName,
                                          PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  std::wcout << "DeleteFile: " << fileNameStr << std::endl;

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  if (fileNode->IsDirectory) return STATUS_ACCESS_DENIED;

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK DeleteDirectory(LPCWSTR FileName,
                                               PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "DeleteDirectory: " << fileNameStr << std::endl;

  if (fileNodes->ListFolder(fileNameStr).size())
    return STATUS_DIRECTORY_NOT_EMPTY;

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK MoveFile(LPCWSTR FileName, LPCWSTR NewFileName,
                                        BOOL ReplaceIfExisting,
                                        PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto newFileNameStr = std::wstring(NewFileName);
  std::wcout << "DeleteDirectory: " << fileNameStr << " to " << newFileNameStr
             << std::endl;
  return fileNodes->Move(fileNameStr, newFileNameStr, ReplaceIfExisting);
}

static NTSTATUS DOKAN_CALLBACK SetEndOfFile(LPCWSTR FileName,
                                            LONGLONG ByteOffset,
                                            PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "SetEndOfFile: " << fileNameStr << " ByteOffset: " << ByteOffset
             << std::endl;
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;
  fileNode->setEndOfFile(ByteOffset);
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK SetAllocationSize(
    LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "SetAllocationSize: " << fileNameStr
             << " AllocSize: " << AllocSize << std::endl;
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
  std::wcout << "LockFile: " << fileNameStr << std::endl;
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK UnlockFile(LPCWSTR FileName, LONGLONG ByteOffset,
                                          LONGLONG Length,
                                          PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "UnlockFile: " << fileNameStr << std::endl;
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK GetDiskFreeSpace(
    PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes,
    PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK
GetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize,
                     LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength,
                     LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer,
                     DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK Mounted(PDOKAN_FILE_INFO DokanFileInfo) {
  std::cout << "Mounted" << std::endl;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK Unmounted(PDOKAN_FILE_INFO DokanFileInfo) {
  std::cout << "UnMounted" << std::endl;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
GetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation,
                PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength,
                PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "GetFileSecurity: " << fileNameStr << std::endl;
  auto fileNode = fileNodes->Find(fileNameStr);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  std::lock_guard<std::mutex> lockFile(fileNode->Security);

  if (fileNode->Security.DescriptorSize > BufferLength) {
    *LengthNeeded = fileNode->Security.DescriptorSize;
    return STATUS_BUFFER_OVERFLOW;
  }

  if (fileNode->Security.Descriptor) {
    memcpy(SecurityDescriptor, fileNode->Security.Descriptor,
           fileNode->Security.DescriptorSize);
    *LengthNeeded = fileNode->Security.DescriptorSize;
  }

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
SetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation,
                PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength,
                PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS DOKAN_CALLBACK
FindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData,
            PDOKAN_FILE_INFO DokanFileInfo) {
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
    nullptr,          // SetFileSecurity
};