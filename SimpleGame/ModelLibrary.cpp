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
	float rotation,
	float screenRotation,
	float heightScale,
	float localRotation,
	float localPivotX,
	float localPivotY) const
{
	auto mesh = m_Meshes.find(meshName);
	if (mesh == m_Meshes.end())
		return;
	const float cosine = std::cos(rotation);
	const float sine = std::sin(rotation);
	const float screenCosine = std::cos(screenRotation);
	const float screenSine = std::sin(screenRotation);
	const float localCosine = std::cos(localRotation);
	const float localSine = std::sin(localRotation);
	struct RenderTriangle
	{
		ScreenPoint points[3];
		float depth;
		float red;
		float green;
		float blue;
	};
	std::vector<RenderTriangle> renderTriangles;
	renderTriangles.reserve(mesh->second.size());
	for (const MeshTriangle& triangle : mesh->second)
	{
		RenderTriangle rendered = {};
		float rotatedPositions[9] = {};
		for (int vertex = 0; vertex < 3; ++vertex)
		{
			const float sourceX = triangle.positions[vertex * 3];
			const float sourceY = triangle.positions[vertex * 3 + 1];
			const float localZ = triangle.positions[vertex * 3 + 2];
			const float pivotOffsetX = sourceX - localPivotX;
			const float pivotOffsetY = sourceY - localPivotY;
			const float localX = localPivotX +
				pivotOffsetX * localCosine - pivotOffsetY * localSine;
			const float localY = localPivotY +
				pivotOffsetX * localSine + pivotOffsetY * localCosine;
			const float rotatedX = localX * cosine - localY * sine;
			const float rotatedY = localX * sine + localY * cosine;
			rotatedPositions[vertex * 3] = rotatedX;
			rotatedPositions[vertex * 3 + 1] = rotatedY;
			rotatedPositions[vertex * 3 + 2] = localZ;
			const float projectedX = (rotatedX - rotatedY) * scale;
			// Match Game::WorldToScreen: increasing world X + Y moves toward the
			// lower half of the isometric plane, while positive height moves upward.
			// The previous positive planar term mirrored every mesh vertically, so
			// diagonal facing appeared opposite to its movement/aim direction.
			const float projectedY =
				-(rotatedX + rotatedY) * scale * 0.5f + localZ * scale * heightScale;
			rendered.points[vertex] = {x + projectedX * screenCosine - projectedY * screenSine,
				y + projectedX * screenSine + projectedY * screenCosine};
			rendered.depth += rendered.points[vertex].y;
		}
		rendered.depth /= 3.0f;
		const float edgeOneX = rotatedPositions[3] - rotatedPositions[0];
		const float edgeOneY = rotatedPositions[4] - rotatedPositions[1];
		const float edgeOneZ = rotatedPositions[5] - rotatedPositions[2];
		const float edgeTwoX = rotatedPositions[6] - rotatedPositions[0];
		const float edgeTwoY = rotatedPositions[7] - rotatedPositions[1];
		const float edgeTwoZ = rotatedPositions[8] - rotatedPositions[2];
		float normalX = edgeOneY * edgeTwoZ - edgeOneZ * edgeTwoY;
		float normalY = edgeOneZ * edgeTwoX - edgeOneX * edgeTwoZ;
		float normalZ = edgeOneX * edgeTwoY - edgeOneY * edgeTwoX;
		const float normalLength =
			std::sqrt(normalX * normalX + normalY * normalY + normalZ * normalZ);
		float light = 0.72f;
		if (normalLength > 0.00001f)
		{
			normalX /= normalLength;
			normalY /= normalLength;
			normalZ /= normalLength;
			const float diffuse = std::max(
				0.0f, normalX * -0.35f + normalY * -0.45f + normalZ * 0.82f);
			light = 0.56f + diffuse * 0.44f;
		}
		rendered.red = std::min(1.0f, triangle.red * light);
		rendered.green = std::min(1.0f, triangle.green * light);
		rendered.blue = std::min(1.0f, triangle.blue * light);
		renderTriangles.push_back(rendered);
	}
	std::stable_sort(renderTriangles.begin(),
		renderTriangles.end(),
		[](const RenderTriangle& left, const RenderTriangle& right) {
			return left.depth < right.depth;
		});
	for (const RenderTriangle& triangle : renderTriangles)
		renderer->DrawTriangle(triangle.points[0],
			triangle.points[1],
			triangle.points[2],
			triangle.red,
			triangle.green,
			triangle.blue,
			1.0f);
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
		const bool preserveModelOrigin = meshEntry.first == "void_claw_left" ||
			meshEntry.first == "void_claw_right";
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
		const float centerX = preserveModelOrigin ? 0.0f : (minimumX + maximumX) * 0.5f;
		const float centerY = preserveModelOrigin ? 0.0f : (minimumY + maximumY) * 0.5f;
		const float baseZ = preserveModelOrigin ? 0.0f : minimumZ;
		for (MeshTriangle& triangle : meshEntry.second)
		{
			for (int vertex = 0; vertex < 3; ++vertex)
			{
				triangle.positions[vertex * 3] -= centerX;
				triangle.positions[vertex * 3 + 1] -= centerY;
				triangle.positions[vertex * 3 + 2] -= baseZ;
			}
		}
	}
	return !m_Meshes.empty();
}
