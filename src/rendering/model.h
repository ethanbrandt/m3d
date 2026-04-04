#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <map>
#include <vector>
#include <cstring>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>

#include "shader.h"
#include "mesh.h"

inline unsigned int texture_from_file(const char *path, const std::string &directory, bool gamma = false)
{
	auto current_time = std::chrono::steady_clock::now();
	(void)gamma;
	const std::filesystem::path filename = std::filesystem::path(directory) / path;

	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	const std::string texturePath = filename.string();
	unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &nrComponents, 0);

	if (data)
	{
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << texturePath << std::endl;
		stbi_image_free(data);
	}

	std::chrono::duration<float> elapsed_duration = std::chrono::steady_clock::now() - current_time;
	std::cout << "texture_from_file runtime: " << elapsed_duration.count() << '\n';
	return textureID;
}

class Model
{
public:
	explicit Model(const std::string& path)
	{
		load_model(path);
	}

	void draw(Shader &shader)
	{
		for (unsigned int i = 0; i < meshes.size(); i++)
			meshes[i].draw(shader);
	}

private:
	inline static std::vector<Texture> textures_loaded;
	std::vector<Mesh> meshes;
	std::string directory;


	void load_model(const std::string& path)
	{
		std::string fileExtension = path.substr(path.find_last_of('.'), path.size());

		if (fileExtension == ".obj")
			stbi_set_flip_vertically_on_load(true);
		else if (fileExtension == ".gltf")
			stbi_set_flip_vertically_on_load(false);

		auto current_time = std::chrono::steady_clock::now();
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "[ASSIMP Error] " << importer.GetErrorString() << '\n';
			return;
		}

		const std::filesystem::path modelPath(path);
		directory = modelPath.has_parent_path() ? modelPath.parent_path().string() : ".";

		process_node(scene->mRootNode, scene);
		std::chrono::duration<float> elapsed_duration = std::chrono::steady_clock::now() - current_time;
		std::cout << "load_model runtime: " << elapsed_duration.count() << '\n';
	}

	void process_node(aiNode* node, const aiScene* scene)
	{
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(process_mesh(mesh, scene));
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++)
			process_node(node->mChildren[i], scene);
	}

	Mesh process_mesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<Texture> textures;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			glm::vec3 vector;
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.position = vector;

			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.normal = vector;

			if (mesh->mTextureCoords[0])
			{
				glm::vec2 texVec;
				texVec.x = mesh->mTextureCoords[0][i].x;
				texVec.y = mesh->mTextureCoords[0][i].y;
				vertex.texCoords = texVec;
			}
			else
				vertex.texCoords = glm::vec2(0.0f, 0.0f);

			vertices.push_back(vertex);
		}


		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	
		std::vector<Texture> diffuseMaps = load_material_textures(material, aiTextureType_DIFFUSE, "texture_diffuse");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

		std::vector<Texture> specularMaps = load_material_textures(material, aiTextureType_SPECULAR, "texture_specular");
		textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

		return Mesh(std::move(vertices), std::move(indices), std::move(textures));
	}

	std::vector<Texture> load_material_textures(aiMaterial* mat, aiTextureType type, std::string typeName)
	{
		std::vector<Texture> textures;
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
		{
			aiString str;
			mat->GetTexture(type, i, &str);

			bool skip = false;
			for (unsigned int j = 0; j < textures_loaded.size(); j++)
			{
				if (textures_loaded[j].path == str.C_Str())
				{
					textures.push_back(textures_loaded[j]);
					skip = true;
					break;
				}
			}

			if (skip)
				continue;

			Texture texture;
			texture.id = texture_from_file(str.C_Str(), directory);
			texture.type = typeName;
			texture.path = str.C_Str();
			textures.push_back(texture);
			textures_loaded.push_back(texture);
		}
		return textures;
	}
};
