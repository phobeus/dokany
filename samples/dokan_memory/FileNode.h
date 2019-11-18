#pragma once
#include <dokan/dokan.h>
#include <dokan/fileinfo.h>

#include <winbase.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>

struct SecurityInformations : std::mutex {
  PSECURITY_DESCRIPTOR Descriptor = nullptr;
  DWORD DescriptorSize = 0;

  ~SecurityInformations() {
    if (Descriptor) delete[] Descriptor;
  }
};

struct FileTimes {
  void Reset() {
    FILETIME t;
    GetSystemTimeAsFileTime(&t);
    LastAccess = LastWrite = Creation = t;
  }

  static bool isEmpty(CONST FILETIME* fileTime) {
    return fileTime->dwHighDateTime == 0 && fileTime->dwLowDateTime == 0;
  }

  std::atomic<FILETIME> Creation;
  std::atomic<FILETIME> LastAccess;
  std::atomic<FILETIME> LastWrite;
};

class FileNode {
 public:
  FileNode(std::wstring fileName, bool isDirectory)
      : FileNode(
            fileName, isDirectory,
            isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE,
            nullptr) {}

  FileNode(std::wstring fileName, bool isDirectory, DWORD fileAttr,
           PDOKAN_IO_SECURITY_CONTEXT SecurityContext)
      : _fileName(fileName), IsDirectory(isDirectory), Attributes(fileAttr) {
    Times.Reset();
    // FilePath = std::filesystem::path(FileName);

    if (SecurityContext && SecurityContext->AccessState.SecurityDescriptor) {
      // Set security information
      // No lock need, FileNode is still not in directory
      Security.DescriptorSize = GetSecurityDescriptorLength(
          SecurityContext->AccessState.SecurityDescriptor);
      Security.Descriptor = new byte[Security.DescriptorSize];
      memcpy(Security.Descriptor,
             SecurityContext->AccessState.SecurityDescriptor,
             Security.DescriptorSize);
    }
  }

  FileNode(const FileNode& f) = delete;

  // Data
  // Object lock not needed
  const size_t getFileSize() {
    std::lock_guard<std::mutex> lock(_data_mutex);
    return _data.size();
  }
  void setEndOfFile(const LONGLONG& byteOffset) {
    std::lock_guard<std::mutex> lock(_data_mutex);
    _data.resize(byteOffset);
  }

  // Informations
  // FileName can change during move
  const std::wstring getFileName() {
    std::lock_guard<std::mutex> lock(_fileName_mutex);
    return _fileName;
  }
  void setFileName(const std::wstring& f) {
    std::lock_guard<std::mutex> lock(_fileName_mutex);
    _fileName = f;
  }

  // No lock needed above
  std::atomic<bool> IsDirectory = false;
  std::atomic<DWORD> Attributes = 0;

  FileTimes Times;
  SecurityInformations Security;

 private:
  FileNode() = default;

  std::mutex _data_mutex;
  // _data_mutex need to locked for read / write
  std::vector<WCHAR> _data;

  std::mutex _fileName_mutex;
  std::wstring _fileName;
};