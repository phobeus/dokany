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
  FileNode(std::wstring fileName, bool isDirectory);

  FileNode(std::wstring fileName, bool isDirectory, DWORD fileAttr,
           PDOKAN_IO_SECURITY_CONTEXT SecurityContext);

  FileNode(const FileNode& f) = delete;

  // Data
  // Object lock not needed
  const size_t getFileSize();
  void setEndOfFile(const LONGLONG& byteOffset);

  // Informations
  // FileName can change during move
  const std::wstring getFileName();
  void setFileName(const std::wstring& f);

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