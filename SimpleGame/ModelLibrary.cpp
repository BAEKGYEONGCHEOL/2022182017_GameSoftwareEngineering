#include "stdafx.h"
#include "ModelLibrary.h"

#include <fstream>
#include <sstream>

bool ModelLibrary::Load()
{
	m_Models.clear();

	const char* paths[] = {
		"./Data/Models.txt",
		"./SimpleGame/Data/Models.txt",
		"../../SimpleGame/Data/Models.txt",
	};

	for (int index = 0; index < 3; ++index)
	{
		if (LoadFile(paths[index]))
			return true;
	}

	return false;
}

bool ModelLibrary::IsLoaded() const
{
	return !m_Models.empty();
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
