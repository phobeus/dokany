#pragma once
#include <dokan/dokan.h>
#include <dokan/fileinfo.h>

#include <winbase.h>
#include <iostream>
#include "FileNodes.h"
#include "MemoryFSOperations.h"

class DokanMemoryFS {
 public:
  void Run();

  std::unique_ptr<MemoryFSFileNodes> FileNodes;
};