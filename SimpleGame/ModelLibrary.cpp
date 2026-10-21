#include "stdafx.h"
#include "ModelLibrary.h"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>

bool ModelLibrary::Load()
{
	m_Models.clear();
	m_Meshes.clear();

	const char* paths[] = {
		"./Data/Models.txt",
		"./SimpleGame/Data/Models.txt",
		"../../SimpleGame/Data/Models.txt",
	};

	for (int index = 0; index < 3; ++index)
	{
		if (LoadFile(paths[index]))
			break;
	}
	const char* meshPaths[] = {"./Data/RuntimeMeshes.txt",
		"./SimpleGame/Data/RuntimeMeshes.txt",
		"../../SimpleGame/Data/RuntimeMeshes.txt"};
	for (int index = 0; index < 3; ++index)
		if (LoadMeshFile(meshPaths[index]))
			break;
	return !m_Models.empty() || !m_Meshes.empty();
}

void ModelLibrary::DrawMesh(PrototypeRenderer* renderer,
	const std::string& meshName,
	float x,
	float y,
	float scale,
	float rotation) const
{
	auto mesh = m_Meshes.find(meshName);
	if (mesh == m_Meshes.end())
		return;
	const float cosine = std::cos(rotation);
	const float sine = std::sin(rotation);
	for (const MeshTriangle& triangle : mesh->second)
	{
		ScreenPoint projected[3];
		for (int vertex = 0; vertex < 3; ++vertex)
		{
			const float localX = triangle.positions[vertex * 3];
			const float localY = triangle.positions[vertex * 3 + 1];
			const float localZ = triangle.positions[vertex * 3 + 2];
			const float rotatedX = localX * cosine - localY * sine;
			const float rotatedY = localX * sine + localY * cosine;
			projected[vertex] = {x + (rotatedX - rotatedY) * scale,
				y + (rotatedX + rotatedY) * scale * 0.5f + localZ * scale};
		}
		renderer->DrawTriangle(projected[0], projected[1], projected[2],
			triangle.red, triangle.green, triangle.blue, 1.0f);
	}
}

bool ModelLibrary::IsLoaded() const
{
	return !m_Models.empty() || !m_Meshes.empty();
}

void ModelLibrary::Draw(
	PrototypeRenderer* renderer, const std::string& modelName, float x, float y, float scale) const
{
	std::unordered_map<std::string, std::vector<ModelPrimitive>>::const_iterator model =
		m_Models.find(modelName);

	if (model == m_Models.end())
		return;

	for (size_t index = 0; index < model->second.size(); ++index)
	{
		const ModelPrimitive& primitive = model->second[index];
		float drawX = x + primitive.offsetX * scale;
		float drawY = y + primitive.offsetY * scale;
		float width = primitive.width * scale;
		float height = primitive.height * scale;

		if (primitive.type == ModelRect)
		{
			renderer->DrawRect(drawX,
				drawY,
				width,
				height,
				primitive.red,
				primitive.green,
				primitive.blue,
				primitive.alpha);
		}
		else if (primitive.type == ModelDiamond)
		{
			renderer->DrawDiamond(drawX,
				drawY,
				width,
				height,
				primitive.red,
				primitive.green,
				primitive.blue,
				primitive.alpha);
		}
		else
		{
			renderer->DrawEllipse(drawX,
				drawY,
				width,
				height,
				primitive.red,
				primitive.green,
				primitive.blue,
				primitive.alpha);
		}
	}
}

bool ModelLibrary::LoadFile(const char* path)
{
	std::ifstream file(path);
	if (!file.is_open())
		return false;

	std::string currentModel;
	std::string line;

	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;

		std::istringstream stream(line);
		std::string command;
		stream >> command;

		if (command == "model")
		{
			stream >> currentModel;
			m_Models[currentModel] = std::vector<ModelPrimitive>();
			continue;
		}

		if (currentModel.empty())
			continue;

		ModelPrimitive primitive = {};
		if (command == "rect")
			primitive.type = ModelRect;
		else if (command == "diamond")
			primitive.type = ModelDiamond;
		else if (command == "ellipse")
			primitive.type = ModelEllipse;
		else
			continue;

		if (stream >> primitive.offsetX >> primitive.offsetY >> primitive.width >>
			primitive.height >> primitive.red >> primitive.green >> primitive.blue >>
			primitive.alpha)
		{
			m_Models[currentModel].push_back(primitive);
		}
	}

	return !m_Models.empty();
}

bool ModelLibrary::LoadMeshFile(const char* path)
{
	std::ifstream file(path);
	if (!file.is_open())
		return false;
	std::string currentMesh;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		std::istringstream stream(line);
		std::string command;
		stream >> command;
		if (command == "mesh")
		{
			stream >> currentMesh;
			m_Meshes[currentMesh] = std::vector<MeshTriangle>();
			continue;
		}
		if (command != "tri" || currentMesh.empty())
			continue;
		MeshTriangle triangle = {};
		bool valid = true;
		for (int value = 0; value < 9; ++value)
			valid = valid && static_cast<bool>(stream >> triangle.positions[value]);
		valid = valid && static_cast<bool>(stream >> triangle.red >> triangle.green >> triangle.blue);
		if (valid)
			m_Meshes[currentMesh].push_back(triangle);
	}
	for (auto& meshEntry : m_Meshes)
	{
		float minimumX = 1.0e9f;
		float maximumX = -1.0e9f;
		float minimumY = 1.0e9f;
		float maximumY = -1.0e9f;
		float minimumZ = 1.0e9f;
		for (const MeshTriangle& triangle : meshEntry.second)
		{
			for (int vertex = 0; vertex < 3; ++vertex)
			{
				minimumX = std::min(minimumX, triangle.positions[vertex * 3]);
				maximumX = std::max(maximumX, triangle.positions[vertex * 3]);
				minimumY = std::min(minimumY, triangle.positions[vertex * 3 + 1]);
				maximumY = std::max(maximumY, triangle.positions[vertex * 3 + 1]);
				minimumZ = std::min(minimumZ, triangle.positions[vertex * 3 + 2]);
			}
		}
		const float centerX = (minimumX + maximumX) * 0.5f;
		const float centerY = (minimumY + maximumY) * 0.5f;
		for (MeshTriangle& triangle : meshEntry.second)
		{
			for (int vertex = 0; vertex < 3; ++vertex)
			{
				triangle.positions[vertex * 3] -= centerX;
				triangle.positions[vertex * 3 + 1] -= centerY;
				triangle.positions[vertex * 3 + 2] -= minimumZ;
			}
		}
	}
	return !m_Meshes.empty();
}
