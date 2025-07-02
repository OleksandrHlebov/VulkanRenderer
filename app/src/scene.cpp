#include "scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "datatypes.h"

#include <stdexcept>

#include "command_pool.h"

Scene::Scene(Context& context, CommandPool& commandPool)
	: m_Context{ context }
	, m_CommandPool{ commandPool } {}

void Scene::LoadScene(std::string_view filename)
{
	Assimp::Importer importer;
	const aiScene*   scene = importer.ReadFile(filename.data()
											 , aiProcess_Triangulate |
											   aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
											   aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords |
											   aiProcess_GenNormals | aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		throw std::runtime_error("failed to load model " + std::string(importer.GetErrorString()));
	}
	CommandBuffer& commandBuffer = m_CommandPool.AllocateCommandBuffer(m_Context);
	commandBuffer.Begin(m_Context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	ProcessNode(scene->mRootNode, scene, commandBuffer);
	commandBuffer.End(m_Context);
	commandBuffer.Submit(m_Context, m_Context.GraphicsQueue, {}, {});
	if (auto const result = m_Context.DispatchTable.waitForFences(1, &commandBuffer.GetFence(), VK_TRUE, UINT64_MAX);
		result != VK_SUCCESS)
		throw std::runtime_error("failed to wait for the fences");
}

void Scene::LoadFirstMeshFromFile(std::string_view filename)
{
	std::string string{ filename };
	m_Meshes.clear();
}

void Scene::ProcessNode(aiNode const* node, aiScene const* scene, CommandBuffer& commandBuffer)
{
	for (uint32_t meshIndex{}; meshIndex < node->mNumMeshes; meshIndex++)
	{
		aiMesh const* const   mesh = scene->mMeshes[node->mMeshes[meshIndex]];
		std::vector<Vertex>   tempVertices;
		std::vector<uint32_t> tempIndices;
		aiMatrix4x4           transform = scene->mRootNode->mTransformation;
		for (uint32_t vertexIndex{}; vertexIndex < mesh->mNumVertices; vertexIndex++)
		{
			aiVector3D const aiPosition = transform * mesh->mVertices[vertexIndex];

			Vertex tempVertex{};
			tempVertex.position = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);

			// TODO: normals, tangent, bitangent

			m_AABBMin.x = std::min(m_AABBMin.x, tempVertex.position.x);
			m_AABBMin.y = std::min(m_AABBMin.y, tempVertex.position.y);
			m_AABBMin.z = std::min(m_AABBMin.z, tempVertex.position.z);

			m_AABBMax.x = std::max(m_AABBMax.x, tempVertex.position.x);
			m_AABBMax.y = std::max(m_AABBMax.y, tempVertex.position.y);
			m_AABBMax.z = std::max(m_AABBMax.z, tempVertex.position.z);

			tempVertices.push_back(tempVertex);
		}
		for (uint32_t faceIndex{}; faceIndex < mesh->mNumFaces; faceIndex++)
		{
			aiFace const& face = mesh->mFaces[faceIndex];
			for (uint32_t index{}; index < face.mNumIndices; index++)
				tempIndices.push_back(face.mIndices[index]);
		}
		// TODO: materials

		Buffer& stagingVert = m_StagingBuffers.emplace_back(BufferBuilder{ m_Context }
															.MapMemory()
															.SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
																					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
															.Build(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
																   , tempVertices.size() * sizeof(tempVertices[0])));
		stagingVert.UpdateData(tempVertices);

		Buffer& stagingIndex = m_StagingBuffers.emplace_back(BufferBuilder{ m_Context }
															 .MapMemory()
															 .SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
																					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
															 .Build(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
																	, tempIndices.size() * sizeof(tempIndices[0])));
		stagingIndex.UpdateData(tempIndices);

		m_Meshes.emplace_back(m_Context, commandBuffer, std::move(tempVertices), stagingVert, std::move(tempIndices), stagingIndex);
	}
	for (uint32_t index{}; index < node->mNumChildren; index++)
		ProcessNode(node->mChildren[index], scene, commandBuffer);
}
