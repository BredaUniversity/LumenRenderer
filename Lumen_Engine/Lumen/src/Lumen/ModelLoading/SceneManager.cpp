#include "lmnpch.h"
#include "SceneManager.h"
#include "Node.h"
#include "Transform.h"
#include "../Renderer/ILumenResources.h"
#include "stb_image.h"
//#include "../../LumenPT/src/Framework/OptiXRenderer.h"

#include <glm/gtc/type_ptr.hpp>

//#include <string>
#include <memory>

Lumen::SceneManager::~SceneManager()
{
	for (auto loadedScene : m_LoadedScenes)
	{
		// Release the meshes and materials from each GLTFResource manually
		auto& res = loadedScene.second;

		for (auto& mesh : res.m_MeshPool)
			mesh.reset();

		for (auto& material : res.m_MaterialPool)
			material.reset();
	}

	// Release the meshes and materials which are no longer parts of GLTFResources
	// due to being in use by the renderer

	for (auto& inUseMesh : m_InUseMeshes)
		inUseMesh.reset();

	for (auto& inUseMaterial : m_InUseMaterials)
		inUseMaterial.reset();
}

Lumen::SceneManager::GLTFResource* Lumen::SceneManager::LoadGLTF(std::string a_FileName, std::string a_Path, const glm::mat4& a_TransformMat)
{
	const auto fullPath = a_Path + a_FileName;
	auto findIter = m_LoadedScenes.find(fullPath);

	if (findIter != m_LoadedScenes.end())
	{
		return &(*findIter).second;
	}

	auto& res = m_LoadedScenes[fullPath];		// create new scene at path key

	//Check for glb or gltf
	const std::string binarySuffix = ".glb";
	bool isBinary = (a_FileName.length() >= binarySuffix.length()) && (0 == a_FileName.compare(a_FileName.length() - binarySuffix.length(), binarySuffix.length(), binarySuffix));

	//NOTE: No quotas specified and no check for .gltf suffix. Might fail to load with large files and wrongly specified suffix.
	fx::gltf::Document doc;
	if (!isBinary)
	{
		doc = fx::gltf::LoadFromText(fullPath);
	}
	else
	{
		doc = fx::gltf::LoadFromBinary((fullPath));
	}

	res.m_Path = fullPath;


	LoadMaterials(doc, res, a_Path);

	LoadMeshes(doc, res);

	LoadScenes(doc, res);

	//Loop over the root nodes in every scene in this GLTF file, and then add them as instances.
	for (int sceneId = 0; sceneId < doc.scenes.size(); ++sceneId)
	{
		auto& scene = doc.scenes[sceneId];
		for (auto& rootNodeId : scene.nodes)
		{
			LoadNodes(doc, res, rootNodeId, true, a_TransformMat);
		}
	}

	return &res;
}

void Lumen::SceneManager::SetPipeline(LumenRenderer& a_Renderer)
{
	m_RenderPipeline = &a_Renderer;
	m_VolumeManager.SetPipeline(a_Renderer);

	InitializeDefaultResources();
}

void Lumen::SceneManager::ClearUnusedAssets()
{
	for (auto loadedScene : m_LoadedScenes)
	{
		auto& res = loadedScene.second;

		for (auto& mesh : res.m_MeshPool)
		{
			if (mesh.use_count() > 1) // Are there other shared pointer instances to this mesh?
			{
				// If yes, store the mesh separately from the GLTF resource
				m_InUseMeshes.push_back(mesh);
			}
			else
			{
				// Otherwise, reset the pointer, thus destroying the mesh
				mesh.reset();
			}
		}

		for (auto& material : res.m_MaterialPool)
		{
			if (material.use_count() > 1) // Are there other shared pointer instances to this material?
			{
				// If yes, store the material separately from the GLTF resource
				m_InUseMaterials.push_back(material);
			}
			else
			{
				// Otherwise, reset the pointer, thus destroying the material
				material.reset();
			}
		}
	}

	// Verify that none of the meshes and materials that are stored separately
	// have been left unused since the last call to this function

	for (auto& mesh : m_InUseMeshes)
	{
		if (mesh.use_count() <= 1) // Are there no other shared pointer instances to this mesh?
		{
			// If there are none, this mesh can be safely deleted.
			mesh.reset();
		}
	}

	for (auto& material : m_InUseMaterials)
	{
		if (material.use_count() <= 1) // Are there no other shared pointer instances to this material?
		{
			// If there are none, this material can be safely deleted.
			material.reset();
		}
	}
}

void Lumen::SceneManager::InitializeDefaultResources()
{
	uchar4 whitePixel = { 255,255,255,255 };
	uchar4 diffusePixel{ 0, 255, 255, 0};
	uchar4 normal = { 128, 128, 255, 0 };
	m_DefaultDiffuseTexture = m_RenderPipeline->CreateTexture(&whitePixel, 1, 1);
	m_DefaultMetalRoughnessTexture = m_RenderPipeline->CreateTexture(&diffusePixel, 1, 1);
	m_DefaultNormalTexture = m_RenderPipeline->CreateTexture(&normal, 1, 1);
	m_DefaultEmissiveTexture = m_RenderPipeline->CreateTexture(&whitePixel, 1, 1);
}

void Lumen::SceneManager::LoadNodes(fx::gltf::Document& a_Doc, GLTFResource& a_Res, int a_NodeId, bool a_Root, const glm::mat4& a_TransformMat)
{
	const static glm::mat4 IDENTITY = glm::identity<glm::mat4>();

	auto& node = a_Doc.nodes[a_NodeId];
	glm::mat4 transform = glm::make_mat4(&node.matrix[0]);

	//If the matrix is not defined, load from the other settings.
	if (IDENTITY == transform)
	{
		auto translation = glm::vec3(
			node.translation[0],
			node.translation[1],
			node.translation[2]
		);

		auto rotation = glm::quat(
			node.rotation[0],
			node.rotation[1],
			node.rotation[2],
			node.rotation[3]
		);

		auto scale = glm::vec3(
			node.scale[0],
			node.scale[1],
			node.scale[2]
		);

		Transform t;
		t.SetPosition(translation);
		t.SetRotation(rotation);
		t.SetScale(scale);

		transform = t.GetTransformationMatrix();
	}

	const glm::mat4 chainedTransform = a_TransformMat * transform;

	if (a_Root)
	{
		a_Res.m_RootNodeIndices.push_back(a_NodeId);
	}

	std::shared_ptr<Node> newNode = std::make_shared<Node>();
	newNode->m_MeshID = node.mesh;
	newNode->m_Name = node.name;
	newNode->m_NodeID = static_cast<int>(a_NodeId);
	newNode->m_LocalTransform = std::make_unique<Transform>(chainedTransform);

	for (int i = 0; i < static_cast<int>(node.children.size()); i++)
	{
		newNode->m_ChilIndices.push_back(node.children.at(i));
	}

	a_Res.m_NodePool.push_back(newNode);

	//If there is child meshes, recursively call.
	if (!node.children.empty())
	{
		for (auto& id : node.children)
		{
			LoadNodes(a_Doc, a_Res, id, false, chainedTransform);
		}
	}

	////store offsets in the case there is somehow already data loaded in the scene object
	//const int nodeOffset = static_cast<int>(a_Res.m_NodePool.size()) + 1;
	//const int meshOffset = static_cast<int>(a_Res.m_MeshPool.size());

	//std::vector<std::shared_ptr<Node>> nodes;	//only supporting one scene per file. Seems to work fine 99% of the time

	//std::shared_ptr<Node> baseNode = std::make_shared<Node>();
	//baseNode->m_NodeID = nodeOffset - 1;
	//baseNode->m_LocalTransform = std::make_unique<Transform>(a_TransformMat);
	//nodes.push_back(baseNode);

	//for (auto& fxNodeIdx : a_Doc.scenes.at(a_SceneId).nodes)
	//{
	//	const fx::gltf::Node& fxNode = a_Doc.nodes.at(fxNodeIdx);

	//	std::shared_ptr<Node> newNode = std::make_shared<Node>();
	//	newNode->m_MeshID = -1 ? -1 : (fxNode.mesh + meshOffset);
	//	newNode->m_Name = fxNode.name;
	//	newNode->m_NodeID = static_cast<int>(nodes.size());
	//	newNode->m_LocalTransform = std::make_unique<Transform>();

	//	for (int i = 0; i < static_cast<int>(fxNode.children.size()); i++)
	//	{
	//		newNode->m_ChilIndices.push_back(fxNode.children.at(i) + nodeOffset);
	//	}

	//NOTE: .size() returns the amount of elements (templated) at compile time. It's not the amount of valid values!
	//	if (fxNode.translation.size() == 3)
	//	{
	//		newNode->m_LocalTransform->SetPosition(glm::vec3(
	//			fxNode.translation[0],
	//			fxNode.translation[1],
	//			fxNode.translation[2]
	//		));
	//	}

	//	if (fxNode.rotation.size() == 4)
	//	{
	//		newNode->m_LocalTransform->SetRotation(glm::quat(
	//			fxNode.rotation[0],
	//			fxNode.rotation[1],
	//			fxNode.rotation[2],
	//			fxNode.rotation[3]
	//		));
	//	}

	//	if (fxNode.scale.size() == 3)
	//	{
	//		newNode->m_LocalTransform->SetScale(glm::vec3(
	//			fxNode.scale[0],
	//			fxNode.scale[1],
	//			fxNode.scale[2]
	//		));
	//	}

	//	a_Res.m_NodePool.push_back(newNode);
	//}
}

void Lumen::SceneManager::LoadMeshes(fx::gltf::Document& a_Doc, GLTFResource& a_Res)
{
	const int meshOffset = static_cast<int>(a_Res.m_MeshPool.size());

	std::vector<std::shared_ptr<ILumenMesh>> meshes;
	// pass binary data into mesh

	// For each mesh in file
	for (auto& fxMesh : a_Doc.meshes)
	{
		// List of primitives in the mesh to fill out
		std::vector<std::shared_ptr<Lumen::ILumenPrimitive>> primitives;
		// For each primitive in mesh
		for (auto& fxPrim : fxMesh.primitives)
		{
			// Extract the binary data of the primitive
			std::vector<uint8_t> posBinary, texBinary, norBinary, tanBinary;
			// For each attribute in the primitive
			for (auto& fxAttribute : fxPrim.attributes)
			{
				if (fxAttribute.first == "POSITION")
				{
					// store position accessor
					//fxAttribute.second

					posBinary = LoadBinary(a_Doc, fxAttribute.second);
					//make vertex buffer of this, make function

				}
				else if (fxAttribute.first == "TEXCOORD_0")
				{
					texBinary = LoadBinary(a_Doc, fxAttribute.second);
				}
				else if (fxAttribute.first == "COLOR_0")
				{

				}
				else if (fxAttribute.first == "NORMAL")
				{
					norBinary = LoadBinary(a_Doc, fxAttribute.second);
				}
				else if (fxAttribute.first == "TANGENT")
				{
					tanBinary = LoadBinary(a_Doc, fxAttribute.second);
				}

			}

			// Get binary data for the primitive's indices if those are included
			auto indexBufferAcc = a_Doc.accessors[fxPrim.indices];
			std::vector<unsigned char> indexBin = LoadBinary(a_Doc, fxPrim.indices);
			auto indexSize = GetComponentSize(indexBufferAcc); // indices are are always a single component
			assert(indexSize <= 4);

			assert(fxPrim.mode == fx::gltf::Primitive::Mode::Triangles);

			VectorView<glm::vec3, uint8_t> vertexView(posBinary);
			VectorView<glm::vec2, uint8_t> texView(texBinary);
			VectorView<uint32_t, uint8_t> indexView32(indexBin);
			VectorView<uint16_t, uint8_t> indexView16(indexBin);
			VectorView<glm::vec3, uint8_t> normalView(norBinary);

			//If no tangents were loaded, generate them.
			if (tanBinary.empty())
			{
				const auto numIndices = indexBin.size() / indexSize;
				tanBinary.resize(numIndices * sizeof(glm::vec4));

				VectorView<glm::vec4, uint8_t> tangentView(tanBinary);

				////Invert the V coordinates
				for(auto i = 0; i < texView.Size(); ++i)
				{
					texView[i].y = 1.f - texView[i].y;
				}

				//Loop over every triangle in the index buffer.
				for (auto index = 0u; index < numIndices; index += 3)
				{
					//Retrieve the indices from the index buffer.
					unsigned int index1;
					unsigned int index2;
					unsigned int index3;

					if (indexSize == 2)
					{
						index1 = indexView16[index + 0];
						index2 = indexView16[index + 1];
						index3 = indexView16[index + 2];
					}
					else
					{
						index1 = indexView32[index + 0];
						index2 = indexView32[index + 1];
						index3 = indexView32[index + 2];
					}

					//Can't have a triangle that has two points the same. That's called a line.
					if(index1 == index2 || index2 == index3 || index3 == index1)
					{
						printf("Warning: Invalid vertex indices found.\n");
					}

					//Thanks to opengl-tutorial.com
					const glm::vec3* v0 = &vertexView[index1];
					const glm::vec3* v1 = &vertexView[index2];
					const glm::vec3* v2 = &vertexView[index3];
					glm::vec2* uv0 = &texView[index1];
					glm::vec2* uv1 = &texView[index2];
					glm::vec2* uv2 = &texView[index3];

					glm::vec3 deltaPos1 = *v1 - *v0;
					glm::vec3 deltaPos2 = *v2 - *v0;

					glm::vec2 deltaUV1 = *uv1 - *uv0;
					glm::vec2 deltaUV2 = *uv2 - *uv0;

					float cross = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;

					//If cross is 0, it means the texture coordinates are a single point for two corners.
				    assert(cross != 0);

					float r = 1.0f / cross;

					glm::vec3 tangent = glm::vec3((deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r);

					assert(!isnan(tangent.x));
					assert(!isnan(tangent.y));
					assert(!isnan(tangent.z));
					assert(glm::length(glm::vec3(tangent.x, tangent.y, tangent.z)) > 0.f);
					//
					if(isnan(tangent.x) || isnan(tangent.y) || isnan(tangent.z) || isinf(tangent.x) || isinf(tangent.y) || isinf(tangent.z))
					{
						tangent = glm::vec3(1.f, 0.f, 0.f);
					}

					//Normalize the tangent.
					tangent = glm::normalize(tangent);

					auto tanVec4 = glm::vec4(tangent, 1.f);

					//Put in the output buffer. Same tangent for every vertex that was processed.
					tangentView[index1] = tanVec4;
					tangentView[index2] = tanVec4;
					tangentView[index3] = tanVec4;
				}
			}


			// Fill out the primitive data struct and make a primitive through the renderer
			LumenRenderer::PrimitiveData primitiveData;
			primitiveData.m_Positions = posBinary;
			primitiveData.m_TexCoords = texBinary;
			primitiveData.m_Normals = norBinary;
			primitiveData.m_Tangents = tanBinary;

			primitiveData.m_IndexBinary = indexBin;
			primitiveData.m_IndexSize = indexSize;

			primitiveData.m_Material = a_Res.m_MaterialPool[fxPrim.material];

			auto newPrim = m_RenderPipeline->CreatePrimitive(primitiveData);

			// Add the primitive to the primitive list
			primitives.push_back(std::move(newPrim));
		}
		// Create a mesh from the primitives
		meshes.push_back(m_RenderPipeline->CreateMesh(primitives));
	}

	// Save all the meshes into the GLTFResource
	a_Res.m_MeshPool = meshes;

}

void Lumen::SceneManager::LoadScenes(fx::gltf::Document& a_Doc, GLTFResource& a_Res)
{
	int32_t counter = 0;
	// For each scene in the document
	for (auto& scene : a_Doc.scenes)
	{
		// Create a renderer-specific scene
		auto lumenScene = m_RenderPipeline->CreateScene();
        if (scene.name.empty())
        {
			lumenScene->m_Name = "Unnamed scene " + std::to_string(counter++);
        }
		else
		    lumenScene->m_Name = scene.name;            

		// And load all of the scene's root nodes
		for (auto& rootNode : scene.nodes)
		{
			LoadNodeAndChildren(a_Doc, a_Res, *lumenScene, rootNode);
		}

		// Save the scene into the resource
		a_Res.m_Scenes.push_back(lumenScene);
	}
}

void Lumen::SceneManager::LoadNodeAndChildren(fx::gltf::Document a_Doc, Lumen::SceneManager::GLTFResource a_Res, ILumenScene& a_Scene, uint32_t a_NodeID, Lumen::Transform a_ParentTransform)
{
	auto node = a_Doc.nodes[a_NodeID];
	auto nodeTransform = LoadNodeTransform(node);

	// Calculate the world space transform of this node taking into account the parent's world space transform
	nodeTransform = a_ParentTransform * nodeTransform;

	// If the node has a mesh, we add the mesh to the scene and give it the node's transform
	if (node.mesh != -1)
	{
		auto newMesh = a_Scene.AddMesh();
		newMesh->SetMesh(a_Res.m_MeshPool[node.mesh]);
		newMesh->m_Transform = nodeTransform;
	}

	// Recursively load all of the node's children
	for (auto& child : node.children)
	{
		LoadNodeAndChildren(a_Doc, a_Res, a_Scene, child, nodeTransform);
	}
}

Lumen::Transform Lumen::SceneManager::LoadNodeTransform(fx::gltf::Node a_Node)
{
	const static glm::mat4 identity = glm::identity<glm::mat4>();
	auto nodeMat = glm::make_mat4(a_Node.matrix.data());

	Transform transform;

	// The GLTF spec is quite ambiguous about the transform, but this appears to work
	// If the matrix representation is an identity matrix, it seems safe to assume that the transform is represented by the TRS
	if (nodeMat == identity)
	{
		auto t = a_Node.translation;
		auto r = a_Node.rotation;
		auto s = a_Node.scale;
		transform.SetPosition(glm::vec3(t[0], t[1], t[2]));
		transform.SetRotation(glm::quat(r[3], r[0], r[1], r[2]));
		transform.SetScale(glm::vec3(s[0], s[1], s[2]));
	}
	else
	{
		transform = nodeMat;
	}
	return transform;
}

Lumen::LoadedImageInformation Lumen::SceneManager::LoadTexture(fx::gltf::Document& a_File, int a_TextureId,
	const std::string& a_Path, int a_NumChannels)
{
	assert(a_TextureId >= 0);

	std::uint8_t* imgData = nullptr;
	int w = 0;
	int h = 0;
	int channels = 0;


	ImageData data(a_File, a_TextureId, a_Path);
	auto info = data.Info();

	stbi_set_flip_vertically_on_load(false);
	if (info.IsBinary())
	{
		//Load from raw
		imgData = stbi_load_from_memory(info.BinaryData, info.BinarySize, &w, &h, &channels, a_NumChannels);
	}
	else
	{
		//Load from file.
		imgData = stbi_load(info.FileName.c_str(), &w, &h, &channels, a_NumChannels);
	}
	assert(imgData != nullptr && "Could not load and decode image for some reason.");

	//If a specific number of channels was requested, overwrite the "would have been" channels count.
	if (a_NumChannels != 0)
	{
		channels = a_NumChannels;
	}
	return LoadedImageInformation{ imgData, w, h, channels };
}

std::vector<uint8_t> Lumen::SceneManager::LoadBinary(fx::gltf::Document& a_Doc, uint32_t a_AccessorIndx)
{
	std::vector<unsigned char> data;
	// Load raw data at accessor index
	auto bufferAccessor = a_Doc.accessors[a_AccessorIndx];
	auto bufferView = a_Doc.bufferViews[bufferAccessor.bufferView];
	auto buffer = a_Doc.buffers[bufferView.buffer];

	uint32_t compCount = GetComponentCount(bufferAccessor);
	uint32_t compSize = GetComponentSize(bufferAccessor);
	uint32_t attSize = compCount * compSize;

	// Sometimes bufferView.byteStride is 0, so we pick the biggest of the two
	// That way this works for interleaved buffers as well
	uint32_t stride = std::max(attSize, bufferView.byteStride);

	// Resize the output buffer to fit all the data that will be extracted
	data.resize(bufferView.byteLength / stride * static_cast<uint64_t>(attSize));

	// Offset from which to read in the buffer
	auto bufferOffset = 0;
	// Buffer offset specified in the file
	auto gltfBufferOffset = bufferView.byteOffset + bufferAccessor.byteOffset;

	// Copy over the binary data via a loop
	for (uint32_t i = 0; i < bufferAccessor.count; i++)
	{
		memcpy(data.data() + bufferOffset, buffer.data.data() + gltfBufferOffset + bufferOffset, attSize);
		bufferOffset += stride;
	}

	return data;
}

void Lumen::SceneManager::LoadMaterials(fx::gltf::Document& a_Doc, GLTFResource& a_Res, const std::string& a_Path)
{
	std::vector<std::shared_ptr<ILumenMaterial>> materials;	// Get these iLumenMats from OptiXRenderer
	//needs to be a more specific material implementation
	// with actual roughness values etc. 

	for (auto& fxMat : a_Doc.materials)
	{
		LumenRenderer::MaterialData matData;
		auto& mat = materials.emplace_back(m_RenderPipeline->CreateMaterial(matData));

		//Load the diffuse color if not empty.
		if (!fxMat.pbrMetallicRoughness.baseColorFactor.empty()) {
			auto& arr = fxMat.pbrMetallicRoughness.baseColorFactor;
			mat->SetDiffuseColor(glm::vec4(
				arr[0],
				arr[1],
				arr[2],
				arr[3]
			));
		}

		// Check if the diffuse texture is specified
		if (fxMat.pbrMetallicRoughness.baseColorTexture.index != -1)
		{
			//auto& fxTex = a_Doc.images.at(fxMat.pbrMetallicRoughness.baseColorTexture.index);
			//int x, y, c;
			//const auto file = (a_Res.m_Path + "/../" + fxTex.uri);
			//const auto stbTex = stbi_load(file.c_str(), &x, &y, &c, 4);
			//const auto tex = m_RenderPipeline->CreateTexture(stbTex, x, y);


			//Load the texture either from file or binary. Then create the engine texture object.
			auto info = LoadTexture(a_Doc, fxMat.pbrMetallicRoughness.baseColorTexture.index, a_Path, 4);

			const auto tex = m_RenderPipeline->CreateTexture(info.data, info.w, info.h);
			mat->SetDiffuseTexture(tex);

			//Free the memory after it's uploaded.
			stbi_image_free(info.data);
		}
		else
		{
			// If it isn't, set the diffuse texture to a default white one
			mat->SetDiffuseTexture(m_DefaultDiffuseTexture);
		}

		//Metallic/roughness value
		if (fxMat.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
		{
			//Load the texture either from file or binary. Then create the engine texture object.
			auto info = LoadTexture(a_Doc, fxMat.pbrMetallicRoughness.metallicRoughnessTexture.index, a_Path, 4);

			//Clamp perfect mirrors to be within bounds.
			for(int index = 0; index < info.w * info.h; ++index)
			{
				uchar4* data = reinterpret_cast<uchar4*>(&info.data[index * 4]);
				data->y = std::max(data->y, static_cast<unsigned char>(1));
			}

			const auto tex = m_RenderPipeline->CreateTexture(info.data, info.w, info.h);
			mat->SetMetalRoughnessTexture(tex);

			//Free the memory after it's uploaded.
			stbi_image_free(info.data);
		}
		else
		{
			mat->SetMetalRoughnessTexture(m_DefaultMetalRoughnessTexture);
		}

		//Normal map
		if (fxMat.normalTexture.index != -1)
		{
			//Load the texture either from file or binary. Then create the engine texture object.
			auto info = LoadTexture(a_Doc, fxMat.normalTexture.index, a_Path, 4);
			const auto tex = m_RenderPipeline->CreateTexture(info.data, info.w, info.h);
			mat->SetNormalTexture(tex);

			//Free the memory after it's uploaded.
			stbi_image_free(info.data);
		}
		else
		{
			mat->SetNormalTexture(m_DefaultNormalTexture);
		}

		glm::vec3 emission = { 0.f, 0.f, 0.f };

		if (fxMat.emissiveFactor != std::array<float, 3>{0.0f, 0.0f, 0.0f})
		{
			emission = glm::vec3(
				fxMat.emissiveFactor.at(0),
				fxMat.emissiveFactor.at(1),
				fxMat.emissiveFactor.at(2)
			);
		}

		mat->SetEmission(emission);

		if (fxMat.emissiveTexture.index != -1)
		{
			auto info = LoadTexture(a_Doc, fxMat.emissiveTexture.index, a_Path, 4);
			const auto tex = m_RenderPipeline->CreateTexture(info.data, info.w, info.h);

			mat->SetEmissiveTexture(tex);

			//LMN_INFO("Emissive texture present");

			stbi_image_free(info.data);
		}
		else
		{
			mat->SetEmissiveTexture(m_DefaultEmissiveTexture);
		}


		//materials.push_back(mat);
	}

	a_Res.m_MaterialPool = materials;
}

uint32_t Lumen::SceneManager::GetComponentSize(fx::gltf::Accessor& a_Accessor)
{

	switch (a_Accessor.componentType)
	{
	case fx::gltf::Accessor::ComponentType::Byte:
	case fx::gltf::Accessor::ComponentType::UnsignedByte:
		return 1;
		break;
	case fx::gltf::Accessor::ComponentType::Short:
	case fx::gltf::Accessor::ComponentType::UnsignedShort:
		return 2;
		break;
	case fx::gltf::Accessor::ComponentType::Float:
	case fx::gltf::Accessor::ComponentType::UnsignedInt:
		return 4;
		break;
	default:
		LMN_ASSERT(0 && "Failed to load GLTF file");
		return 0;
	}

}

uint32_t Lumen::SceneManager::GetComponentCount(fx::gltf::Accessor& a_Accessor)
{
	switch (a_Accessor.type)
	{
	case fx::gltf::Accessor::Type::Scalar:
		return 1;
		break;
	case fx::gltf::Accessor::Type::Vec2:
		return 2;
		break;
	case fx::gltf::Accessor::Type::Vec3:
		return 3;
		break;
	case fx::gltf::Accessor::Type::Vec4:
		return 4;
		break;
	case fx::gltf::Accessor::Type::Mat2:
		return 4;
		break;
	case fx::gltf::Accessor::Type::Mat3:
		return 9;
		break;
	case fx::gltf::Accessor::Type::Mat4:
		return 16;
		break;
	default:
		LMN_ASSERT(0 && "Failed to load GLTF file");
		assert(0);
		return 0;
	}
}