#ifndef SCENE_H
#define SCENE_H

#include <list>
#include <string>

#include "mesh.h"

#include "command_pool.h"
#include "context.h"

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

private:
	void ProcessNode(aiNode const* node, aiScene const* scene, CommandBuffer& commandBuffer);

	Context&     m_Context;
	CommandPool& m_CommandPool;

	std::list<Buffer> m_StagingBuffers;

	std::list<Mesh> m_Meshes;

	glm::vec3 m_AABBMin{ FLT_MAX };
	glm::vec3 m_AABBMax{ FLT_MIN };
};

#endif //SCENE_H
