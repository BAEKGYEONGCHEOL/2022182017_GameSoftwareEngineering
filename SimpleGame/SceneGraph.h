#pragma once

#include "Actor.h"

class SceneGraph
{
  public:
	SceneGraph();

	void Clear();
	Actor* CreateActor(const std::string& name, Actor* parent = nullptr);
	void Render();
	Actor* Root();

  private:
	std::unique_ptr<Actor> m_Root;
};
