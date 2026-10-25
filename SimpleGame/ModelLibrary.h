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

struct MeshTriangle
{
	float positions[9];
	float red;
	float green;
	float blue;
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
	void DrawMesh(PrototypeRenderer* renderer,
		const std::string& meshName,
		float x,
		float y,
		float scale = 1.0f,
		float rotation = 0.0f,
		float screenRotation = 0.0f,
		float heightScale = 1.0f,
		float localRotation = 0.0f,
		float localPivotX = 0.0f,
		float localPivotY = 0.0f) const;

  private:
	bool LoadFile(const char* path);
	bool LoadMeshFile(const char* path);
	std::unordered_map<std::string, std::vector<ModelPrimitive>> m_Models;
	std::unordered_map<std::string, std::vector<MeshTriangle>> m_Meshes;
};
