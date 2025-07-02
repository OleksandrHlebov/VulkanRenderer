#ifndef MESH_H
#define MESH_H

#include <memory>
#include <vector>

#include "buffer.h"
#include "datatypes.h"
#include "glm/glm.hpp"

class Mesh final
{
public:
	Mesh
	(
		Context&                  context
		, CommandBuffer const&    commandBuffer
		, std::vector<Vertex>&&   vertices, Buffer const& stagingVert
		, std::vector<uint32_t>&& indices, Buffer const&  stagingIndex
	);
	~Mesh() = default;

	Mesh(Mesh&&)                 = delete;
	Mesh(Mesh const&)            = delete;
	Mesh& operator=(Mesh&&)      = delete;
	Mesh& operator=(Mesh const&) = delete;

	[[nodiscard]] Buffer const& GetVertexBuffer() const
	{
		return m_VertexBuffer;
	}

	[[nodiscard]] Buffer const& GetIndexBuffer() const
	{
		return m_IndexBuffer;
	}

	void SetRotation(glm::vec3 const& rotation);

	void Rotate(glm::vec3 const& rotation);

	void SetPosition(glm::vec3 const& position);

	void Move(glm::vec3 const& position);

	void SetScale(glm::vec3 const& scale);

	void Scale(glm::vec3 const& scale);

	glm::mat4 GetModelMatrix();

private:
	glm::vec3 m_Position{};
	glm::vec3 m_Rotation{};
	glm::vec3 m_Scale{};
	glm::mat4 m_ModelMatrix{};

	std::vector<Vertex>   m_Vertices;
	std::vector<uint32_t> m_Indices;

	Buffer m_VertexBuffer;
	Buffer m_IndexBuffer;
	bool   m_ModelChanged{ false };
};

#endif //MESH_H
