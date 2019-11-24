#include "FileNodes.h"

#include <sddl.h>
#include <spdlog/spdlog.h>

MemoryFSFileNodes::MemoryFSFileNodes() {
  WCHAR buffer[1024];
  WCHAR finalBuffer[2048];
  PTOKEN_USER userToken = NULL;
  PTOKEN_GROUPS groupsToken = NULL;
  HANDLE tokenHandle;
  LPTSTR userSidString = NULL, groupSidString = NULL;

  if (OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &tokenHandle) ==
      FALSE) {
    throw std::runtime_error("Failed init root resources");
  }

  DWORD returnLength;
  if (!GetTokenInformation(tokenHandle, TokenUser, buffer, sizeof(buffer),
                           &returnLength)) {
    CloseHandle(tokenHandle);
    throw std::runtime_error("Failed init root resources");
  }

  userToken = (PTOKEN_USER)buffer;
  if (!ConvertSidToStringSid(userToken->User.Sid, &userSidString)) {
    CloseHandle(tokenHandle);
    throw std::runtime_error("Failed init root resources");
  }

  if (!GetTokenInformation(tokenHandle, TokenGroups, buffer, sizeof(buffer),
                           &returnLength)) {
    CloseHandle(tokenHandle);
    throw std::runtime_error("Failed init root resources");
  }

  groupsToken = (PTOKEN_GROUPS)buffer;
  if (groupsToken->GroupCount > 0) {
    if (!ConvertSidToStringSid(groupsToken->Groups[0].Sid, &groupSidString)) {
      CloseHandle(tokenHandle);
      throw std::runtime_error("Failed init root resources");
    }
    swprintf_s(buffer, 1024, L"O:%lsG:%ls", userSidString, groupSidString);
  } else
    swprintf_s(buffer, 1024, L"O:%ls", userSidString);

  LocalFree(userSidString);
  LocalFree(groupSidString);
  CloseHandle(tokenHandle);

  swprintf_s(finalBuffer, 2048, L"%lsD:PAI(A;OICI;FA;;;AU)", buffer);

  PSECURITY_DESCRIPTOR securityDescriptor = NULL;
  ULONG Size = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
          finalBuffer, SDDL_REVISION_1, &securityDescriptor, &Size))
    throw std::runtime_error("Failed init root resources");

  auto fileNode = std::make_shared<FileNode>(L"\\", true,
                                             FILE_ATTRIBUTE_DIRECTORY, nullptr);
  fileNode->Security.SetDescriptor(securityDescriptor);
  LocalFree(securityDescriptor);

  _fileNodes[L"\\"] = fileNode;
  _directoryPaths.emplace(L"\\", std::set<std::shared_ptr<FileNode>>());
}

NTSTATUS MemoryFSFileNodes::Add(const std::shared_ptr<FileNode>& fileNode) {
  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);

  if (fileNode->FileIndex == 0)  // previous init
    fileNode->FileIndex = _FSFileIndexCount++;
  const auto fileName = fileNode->getFileName();
  const auto parent_path =
      std::filesystem::path(fileName).parent_path().wstring();

  // Does target folder exist
  if (!_directoryPaths.count(parent_path)) {
    spdlog::warn(L"Add: No directory: {} exist FilePath: {}", parent_path,
                 fileName);
    return STATUS_OBJECT_PATH_NOT_FOUND;
  }

  // If we have a folder, we add it to our directoryPaths
  if (fileNode->IsDirectory && !_directoryPaths.count(fileName))
    _directoryPaths.emplace(fileName, std::set<std::shared_ptr<FileNode>>());

  // Add our file to the fileNodes and directoryPaths
  _fileNodes[fileName] = fileNode;
  _directoryPaths[parent_path].insert(fileNode);

  spdlog::info(L"Add file: {} in folder: {}", fileName, parent_path);
  return STATUS_SUCCESS;
}

std::shared_ptr<FileNode> MemoryFSFileNodes::Find(
    const std::wstring& fileName) {
  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
  auto fileNode = _fileNodes.find(fileName);
  return (fileNode != _fileNodes.end()) ? fileNode->second : nullptr;
}

std::set<std::shared_ptr<FileNode>> MemoryFSFileNodes::ListFolder(
    const std::wstring& fileName) {
  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);

  auto it = _directoryPaths.find(fileName);
  return (it != _directoryPaths.end()) ? it->second
                                       : std::set<std::shared_ptr<FileNode>>();
}

void MemoryFSFileNodes::Remove(const std::wstring& fileName) {
  return Remove(Find(fileName));
}

void MemoryFSFileNodes::Remove(const std::shared_ptr<FileNode>& fileNode) {
  if (!fileNode) return;

  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
  auto fileName = fileNode->getFileName();
  spdlog::info(L"Remove: {}", fileName);

  // Remove node from fileNodes and directoryPaths
  _fileNodes.erase(fileName);
  _directoryPaths[std::filesystem::path(fileName).parent_path()].erase(
      fileNode);

  // if it was a directory we need to remove it from directoryPaths
  if (fileNode->IsDirectory) {
    // but first we need to remove the directory content by looking recursively
    // into it
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

  // Cannot move to an existing destination without replace flag
  if (!replaceIfExisting && newFileNode) return STATUS_OBJECT_NAME_COLLISION;

  // Cannot replace read only destination
  if (newFileNode && newFileNode->Attributes & FILE_ATTRIBUTE_READONLY)
    return STATUS_ACCESS_DENIED;

  // If destination exist - Cannot move directory or replace a directory
  if (newFileNode && (fileNode->IsDirectory || newFileNode->IsDirectory))
    return STATUS_ACCESS_DENIED;

  auto newParent_path =
      std::filesystem::path(newFileName).parent_path().wstring();

  std::lock_guard<std::recursive_mutex> lock(_filesNodes_mutex);
  if (!_directoryPaths.count(newParent_path)) {
    spdlog::warn(L"Move: No directory: {} exist FilePath: {}", newParent_path,
                 newFileName);
    return STATUS_OBJECT_PATH_NOT_FOUND;
  }

  // Remove destination
  Remove(newFileNode);

  // Update current node with new data
  const auto fileName = fileNode->getFileName();
  auto oldParentPath = std::filesystem::path(fileName).parent_path();
  fileNode->setFileName(newFileName);

  // Move fileNode
  // 1 - by removing current not with oldName as key
  Add(fileNode);

  // 2 - If fileNode is a Dir we move content to destination
  if (fileNode->IsDirectory) {
    // recurse remove sub folders/files
    auto files = ListFolder(oldFilename);
    for (const auto& file : files) {
      const auto fileName = file->getFileName();
      auto newSubFileName =
          std::filesystem::path(newFileName)
              .append(std::filesystem::path(fileName).filename().wstring())
              .wstring();
      auto n = Move(fileName, newSubFileName, replaceIfExisting);
      if (n != STATUS_SUCCESS) {
        spdlog::warn(
            L"Move: Subfolder file move {} to {} replaceIfExisting {} failed: "
            L"{}",
            fileName, newSubFileName, replaceIfExisting, n);
        return n;  // That's bad...we have not done a full move
      }
    }

    // remove folder from directories
    _directoryPaths.erase(oldFilename);
  }

  // 3 - Remove fileNode link with oldFilename
  _fileNodes.erase(oldFilename);
  if (oldParentPath != newParent_path)  // Same folder destination
    _directoryPaths[oldParentPath].erase(fileNode);

  spdlog::info(L"Move file: {} to folder: {}", oldFilename, newFileName);
  return STATUS_SUCCESS;
}
