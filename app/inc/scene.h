#ifndef SCENE_H
#define SCENE_H

#include <list>
#include <string>

#include "mesh.h"

#include "command_pool.h"
#include "context.h"
#include "assimp/material.h"

struct aiNode;
struct aiScene;

class Scene final
{
public:
	Scene() = delete;
	Scene(vkc::Context& context, vkc::CommandPool& commandPool);
	~Scene() = default;

	Scene(Scene&&)                 = delete;
	Scene(Scene const&)            = delete;
	Scene& operator=(Scene&&)      = delete;
	Scene& operator=(Scene const&) = delete;

	void Load(std::string_view filename);

	void LoadFirstMeshFromFile(std::string_view filename);

	[[nodiscard]] bool ContainsPBRInfo() const
	{
		return m_ContainsPBRInfo;
	};

	[[nodiscard]] std::list<Mesh> const& GetMeshes() const
	{
		return m_Meshes;
	}

	[[nodiscard]] std::span<vkc::Image> GetTextureImages()
	{
		return m_TextureImages;
	}

	[[nodiscard]] std::span<vkc::ImageView> GetTextureImageViews()
	{
		return m_TextureImageViews;
	}

	[[nodiscard]] std::span<Light> GetLights()
	{
		return m_Lights;
	}

	void AddLight(Light&& light);
	void AddLight(Light const& light);
	void AddLight(glm::vec3 const& position, bool isPoint, glm::vec3 const& colour, float intensity);

private:
	void     ProcessNode(aiNode const* node, aiScene const* scene, vkc::CommandBuffer& commandBuffer);
	uint32_t LoadTexture
	(aiTextureType, aiMaterial const* material, vkc::CommandBuffer const& commandBuffer, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

	vkc::Context&     m_Context;
	vkc::CommandPool& m_CommandPool;

	std::stack<vkc::Buffer> m_StagingBuffers;

	std::list<Mesh>    m_Meshes;
	std::vector<Light> m_Lights;

	std::vector<vkc::Image>                   m_TextureImages;
	std::vector<vkc::ImageView>               m_TextureImageViews;
	std::unordered_map<std::string, uint32_t> m_LoadedTextures;

	glm::vec3 m_AABBMin{ FLT_MAX };
	glm::vec3 m_AABBMax{ FLT_MIN };

	bool m_ContainsPBRInfo{};
};

#endif //SCENE_H
