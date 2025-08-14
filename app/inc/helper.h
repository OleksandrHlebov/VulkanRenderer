#ifndef HELPER_H
#define HELPER_H
#include <fstream>
#include <string>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace help
{
	inline std::vector<char> ReadFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			throw std::runtime_error("failed to open file " + filename);

		std::streampos const fileSize{ file.tellg() };
		std::vector<char>    buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	inline VkFormat FindSupportedFormat
	(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat const& format: candidates)
		{
			VkFormatProperties properties{};
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

			if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
				return format;

			if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
				return format;
		}

		throw std::runtime_error("failed to find supported format");
	}

	inline bool HasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
	}
}

#endif //HELPER_H
