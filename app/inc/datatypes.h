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

struct Config
{
	VkBool32 EnableDirectionalLights{ VK_TRUE };
	VkBool32 EnablePointLights{ VK_TRUE };
};

class Light
{
public:
	Light(glm::vec3 const& position, bool isPoint, glm::vec3 const& colour, float intensity)
		: m_Position{ position.x, position.y, position.z, isPoint ? 1.f : .0f }
		, m_Colour{ colour }
		, m_Intensity{ intensity } {}

	[[nodiscard]] bool IsPoint() const
	{
		return m_Position.w > .0f;
	}

	[[nodiscard]] glm::vec4 const& GetPosition() const
	{
		return m_Position;
	}

	[[nodiscard]] uint32_t GetMatrixIndex() const
	{
		return m_MatrixIndex;
	}

	[[nodiscard]] uint32_t GetShadowMapIndex() const
	{
		return m_ShadowMapIndex;
	}

	void LinkShadowMapIndex(uint32_t shadowMapIndex)
	{
		m_ShadowMapIndex = shadowMapIndex;
	}

private:
	friend class Scene;
	glm::vec4           m_Position;
	glm::vec3           m_Colour;
	float               m_Intensity;
	alignas(16)uint32_t m_ShadowMapIndex{ UINT32_MAX };
	uint32_t            m_MatrixIndex{ UINT32_MAX };
};

struct LightData
{
	std::vector<Light>     Lights;
	std::vector<glm::mat4> LightSpaceMatrices;
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
