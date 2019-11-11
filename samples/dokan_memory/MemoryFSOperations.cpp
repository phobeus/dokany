#include "FileNode.h"
#include "FileNodes.h"
#include "MemoryFSOperations.h"

#include <unordered_map>
#include <sstream>
#include <mutex>
#include <iostream>

DOKAN_OPERATIONS MemoryFSOperations = {
    ZwCreateFile,
    Cleanup,
    CloseFile,
    ReadFile,
    WriteFile,
    FlushFileBuffers,
    GetFileInformation,
    FindFiles,
    nullptr, // FindFilesWithPattern
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
    nullptr, // GetFileSecurity
    nullptr, // SetFileSecurity
};

NTSTATUS ZwCreateFile(LPCWSTR FileName,
                      PDOKAN_IO_SECURITY_CONTEXT SecurityContext,
                      ACCESS_MASK DesiredAccess, ULONG FileAttributes,
                      ULONG ShareAccess, ULONG CreateDisposition,
                      ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo) {
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
    fileNode->IsDirectory = true;
  }

  if (DokanFileInfo->IsDirectory) {
    std::wcout << "CreateFile: [" << fileNameStr << "] is Directory"
               << std::endl;

    if (creationDisposition == CREATE_NEW ||
        creationDisposition == OPEN_ALWAYS) {
      if (fileNode)
        return STATUS_OBJECT_NAME_COLLISION;

      auto newfileNode = std::make_shared<FileNode>(fileNameStr, true,
                                                    FILE_ATTRIBUTE_DIRECTORY);
      return fileNodes->Add(newfileNode);
    }

    if (fileNode && !fileNode->IsDirectory)
      return STATUS_NOT_A_DIRECTORY;
    if (!fileNode)
      return STATUS_OBJECT_NAME_NOT_FOUND;
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
	// Combines the file attributes and flags specified by dwFlagsAndAttributes with FILE_ATTRIBUTE_ARCHIVE
	// All other file attributes override FILE_ATTRIBUTE_NORMAL
	fileAttributesAndFlags &= ~FILE_ATTRIBUTE_NORMAL;
	fileAttributesAndFlags |= FILE_ATTRIBUTE_ARCHIVE;

    switch (creationDisposition) {
    case CREATE_ALWAYS: {
      /*
	  * Creates a new file, always.
	  */
      auto n = fileNodes->Add(std::make_shared<FileNode>(
          fileNameStr, false, fileAttributesAndFlags));
      if (n != STATUS_SUCCESS)
        return n;

      if (fileNode)
        return STATUS_OBJECT_NAME_COLLISION;
    } break;
    case CREATE_NEW: {
      /*
	  * Creates a new file, only if it does not already exist.
	  */
      if (fileNode)
        return STATUS_OBJECT_NAME_COLLISION;
      auto n = fileNodes->Add(std::make_shared<FileNode>(
          fileNameStr, false, fileAttributesAndFlags));
      if (n != STATUS_SUCCESS)
        return n;
    } break;
    case OPEN_ALWAYS: {
      /*
	  * Opens a file, always.
	  */
      if (!fileNode) {
        auto n = fileNodes->Add(std::make_shared<FileNode>(
            fileNameStr, false,
            fileAttributesAndFlags));
        if (n != STATUS_SUCCESS)
          return n;
      }
    } break;
    case OPEN_EXISTING: {
      /*
	  * Opens a file or device, only if it exists.
	  * If the specified file or device does not exist, the function fails and the last-error code is set to ERROR_FILE_NOT_FOUND
	  */
      if (!fileNode)
        return STATUS_OBJECT_NAME_NOT_FOUND;
    } break;
    case TRUNCATE_EXISTING: {
      /*
	  * Opens a file and truncates it so that its size is zero bytes, only if it exists.
	  *	If the specified file does not exist, the function fails and the last-error code is set to ERROR_FILE_NOT_FOUND
	  */
      if (!fileNode)
        return STATUS_OBJECT_NAME_NOT_FOUND;
      // Todo reset fileNode buffer and attributes
    } break;
    }
  }

  /* 
  * CREATE_NEW && OPEN_ALWAYS
  * If the specified file exists, the function fails and the last-error code is set to ERROR_FILE_EXISTS
  */
  if (fileNode &&
      (creationDisposition == CREATE_NEW || creationDisposition == OPEN_ALWAYS))
    return STATUS_OBJECT_NAME_COLLISION;

  return STATUS_SUCCESS;
}

void Cleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  if (DokanFileInfo->DeleteOnClose)
    fileNodes->Remove(fileNameStr);
}

void CloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  //TODO
}

NTSTATUS ReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength,
                  LPDWORD ReadLength, LONGLONG Offset,
                  PDOKAN_FILE_INFO DokanFileInfo) {
  //TODO
  return STATUS_ACCESS_DENIED;
}

NTSTATUS WriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite,
                   LPDWORD NumberOfBytesWritten, LONGLONG Offset,
                   PDOKAN_FILE_INFO DokanFileInfo) {
  //TODO
  return STATUS_ACCESS_DENIED;
}

NTSTATUS FlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  //TODO
  return STATUS_ACCESS_DENIED;
}

NTSTATUS GetFileInformation(LPCWSTR FileName,
                            LPBY_HANDLE_FILE_INFORMATION Buffer,
                            PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  if (!fileNode)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  Buffer->dwFileAttributes = fileNode->Attributes;
  Buffer->ftCreationTime = fileNode->Times.Creation;
  Buffer->ftLastAccessTime = fileNode->Times.LastAccess;
  Buffer->ftLastWriteTime = fileNode->Times.LastWrite;
  auto strLength = fileNode->getFileSize();
  Buffer->nFileSizeHigh = strLength >> 32;
  Buffer->nFileSizeLow = strLength & 0xffffffff;

  //Improve this
  Buffer->nFileIndexHigh = Buffer->nFileIndexLow = 0;
  Buffer->nNumberOfLinks = 0;
  // Buffer->dwVolumeSerialNumber;
  return STATUS_SUCCESS;
}

NTSTATUS FindFiles(LPCWSTR FileName, PFillFindData FillFindData,
                   PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto files = fileNodes->ListFolder(fileNameStr);
  WIN32_FIND_DATAW findData;
  std::wcout << "FindFiles: " << fileNameStr << std::endl;
  ZeroMemory(&findData, sizeof(WIN32_FIND_DATAW));
  for (const auto &fileNode : files) {
    std::wcout << "FindFiles: " << fileNameStr << " fileNode "
               << fileNode->FileName << std::endl;
    auto fileName = fileNode->FilePath.filename().wstring();
    if (fileName.length() > MAX_PATH)
      continue;
    std::copy(fileName.begin(), fileName.end(), std::begin(findData.cFileName));
    findData.dwFileAttributes = fileNode->Attributes;
    findData.ftCreationTime = fileNode->Times.Creation;
    findData.ftLastAccessTime = fileNode->Times.LastAccess;
    findData.ftLastWriteTime = fileNode->Times.LastWrite;
    auto strLength = fileNode->getFileSize();
    findData.nFileSizeHigh = strLength >> 32;
    findData.nFileSizeLow = strLength & 0xffffffff;
    FillFindData(&findData, DokanFileInfo);
  }
  return STATUS_SUCCESS;
}

NTSTATUS FindFilesWithPattern(LPCWSTR PathName, LPCWSTR SearchPattern,
                              PFillFindData FillFindData,
                              PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS SetFileAttributes(LPCWSTR FileName, DWORD FileAttributes,
                           PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  std::wcout << "SetFileAttributes: " << fileNameStr << std::endl;
  if (!fileNode)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  fileNode->Attributes = FileAttributes;
  return STATUS_SUCCESS;
}

NTSTATUS SetFileTime(LPCWSTR FileName, CONST FILETIME *CreationTime,
                     CONST FILETIME *LastAccessTime,
                     CONST FILETIME *LastWriteTime,
                     PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  std::wcout << "SetFileTime: " << fileNameStr << std::endl;
  if (!fileNode)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (CreationTime)
    fileNode->Times.Creation = *CreationTime;
  if (LastAccessTime)
    fileNode->Times.LastAccess = *LastAccessTime;
  if (LastWriteTime)
    fileNode->Times.LastWrite = *LastWriteTime;
  return STATUS_SUCCESS;
}

NTSTATUS DeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto fileNode = fileNodes->Find(fileNameStr);
  std::wcout << "DeleteFile: " << fileNameStr << std::endl;

  if (!fileNode)
    return STATUS_OBJECT_NAME_NOT_FOUND;

  if (fileNode->IsDirectory)
    return STATUS_ACCESS_DENIED;

  return STATUS_SUCCESS;
}

NTSTATUS DeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "DeleteDirectory: " << fileNameStr << std::endl;

  if (fileNodes->ListFolder(fileNameStr).size())
    return STATUS_DIRECTORY_NOT_EMPTY;

  return STATUS_SUCCESS;
}

NTSTATUS MoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting,
                  PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  auto newFileNameStr = std::wstring(NewFileName);
  std::wcout << "DeleteDirectory: " << fileNameStr << " to " << newFileNameStr
             << std::endl;
  return fileNodes->Move(fileNameStr, newFileNameStr, ReplaceIfExisting);
}

NTSTATUS SetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset,
                      PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "SetEndOfFile: " << fileNameStr << std::endl;
  return STATUS_SUCCESS;
}

NTSTATUS SetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize,
                           PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "SetAllocationSize: " << fileNameStr << std::endl;
  return STATUS_SUCCESS;
}

NTSTATUS LockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length,
                  PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "LockFile: " << fileNameStr << std::endl;
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS UnlockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length,
                    PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileNodes = fs_instance;
  auto fileNameStr = std::wstring(FileName);
  std::wcout << "UnlockFile: " << fileNameStr << std::endl;
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS GetDiskFreeSpace(PULONGLONG FreeBytesAvailable,
                          PULONGLONG TotalNumberOfBytes,
                          PULONGLONG TotalNumberOfFreeBytes,
                          PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
GetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize,
                     LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength,
                     LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer,
                     DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS Mounted(PDOKAN_FILE_INFO DokanFileInfo) {
  std::cout << "Mounted" << std::endl;
  return STATUS_SUCCESS;
}

NTSTATUS Unmounted(PDOKAN_FILE_INFO DokanFileInfo) {
  std::cout << "UnMounted" << std::endl;
  return STATUS_SUCCESS;
}

NTSTATUS GetFileSecurity(LPCWSTR FileName,
                         PSECURITY_INFORMATION SecurityInformation,
                         PSECURITY_DESCRIPTOR SecurityDescriptor,
                         ULONG BufferLength, PULONG LengthNeeded,
                         PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS SetFileSecurity(LPCWSTR FileName,
                         PSECURITY_INFORMATION SecurityInformation,
                         PSECURITY_DESCRIPTOR SecurityDescriptor,
                         ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS FindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData,
                     PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_NOT_IMPLEMENTED;
}