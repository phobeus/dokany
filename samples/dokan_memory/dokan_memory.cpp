#include "DokanMemoryFS.h"

int main() {
  auto dokanMemoryFs = std::make_shared<DokanMemoryFS>();
  dokanMemoryFs->run();
}