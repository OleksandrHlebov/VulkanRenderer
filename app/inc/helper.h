#ifndef HELPER_H
#define HELPER_H
#include <format>
#include <fstream>
#include <string>
#include <vector>
#include <span>

#include "vulkan/vulkan_core.h"
#include "Context.h"

namespace help
{
	[[nodiscard]] inline std::vector<char> ReadFile(const std::string& filename)
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

	[[nodiscard]] inline std::string UUIDToHex(std::span<std::uint8_t> uuidBytes)
	{
		std::string result;
		result.reserve(uuidBytes.size() * 2);
		for (auto b: uuidBytes)
			result += std::format("{:02x}", b);

		return result;
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

	struct ImageData
	{
		~ImageData();

		int            Width;
		int            Height;
		int            Channels;
		unsigned char* Pixels;
		VkDeviceSize   Size;
	};

	ImageData LoadImage(std::string_view path);

	inline void NameObject(vkc::Context const& context, uint64_t handle, VkObjectType type, std::string_view name)
	{
		VkDebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType   = type;
		nameInfo.objectHandle = handle;
		nameInfo.pObjectName  = name.data();

		if (context.DispatchTable.setDebugUtilsObjectNameEXT(&nameInfo) != VK_SUCCESS)
			throw std::runtime_error("failed to set debug object name");
	}
}

#endif //HELPER_H
