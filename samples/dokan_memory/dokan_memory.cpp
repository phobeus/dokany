#include "DokanMemoryFS.h"
#include <spdlog/spdlog.h>

int main() {
  try {
    auto dokanMemoryFs = std::make_shared<DokanMemoryFS>();
    dokanMemoryFs->Run();
  } catch (const std::exception& ex) {
	spdlog::error("DokanMemoryFS failure: {}", ex.what());
  }
}