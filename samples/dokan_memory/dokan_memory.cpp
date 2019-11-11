#include "DokanMemoryFS.h"


int main() {

	/*auto FilePath = std::filesystem::path(L"\\myfir\\myfile.data");	
	std::cout << FilePath.relative_path() << std::endl;
        std::cout << FilePath.parent_path() << std::endl;
        std::cout << FilePath.root_path() << std::endl;
        std::cout << FilePath.filename() << std::endl;*/
  /*std::vector<WCHAR> data;
        std::cout << data.size() << std::endl;
  data.resize(42);
        std::cout << data.size() << std::endl;
  data.resize(0);
  std::cout << data.size() << std::endl;*/

  auto dokanMemoryFs = std::make_shared<DokanMemoryFS>();
  dokanMemoryFs->run();
}