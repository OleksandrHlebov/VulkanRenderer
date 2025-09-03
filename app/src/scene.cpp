#include "scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "datatypes.h"

#include <stdexcept>

#include "command_pool.h"
#include "helper.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"

Scene::Scene(vkc::Context& context, vkc::CommandPool& commandPool)
	: m_Context{ context }
	, m_CommandPool{ commandPool } {}

void Scene::Load(std::string_view filename)
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
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
	{
		if (aiMaterial const* material = scene->mMaterials[i];
			material->GetTextureCount(aiTextureType_NORMALS) > 0 &&
			material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0 &&
			material->GetTextureCount(aiTextureType_METALNESS) > 0)
		{
			m_ContainsPBRInfo = true;
			break;
		}
	}
	vkc::CommandBuffer& commandBuffer = m_CommandPool.AllocateCommandBuffer(m_Context);
	commandBuffer.Begin(m_Context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	ProcessNode(scene->mRootNode, scene, commandBuffer);
	commandBuffer.End(m_Context);
	commandBuffer.Submit(m_Context, m_Context.GraphicsQueue, {}, {});
	if (auto const result = m_Context.DispatchTable.waitForFences(1, &commandBuffer.GetFence(), VK_TRUE, UINT64_MAX);
		result != VK_SUCCESS)
		throw std::runtime_error("failed to wait for the fences");

	while (!m_StagingBuffers.empty())
	{
		m_StagingBuffers.top().Destroy(m_Context);
		m_StagingBuffers.pop();
	}
}

void Scene::LoadFirstMeshFromFile(std::string_view filename)
{
	std::string string{ filename };
	m_Meshes.clear();
}

glm::mat4 Scene::CalculateLightSpaceMatrix(glm::vec3 const& direction) const
{
	glm::vec3 const sceneCenter    = (m_AABBMin + m_AABBMax) * .5f;
	glm::vec3 const lightDirection = glm::normalize(glm::vec3{ -direction });

	std::vector<glm::vec3> const corners
	{
		{ m_AABBMin.x, m_AABBMin.y, m_AABBMin.z }
		, { m_AABBMax.x, m_AABBMin.y, m_AABBMin.z }
		, { m_AABBMin.x, m_AABBMax.y, m_AABBMin.z }
		, { m_AABBMax.x, m_AABBMax.y, m_AABBMin.z }
		, { m_AABBMin.x, m_AABBMin.y, m_AABBMax.z }
		, { m_AABBMax.x, m_AABBMin.y, m_AABBMax.z }
		, { m_AABBMin.x, m_AABBMax.y, m_AABBMax.z }
		, { m_AABBMax.x, m_AABBMax.y, m_AABBMax.z }
	};

	float minProj = FLT_MAX;
	float maxProj = FLT_MIN;
	for (glm::vec3 const& corner: corners)
	{
		const float proj = glm::dot(corner, lightDirection);
		minProj          = std::min(minProj, proj);
		maxProj          = std::max(maxProj, proj);
	}

	float const     distance = maxProj - glm::dot(sceneCenter, lightDirection);
	glm::vec3 const lightPos = sceneCenter - lightDirection * distance;

	glm::vec3 const up = glm::abs(glm::dot(lightDirection, glm::vec3(.0f, 1.f, .0f))) > .99f
						 ? glm::vec3(.0f, .0f, 1.f)
						 : glm::vec3(.0f, 1.f, .0f);

	glm::mat4 const view = glm::lookAt(lightPos, sceneCenter, up);

	glm::vec3 minLightSpace{ FLT_MAX };
	glm::vec3 maxLightSpace{ FLT_MIN };
	for (const glm::vec3& corner: corners)
	{
		auto const transformedCorner = glm::vec3{ view * glm::vec4(corner, 1.f) };
		minLightSpace                = glm::min(minLightSpace, transformedCorner);
		maxLightSpace                = glm::max(maxLightSpace, transformedCorner);
	}

	constexpr float near       = .0f;
	const float     far        = maxLightSpace.z - minLightSpace.z;
	glm::mat4       projection = glm::ortho(minLightSpace.x, maxLightSpace.x, minLightSpace.y, maxLightSpace.y, near, far);
	projection[1][1] *= -1;
	return projection * view;
}

uint32_t Scene::AddTextureToPool(vkc::Image&& image, vkc::ImageView&& imageView)
{
	m_TextureImages.emplace_back(std::move(image));
	m_TextureImageViews.emplace_back(std::move(imageView));
	return static_cast<uint32_t>(m_TextureImageViews.size() - 1);
}

void Scene::AddLight(Light light)
{
	m_LightData.Lights.emplace_back(light);
	if (!m_LightData.Lights.back().IsPoint())
	{
		m_LightData.LightSpaceMatrices.emplace_back(CalculateLightSpaceMatrix(light.m_Position));
		m_LightData.Lights.back().m_MatrixIndex = static_cast<uint32_t>(m_LightData.LightSpaceMatrices.size() - 1);
	}
	else
	{
		static uint32_t pointLightCount         = 0;
		m_LightData.Lights.back().m_MatrixIndex = pointLightCount++;
	}
	std::ranges::sort(m_LightData.Lights
					  , [](auto& l, auto& r)
					  {
						  return l.GetMatrixIndex() < r.GetMatrixIndex() && l.GetPosition().w < r.GetPosition().w
							  // && l.GetShadowMapIndex() < r.GetShadowMapIndex()
							  ;
					  });
}

void Scene::AddLight(glm::vec3 const& position, bool isPoint, glm::vec3 const& colour, float intensity)
{
	AddLight(Light{ position, isPoint, colour, intensity });
}

void Scene::ProcessNode(aiNode const* node, aiScene const* scene, vkc::CommandBuffer& commandBuffer)
{
	for (uint32_t meshIndex{}; meshIndex < node->mNumMeshes; meshIndex++)
	{
		aiMesh const* const   mesh = scene->mMeshes[node->mMeshes[meshIndex]];
		std::vector<Vertex>   tempVertices;
		std::vector<uint32_t> tempIndices;
		aiMatrix4x4           transform = scene->mRootNode->mTransformation;

		aiVector3D   translation{};
		aiQuaternion rotationQuat{};

		transform.DecomposeNoScaling(rotationQuat, translation);

		aiMatrix3x3 const rotation{ rotationQuat.GetMatrix() };

		for (uint32_t vertexIndex{}; vertexIndex < mesh->mNumVertices; vertexIndex++)
		{
			aiVector3D const aiPosition  = transform * mesh->mVertices[vertexIndex];
			aiVector3D const aiNormal    = rotation * mesh->mNormals[vertexIndex];
			aiVector3D const aiTangent   = rotation * mesh->mTangents[vertexIndex];
			aiVector3D const aiBitangent = rotation * mesh->mBitangents[vertexIndex];

			Vertex tempVertex{};
			tempVertex.Position  = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);
			tempVertex.UV        = glm::vec2(mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y);
			tempVertex.Normal    = glm::vec3(aiNormal.x, aiNormal.y, aiNormal.z);
			tempVertex.Tangent   = glm::vec3(aiTangent.x, aiTangent.y, aiTangent.z);
			tempVertex.Bitangent = glm::vec3(aiBitangent.x, aiBitangent.y, aiBitangent.z);

			m_AABBMin.x = std::min(m_AABBMin.x, tempVertex.Position.x);
			m_AABBMin.y = std::min(m_AABBMin.y, tempVertex.Position.y);
			m_AABBMin.z = std::min(m_AABBMin.z, tempVertex.Position.z);

			m_AABBMax.x = std::max(m_AABBMax.x, tempVertex.Position.x);
			m_AABBMax.y = std::max(m_AABBMax.y, tempVertex.Position.y);
			m_AABBMax.z = std::max(m_AABBMax.z, tempVertex.Position.z);

			tempVertices.push_back(tempVertex);
		}
		for (uint32_t faceIndex{}; faceIndex < mesh->mNumFaces; faceIndex++)
		{
			aiFace const& face = mesh->mFaces[faceIndex];
			for (uint32_t index{}; index < face.mNumIndices; index++)
				tempIndices.push_back(face.mIndices[index]);
		}

		TextureIndices textureIndices{};
		textureIndices.Diffuse = LoadTexture(aiTextureType_DIFFUSE, scene->mMaterials[mesh->mMaterialIndex], commandBuffer);
		textureIndices.Normals = LoadTexture(aiTextureType_NORMALS
											 , scene->mMaterials[mesh->mMaterialIndex]
											 , commandBuffer
											 , VK_FORMAT_R8G8B8A8_UNORM);
		textureIndices.Metalness = LoadTexture(aiTextureType_METALNESS, scene->mMaterials[mesh->mMaterialIndex], commandBuffer);
		textureIndices.Roughness = LoadTexture(aiTextureType_DIFFUSE_ROUGHNESS, scene->mMaterials[mesh->mMaterialIndex], commandBuffer);

		vkc::Buffer& stagingVert = m_StagingBuffers.emplace(vkc::BufferBuilder{ m_Context }
															.MapMemory()
															.SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
																					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
															.Build(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
																   , tempVertices.size() * sizeof(tempVertices[0])
																   , false));
		stagingVert.UpdateData(tempVertices);

		vkc::Buffer& stagingIndex = m_StagingBuffers.emplace(vkc::BufferBuilder{ m_Context }
															 .MapMemory()
															 .SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
																					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
															 .Build(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
																	, tempIndices.size() * sizeof(tempIndices[0])
																	, false));
		stagingIndex.UpdateData(tempIndices);

		m_Meshes.emplace_back(m_Context
							  , commandBuffer
							  , std::move(tempVertices)
							  , stagingVert
							  , std::move(tempIndices)
							  , stagingIndex
							  , textureIndices);
	}
	for (uint32_t index{}; index < node->mNumChildren; index++)
		ProcessNode(node->mChildren[index], scene, commandBuffer);
}

uint32_t Scene::LoadTexture(aiTextureType type, aiMaterial const* material, vkc::CommandBuffer const& commandBuffer, VkFormat format)
{
	aiString str;
	if (material->GetTextureCount(type))
		material->GetTexture(type, 0, &str);
	else
		str = aiString{ "200px-Debugempty.png" };

	if (!m_LoadedTextures.contains(str.C_Str()))
	{
		std::string const fullPath{ "data/textures/" + std::string{ str.C_Str() } };
		m_LoadedTextures[str.C_Str()] = static_cast<uint32_t>(m_TextureImages.size());

		help::ImageData const imageData{ help::LoadImage(fullPath) };

		std::span const span{ imageData.Pixels, imageData.Pixels + imageData.Size };

		vkc::Buffer& stagingBuffer = m_StagingBuffers.emplace(vkc::BufferBuilder{ m_Context }
															  .MapMemory()
															  .SetRequiredMemoryFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
																					  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
															  .Build(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageData.Size, false));
		stagingBuffer.UpdateData(span);

		vkc::Image& image = m_TextureImages.emplace_back(vkc::ImageBuilder{ m_Context }
														 .SetType(VK_IMAGE_TYPE_2D)
														 .SetFormat(format)
														 .SetAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
														 .SetExtent({
																		static_cast<uint32_t>(imageData.Width)
																		, static_cast<uint32_t>(imageData.Height)
																	})
														 .Build(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

		m_TextureImageViews.emplace_back(image.CreateView(m_Context, VK_IMAGE_VIEW_TYPE_2D));
		//
		{
			vkc::Image::Transition transition{};
			transition.NewLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transition.SrcAccessMask = VK_ACCESS_NONE;
			transition.DstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			transition.DstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
			image.MakeTransition(m_Context, commandBuffer, transition);
		}
		stagingBuffer.CopyTo(m_Context, commandBuffer, image);
		//
		{
			vkc::Image::Transition transition{};
			transition.NewLayout     = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
			transition.SrcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			transition.DstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
			transition.DstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			image.MakeTransition(m_Context, commandBuffer, transition);
		}
	}
	return m_LoadedTextures[str.C_Str()];
}
