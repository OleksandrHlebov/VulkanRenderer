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

	[[nodiscard]] std::list<Image> const& GetTextureImages() const
	{
		return m_TextureImages;
	}

	[[nodiscard]] std::list<ImageView> const& GetTextureImageViews() const
	{
		return m_TextureImageViews;
	}

private:
	void     ProcessNode(aiNode const* node, aiScene const* scene, CommandBuffer& commandBuffer);
	uint32_t LoadTexture(aiTextureType, aiMaterial const* material);

	Context&     m_Context;
	CommandPool& m_CommandPool;

	std::list<Buffer> m_StagingBuffers;

	std::list<Mesh> m_Meshes;

	std::list<Image>                          m_TextureImages;
	std::list<ImageView>                      m_TextureImageViews;
	std::unordered_map<std::string, uint32_t> m_LoadedTextures;

	glm::vec3 m_AABBMin{ FLT_MAX };
	glm::vec3 m_AABBMax{ FLT_MIN };
};

#endif //SCENE_H
