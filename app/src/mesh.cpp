#include "mesh.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"

Mesh::Mesh
(
	Context&                  context
	, CommandBuffer const&    commandBuffer
	, std::vector<Vertex>&&   vertices, Buffer const& stagingVert
	, std::vector<uint32_t>&& indices, Buffer const&  stagingIndex
)
	: m_Vertices(std::move(vertices))
	, m_Indices(std::move(indices))
{
	m_VertexBuffer = std::make_unique<Buffer>(BufferBuilder{ context }
											  .SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
											  .Build(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
													 , m_Vertices.size() * sizeof(m_Vertices[0])));
	stagingVert.CopyTo(context, commandBuffer, *m_VertexBuffer);
	m_IndexBuffer = std::make_unique<Buffer>(BufferBuilder{ context }
											 .SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
											 .Build(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
													, m_Indices.size() * sizeof(m_Indices[0])));
	stagingIndex.CopyTo(context, commandBuffer, *m_IndexBuffer);
}

void Mesh::SetRotation(glm::vec3 const& rotation)
{
	m_Rotation     = rotation;
	m_ModelChanged = true;
}

void Mesh::Rotate(glm::vec3 const& rotation)
{
	m_Rotation += rotation;
	m_ModelChanged = true;
}

void Mesh::SetPosition(glm::vec3 const& position)
{
	m_Position     = position;
	m_ModelChanged = true;
}

void Mesh::Move(glm::vec3 const& position)
{
	m_Position += position;
	m_ModelChanged = true;
}

void Mesh::SetScale(glm::vec3 const& scale)
{
	m_Scale        = scale;
	m_ModelChanged = true;
}

void Mesh::Scale(glm::vec3 const& scale)
{
	m_Scale += scale;
	m_ModelChanged = true;
}

glm::mat4 Mesh::GetModelMatrix()
{
	if (m_ModelChanged)
	{
		m_ModelMatrix  = glm::mat4{ 1.f };
		m_ModelMatrix  = glm::translate(m_ModelMatrix, m_Position);
		m_ModelMatrix  = m_ModelMatrix * glm::toMat4(glm::quat(glm::radians(m_Rotation)));
		m_ModelMatrix  = glm::scale(m_ModelMatrix, m_Scale);
		m_ModelChanged = false;
	}
	return m_ModelMatrix;
}
