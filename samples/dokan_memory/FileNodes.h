#pragma once
#include <memory>
#include <mutex>

#include <iostream>
#include <set>
#include <unordered_map>
#include "FileNode.h"

// TODO - support streams
class MemoryFSFileNodes {
 public:
  MemoryFSFileNodes();

  NTSTATUS Add(const std::shared_ptr<FileNode>& fileNode);

  std::shared_ptr<FileNode> Find(const std::wstring& fileName);

  std::set<std::shared_ptr<FileNode>> ListFolder(const std::wstring& fileName);

  void Remove(const std::wstring& fileName);
  void Remove(const std::shared_ptr<FileNode>& fileNode);

  NTSTATUS Move(std::wstring oldFilename, std::wstring newFileName,
                BOOL replaceIfExisting);

 private:
  std::atomic<LONGLONG> _FSFileIndexCount = 1;

  // Mutex need to be aquired when using fileNodes / directoryPaths
  std::recursive_mutex _filesNodes_mutex;
  std::unordered_map<std::wstring, std::shared_ptr<FileNode>> _fileNodes;
  std::unordered_map<std::wstring, std::set<std::shared_ptr<FileNode>>>
      _directoryPaths;
};
