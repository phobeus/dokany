#include "FileNode.h"

FileNode::FileNode(std::wstring fileName, bool isDirectory)
    : FileNode(fileName, isDirectory,
               isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE,
               nullptr) {}

FileNode::FileNode(std::wstring fileName, bool isDirectory, DWORD fileAttr,
                   PDOKAN_IO_SECURITY_CONTEXT SecurityContext)
    : _fileName(fileName), IsDirectory(isDirectory), Attributes(fileAttr) {
  // No lock need, FileNode is still not in directory
  Times.Reset();

  if (SecurityContext && SecurityContext->AccessState.SecurityDescriptor) {
    Security.DescriptorSize = GetSecurityDescriptorLength(
        SecurityContext->AccessState.SecurityDescriptor);
    Security.Descriptor = new byte[Security.DescriptorSize];
    memcpy(Security.Descriptor, SecurityContext->AccessState.SecurityDescriptor,
           Security.DescriptorSize);
  }
}

DWORD FileNode::Read(LPVOID Buffer, DWORD BufferLength, LONGLONG Offset) {
  std::lock_guard<std::mutex> lock(_data_mutex);
  if (static_cast<size_t>(Offset + BufferLength) > _data.size())
    BufferLength = static_cast<DWORD>(_data.size() - Offset);
  if (BufferLength)
    memcpy(Buffer, &_data[static_cast<size_t>(Offset)], BufferLength);
  return BufferLength;
}

DWORD FileNode::Write(LPCVOID Buffer, DWORD NumberOfBytesToWrite,
                      LONGLONG Offset) {
  if (!NumberOfBytesToWrite) return 0;

  std::lock_guard<std::mutex> lock(_data_mutex);
  if (static_cast<size_t>(Offset + NumberOfBytesToWrite) > _data.size())
    _data.resize(static_cast<size_t>(Offset + NumberOfBytesToWrite));
  memcpy(&_data[static_cast<size_t>(Offset)], Buffer, NumberOfBytesToWrite);
  return NumberOfBytesToWrite;
}

const LONGLONG FileNode::getFileSize() {
  std::lock_guard<std::mutex> lock(_data_mutex);
  return static_cast<LONGLONG>(_data.size());
}

void FileNode::setEndOfFile(const LONGLONG& byteOffset) {
  std::lock_guard<std::mutex> lock(_data_mutex);
  _data.resize(static_cast<size_t>(byteOffset));
}

const std::wstring FileNode::getFileName() {
  std::lock_guard<std::mutex> lock(_fileName_mutex);
  return _fileName;
}

void FileNode::setFileName(const std::wstring& f) {
  std::lock_guard<std::mutex> lock(_fileName_mutex);
  _fileName = f;
}
