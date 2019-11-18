#include "DokanMemoryFS.h"

void DokanMemoryFS::Run() {
  FileNodes = std::make_unique<MemoryFSFileNodes>();

  DOKAN_OPTIONS dokanOptions;
  ZeroMemory(&dokanOptions, sizeof(DOKAN_OPTIONS));
  dokanOptions.Version = DOKAN_VERSION;
  dokanOptions.ThreadCount = 1;
  dokanOptions.Options = DOKAN_OPTION_STDERR | DOKAN_OPTION_DEBUG;
  dokanOptions.MountPoint = L"M";
  dokanOptions.GlobalContext = reinterpret_cast<ULONG64>(FileNodes.get());

  NTSTATUS status = DokanMain(&dokanOptions, &MemoryFSOperations);
  switch (status) {
    case DOKAN_SUCCESS:
      break;
    case DOKAN_ERROR:
      throw std::runtime_error("Error");
      break;
    case DOKAN_DRIVE_LETTER_ERROR:
      throw std::runtime_error("Bad Drive letter");
      break;
    case DOKAN_DRIVER_INSTALL_ERROR:
      throw std::runtime_error("Can't install driver");
      break;
    case DOKAN_START_ERROR:
      throw std::runtime_error("Driver something wrong");
      break;
    case DOKAN_MOUNT_ERROR:
      throw std::runtime_error("Can't assign a drive letter");
      break;
    case DOKAN_MOUNT_POINT_ERROR:
      throw std::runtime_error("Mount point error");
      break;
    case DOKAN_VERSION_ERROR:
      throw std::runtime_error("Version error");
      break;
    default:
      throw std::runtime_error("Unknown error");  // add error
      break;
  }
}
