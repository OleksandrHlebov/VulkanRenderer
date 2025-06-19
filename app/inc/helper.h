#ifndef HELPER_H
#define HELPER_H
#include <fstream>
#include <string>
#include <vector>

namespace help
{
	inline std::vector<char> ReadFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			throw std::runtime_error("failed to open file " + filename);

		std::streampos const fileSize{ file.tellg() };
		std::vector<char>    buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return std::move(buffer);
	}
}

#endif //HELPER_H
