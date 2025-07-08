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
	Scene(Context& context, CommandPool& commandPool);
	~Scene() = default;

	Scene(Scene&&)                 = delete;
	Scene(Scene const&)            = delete;
	Scene& operator=(Scene&&)      = delete;
	Scene& operator=(Scene const&) = delete;

	void LoadScene(std::string_view filename);

	void LoadFirstMeshFromFile(std::string_view filename);

	[[nodiscard]] std::list<Mesh> const& GetMeshes() const
	{
		return m_Meshes;
	}

	[[nodiscard]] std::span<Image> GetTextureImages()
	{
		return m_TextureImages;
	}

	[[nodiscard]] std::span<ImageView> GetTextureImageViews()
	{
		return m_TextureImageViews;
	}

private:
	void     ProcessNode(aiNode const* node, aiScene const* scene, CommandBuffer& commandBuffer);
	uint32_t LoadTexture(aiTextureType, aiMaterial const* material, CommandBuffer const& commandBuffer);

	Context&     m_Context;
	CommandPool& m_CommandPool;

	std::stack<Buffer> m_StagingBuffers;

	std::list<Mesh> m_Meshes;

	std::vector<Image>                        m_TextureImages;
	std::vector<ImageView>                    m_TextureImageViews;
	std::unordered_map<std::string, uint32_t> m_LoadedTextures;

	glm::vec3 m_AABBMin{ FLT_MAX };
	glm::vec3 m_AABBMax{ FLT_MIN };
};

#endif //SCENE_H
