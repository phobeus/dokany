#include "FileNode.h"
#include <spdlog/spdlog.h>

FileNode::FileNode(std::wstring fileName, bool isDirectory, DWORD fileAttr,
                   PDOKAN_IO_SECURITY_CONTEXT SecurityContext)
    : _fileName(fileName), IsDirectory(isDirectory), Attributes(fileAttr) {
  // No lock need, FileNode is still not in directory
  Times.Reset();

  if (SecurityContext && SecurityContext->AccessState.SecurityDescriptor) {
    spdlog::info(L"{} : Attach SecurityDescriptor", fileName);
    Security.SetDescriptor(SecurityContext->AccessState.SecurityDescriptor);
  }
}

DWORD FileNode::Read(LPVOID Buffer, DWORD BufferLength, LONGLONG Offset) {
  std::lock_guard<std::mutex> lock(_data_mutex);
  if (static_cast<size_t>(Offset + BufferLength) > _data.size())
    BufferLength = static_cast<DWORD>(_data.size() - Offset);
  if (BufferLength)
    memcpy(Buffer, &_data[static_cast<size_t>(Offset)], BufferLength);
  spdlog::info(L"Read {} : BufferLength {} Offset {}", getFileName(),
               BufferLength, Offset);
  return BufferLength;
}

DWORD FileNode::Write(LPCVOID Buffer, DWORD NumberOfBytesToWrite,
                      LONGLONG Offset) {
  if (!NumberOfBytesToWrite) return 0;

  std::lock_guard<std::mutex> lock(_data_mutex);
  if (static_cast<size_t>(Offset + NumberOfBytesToWrite) > _data.size())
    _data.resize(static_cast<size_t>(Offset + NumberOfBytesToWrite));

  spdlog::info(L"Write {} : NumberOfBytesToWrite {} Offset {}", getFileName(),
               NumberOfBytesToWrite, Offset);
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
