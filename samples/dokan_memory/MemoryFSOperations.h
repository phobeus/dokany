#pragma once

#include <dokan/dokan.h>

extern DOKAN_OPERATIONS MemoryFSOperations;

#define fs_instance                     \
  reinterpret_cast<MemoryFSFileNodes*>( \
      DokanFileInfo->DokanOptions->GlobalContext)