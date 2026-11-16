#pragma once

#include <memory>
#include <vector>

#include "Actor.h"

class SceneGraph
{
  public:
	SceneGraph();

	void Clear();
	Actor* CreateActor(const std::string& name, Actor* parent = nullptr);
	void Render();
	Actor* Root();
	unsigned int VisibleActorCount() const;
	unsigned int CulledActorCount() const;
	unsigned int ActorPoolSize() const;
	double TransformTimeMs() const;
	double SortTimeMs() const;
	double ActorRenderTimeMs() const;

  private:
	Actor m_Root;
	std::vector<std::unique_ptr<Actor>> m_ActorPool;
	size_t m_UsedActorCount;
	unsigned int m_VisibleActorCount;
	unsigned int m_CulledActorCount;
	double m_TransformTimeMs;
	double m_SortTimeMs;
	double m_ActorRenderTimeMs;
};
