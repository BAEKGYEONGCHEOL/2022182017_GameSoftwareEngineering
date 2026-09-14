#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "PrototypeRenderer.h"

enum ModelPrimitiveType
{
	ModelRect,
	ModelDiamond,
	ModelEllipse
};

struct ModelPrimitive
{
	ModelPrimitiveType type;
	float offsetX;
	float offsetY;
	float width;
	float height;
	float red;
	float green;
	float blue;
	float alpha;
};

class ModelLibrary
{
  public:
	bool Load();
	bool IsLoaded() const;
	void Draw(PrototypeRenderer* renderer,
		const std::string& modelName,
		float x,
		float y,
		float scale = 1.0f) const;

  private:
	bool LoadFile(const char* path);
	std::unordered_map<std::string, std::vector<ModelPrimitive>> m_Models;
};
