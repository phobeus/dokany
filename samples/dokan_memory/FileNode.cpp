#include "FileNode.h"

// Informations
// FileName can change during move


// Data
// Object lock not needed

FileNode::FileNode(std::wstring fileName, bool isDirectory)
    : FileNode(fileName, isDirectory,
               isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE,
               nullptr) {}

FileNode::FileNode(std::wstring fileName, bool isDirectory,
                          DWORD fileAttr,
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
    memcpy(Security.Descriptor, SecurityContext->AccessState.SecurityDescriptor,
           Security.DescriptorSize);
  }
}

const size_t FileNode::getFileSize() {
  std::lock_guard<std::mutex> lock(_data_mutex);
  return _data.size();
}

void FileNode::setEndOfFile(const LONGLONG& byteOffset) {
  std::lock_guard<std::mutex> lock(_data_mutex);
  _data.resize(byteOffset);
}

const std::wstring FileNode::getFileName() {
  std::lock_guard<std::mutex> lock(_fileName_mutex);
  return _fileName;
}

void FileNode::setFileName(const std::wstring& f) {
  std::lock_guard<std::mutex> lock(_fileName_mutex);
  _fileName = f;
}
