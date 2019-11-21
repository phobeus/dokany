#include "FileNodes.h"

MemoryFSFileNodes::MemoryFSFileNodes() {
  _fileNodes[L"\\"] = std::make_shared<FileNode>(L"\\", true);
  _directoryPaths.emplace(L"\\", std::set<std::shared_ptr<FileNode>>());
}

NTSTATUS MemoryFSFileNodes::Add(
    const std::shared_ptr<FileNode>& fileNode) {
  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);

  fileNode->FileIndex = _FSFileIndexCount++;
  const auto fileName = fileNode->getFileName();
  const auto parent_path = std::filesystem::path(fileName).parent_path();
  if (!_directoryPaths.count(parent_path)) {
    std::wcout << "Add: No directory: [" << parent_path << "] exist FilePath: ["
               << fileName << "]" << std::endl;
    return STATUS_OBJECT_PATH_NOT_FOUND;
  }

  if (fileNode->IsDirectory && !_directoryPaths.count(fileName))
    _directoryPaths.emplace(fileName, std::set<std::shared_ptr<FileNode>>());

  _fileNodes[fileName] = fileNode;
  _directoryPaths[parent_path].insert(fileNode);

  std::wcout << "Add file: [" << fileName << "] in folder: [" << parent_path
             << "]" << std::endl;
  return STATUS_SUCCESS;
}

std::shared_ptr<FileNode> MemoryFSFileNodes::Find(
    const std::wstring& fileName) {
  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
  auto fileNode = _fileNodes.find(fileName);
  if (fileNode != _fileNodes.end()) return fileNode->second;
  return nullptr;
}

std::set<std::shared_ptr<FileNode>> MemoryFSFileNodes::ListFolder(
    const std::wstring& fileName) {
  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);

  std::set<std::shared_ptr<FileNode>> r;
  auto it = _directoryPaths.find(fileName);
  if (it != _directoryPaths.end()) {
    r = it->second;
  }
  return r;
}

void MemoryFSFileNodes::Remove(const std::wstring& fileName) {
  return Remove(Find(fileName));
}

void MemoryFSFileNodes::Remove(
    const std::shared_ptr<FileNode>& fileNode) {
  if (!fileNode) return;

  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
  const auto fileName = fileNode->getFileName();
  _fileNodes.erase(fileName);
  _directoryPaths[std::filesystem::path(fileName).parent_path()].erase(
      fileNode);
  if (fileNode->IsDirectory) {
    // recurse remove sub folders/files
    auto files = ListFolder(fileName);
    for (const auto& file : files) Remove(file);

    _directoryPaths.erase(fileName);
  }
}

NTSTATUS MemoryFSFileNodes::Move(std::wstring oldFilename,
                                        std::wstring newFileName,
                                        BOOL replaceIfExisting) {
  auto fileNode = Find(oldFilename);
  auto newFileNode = Find(newFileName);

  if (!fileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

  if (!replaceIfExisting && newFileNode) return STATUS_OBJECT_NAME_COLLISION;

  // Cannot replace read only
  if (newFileNode && newFileNode->Attributes & FILE_ATTRIBUTE_READONLY)
    return STATUS_ACCESS_DENIED;

  // If dist exist - Cannot move directory or replace a directory
  if (newFileNode && (fileNode->IsDirectory || newFileNode->IsDirectory))
    return STATUS_ACCESS_DENIED;

  // Remove destination
  Remove(newFileNode);

  // Update current with new data
  const auto fileName = fileNode->getFileName();
  auto oldParentPath = std::filesystem::path(fileName).parent_path();
  fileNode->setFileName(newFileName);
  auto parent_path = std::filesystem::path(newFileName).parent_path();
  bool isDirectory = fileNode->IsDirectory;

  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
  if (!_directoryPaths.count(parent_path)) {
    std::wcout << "Move: No directory: [" << parent_path
               << "] exist FilePath: [" << newFileName << "]" << std::endl;
    return STATUS_OBJECT_PATH_NOT_FOUND;
  }

  // If fileNode is a Dir we move content to destination
  if (isDirectory) {
    // recurse remove sub folders/files
    auto files = ListFolder(oldFilename);
    for (const auto& file : files) {
      const auto fileName = file->getFileName();
      auto newSubFileName =
          oldFilename + std::filesystem::path(fileName).filename().wstring();
      Move(fileName, newSubFileName, replaceIfExisting);
    }

    // remove folder from directories
    _directoryPaths.erase(oldFilename);
  }

  // Move fileNode
  _fileNodes.erase(oldFilename);
  _directoryPaths[oldParentPath].erase(fileNode);
  _fileNodes[newFileName] = fileNode;
  _directoryPaths[parent_path].insert(fileNode);

  return STATUS_SUCCESS;
}
