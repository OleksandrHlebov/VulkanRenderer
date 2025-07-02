#ifndef DATATYPES_H
#define DATATYPES_H
#include <span>

#include "vulkan/vulkan_core.h"
#include "glm/glm.hpp"

struct ModelViewProj
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct TextureIndices
{
	uint32_t Diffuse;
};

struct Vertex
{
	glm::vec3 Position;
	glm::vec2 UV;

	static std::span<VkVertexInputBindingDescription> GetBindingDescription()
	{
		static VkVertexInputBindingDescription bindingDescription[1]
		{
			{
				.binding = 0
				, .stride = sizeof(Vertex)
				, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}
		};

		return bindingDescription;
	}

	static std::span<VkVertexInputAttributeDescription> GetAttributeDescription()
	{
		static VkVertexInputAttributeDescription attributeDescriptions[]
		{
			{
				.location = 0
				, .binding = 0
				, .format = VK_FORMAT_R32G32B32_SFLOAT
				, .offset = offsetof(Vertex, Position)
			}
			, {
				.location = 1
				, .binding = 0
				, .format = VK_FORMAT_R32G32_SFLOAT
				, .offset = offsetof(Vertex, UV)
			}
		};

		return attributeDescriptions;
	}
};

#endif //DATATYPES_H
