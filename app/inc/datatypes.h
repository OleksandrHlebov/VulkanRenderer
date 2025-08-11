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
	uint32_t Normals;
	uint32_t Metalness;
	uint32_t Roughness;
};

struct Vertex
{
	glm::vec3 Position;
	glm::vec2 UV;

	glm::vec3 Normal;
	glm::vec3 Tangent;
	glm::vec3 Bitangent;

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
			, {
				.location = 2
				, .binding = 0
				, .format = VK_FORMAT_R32G32B32_SFLOAT
				, .offset = offsetof(Vertex, Normal)
			}
			, {
				.location = 3
				, .binding = 0
				, .format = VK_FORMAT_R32G32B32_SFLOAT
				, .offset = offsetof(Vertex, Tangent)
			}
			, {
				.location = 4
				, .binding = 0
				, .format = VK_FORMAT_R32G32B32_SFLOAT
				, .offset = offsetof(Vertex, Bitangent)
			}
		};

		return attributeDescriptions;
	}
};

#endif //DATATYPES_H
