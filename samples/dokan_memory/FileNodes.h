#pragma once
#include <memory>
#include <mutex>

#include "FileNode.h"
#include <unordered_map>
#include <set>
#include <iostream>

class MemoryFSFileNodes {
public:
  MemoryFSFileNodes() {
    //Add root
    _fileNodes[L"\\"] = std::make_shared<FileNode>(L"\\", true);
    _directoryPaths.emplace(L"\\", std::set<std::shared_ptr<FileNode>>());
  }

  NTSTATUS Add(const std::shared_ptr<FileNode> &fileNode) {
    std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);

    auto parent_path = fileNode->FilePath.parent_path();
    if (!_directoryPaths.count(parent_path)) {
      std::wcout << "Add: No directory: [" << parent_path << "] exist FilePath: ["
                 << fileNode->FileName << "]" << std::endl;
      return STATUS_OBJECT_PATH_NOT_FOUND;
    }
    _fileNodes[fileNode->FileName] = fileNode;
    _directoryPaths[parent_path].insert(fileNode);
    if (fileNode->IsDirectory && !_directoryPaths.count(fileNode->FileName))
      _directoryPaths.emplace(fileNode->FileName,
                              std::set<std::shared_ptr<FileNode>>());

    std::wcout << "Add file: [" << fileNode->FileName << "] in folder: ["
               << parent_path << "]" << std::endl;
    return STATUS_SUCCESS;
  }

  std::shared_ptr<FileNode> Find(const std::wstring &fileName) {
    std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
    auto fileNode = _fileNodes.find(fileName);
    if (fileNode != _fileNodes.end())
      return fileNode->second;
    return nullptr;
  }

  std::set<std::shared_ptr<FileNode>> ListFolder(const std::wstring &fileName) {
    std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);

    std::set<std::shared_ptr<FileNode>> r;
    auto it = _directoryPaths.find(fileName);
    if (it != _directoryPaths.end()) {
      r = it->second;
    }
    return r;
  }

  void Remove(const std::wstring &fileName) { return Remove(Find(fileName)); }

  void Remove(const std::shared_ptr<FileNode> &fileNode) {
    if (!fileNode)
      return;

    std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
    _fileNodes.erase(fileNode->FileName);
    _directoryPaths[fileNode->FilePath.parent_path()].erase(fileNode);
    if (fileNode->IsDirectory) {
      //recurse remove sub folders/files
      auto files = ListFolder(fileNode->FileName);
      for (const auto &file : files)
        Remove(file);

      _directoryPaths.erase(fileNode->FileName);
    }
  }

  NTSTATUS Move(std::wstring fileName, std::wstring newFileName,
                BOOL replaceIfExisting) {
    auto fileNode = Find(fileName);
    auto newFileNode = Find(newFileName);

    if (!fileNode)
      return STATUS_OBJECT_NAME_NOT_FOUND;

    if (!replaceIfExisting && newFileNode)
      return STATUS_OBJECT_NAME_COLLISION;

	//Cannot replace read only
	if (newFileNode && newFileNode->Attributes & FILE_ATTRIBUTE_READONLY)
      return STATUS_ACCESS_DENIED;

	//If dist exist - Cannot move directory or replace a directory
	if (newFileNode && (fileNode->IsDirectory || newFileNode->IsDirectory))
          return STATUS_ACCESS_DENIED;

    Remove(newFileNode);

	newFileNode = std::make_shared<FileNode>(*fileNode);
    newFileNode->FileName = newFileName;
    auto parent_path = newFileNode->FilePath.parent_path();

	std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
    if (!_directoryPaths.count(parent_path)) {
      std::wcout << "Move: No directory: [" << parent_path << "] exist FilePath: ["
                 << fileNode->FileName << "]" << std::endl;
      return STATUS_OBJECT_PATH_NOT_FOUND;
    }

	// First move sub files entry if dir
    if (fileNode->IsDirectory) {
      //recurse remove sub folders/files
      auto files = ListFolder(fileNode->FileName);
      for (const auto &file : files) {
        auto newSubFileName = fileNode->FileName + file->FilePath.filename().wstring();
        Move(file->FileName, newSubFileName, replaceIfExisting);
      }

      _directoryPaths.erase(fileNode->FileName);
    }

	// Move dir entry
	_fileNodes.erase(fileNode->FileName);
    _directoryPaths[fileNode->FilePath.parent_path()].erase(fileNode);
    _fileNodes[newFileNode->FileName] = newFileNode;
    _directoryPaths[parent_path].insert(newFileNode);

	return STATUS_SUCCESS;
  }

private:
  std::recursive_mutex _filesNodes_mutex;
  std::unordered_map<std::wstring, std::shared_ptr<FileNode>> _fileNodes;
  std::unordered_map<std::wstring, std::set<std::shared_ptr<FileNode>>>
      _directoryPaths;
};
