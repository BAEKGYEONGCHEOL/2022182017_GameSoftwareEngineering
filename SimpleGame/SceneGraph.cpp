#include "stdafx.h"
#include "SceneGraph.h"

#include <algorithm>
#include <chrono>

SceneGraph::SceneGraph()
	: m_Root("SceneRoot")
	, m_UsedActorCount(0)
	, m_VisibleActorCount(0)
	, m_CulledActorCount(0)
	, m_TransformTimeMs(0.0)
	, m_SortTimeMs(0.0)
	, m_ActorRenderTimeMs(0.0)
{
}

void SceneGraph::Clear()
{
	m_Root.Reset("SceneRoot");
	m_UsedActorCount = 0;
	m_VisibleActorCount = 0;
	m_CulledActorCount = 0;
	m_TransformTimeMs = 0.0;
	m_SortTimeMs = 0.0;
	m_ActorRenderTimeMs = 0.0;
}

Actor* SceneGraph::CreateActor(const std::string& name, Actor* parent)
{
	Actor* actor = nullptr;
	if (m_UsedActorCount < m_ActorPool.size())
	{
		actor = m_ActorPool[m_UsedActorCount].get();
		actor->Reset(name);
	}
	else
	{
		std::unique_ptr<Actor> newActor(new Actor(name));
		actor = newActor.get();
		m_ActorPool.push_back(std::move(newActor));
	}
	++m_UsedActorCount;

	Actor* actualParent = parent != nullptr ? parent : &m_Root;
	actualParent->AddChild(actor);
	return actor;
}

void SceneGraph::Render()
{
	typedef std::chrono::steady_clock Clock;
	const Clock::time_point transformStart = Clock::now();
	const ActorTransform identity = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
	m_Root.UpdateWorldTransform(identity);

	std::vector<Actor*> visibleActors;
	m_Root.CollectVisibleActors(visibleActors);
	m_VisibleActorCount = static_cast<unsigned int>(visibleActors.size());
	m_CulledActorCount = static_cast<unsigned int>(m_UsedActorCount) - m_VisibleActorCount;
	const Clock::time_point transformEnd = Clock::now();
	std::stable_sort(visibleActors.begin(),
		visibleActors.end(),
		[](const Actor* left, const Actor* right)
		{
			if (left->Layer() != right->Layer())
				return left->Layer() < right->Layer();
			return left->SortOrder() < right->SortOrder();
		});
	const Clock::time_point sortEnd = Clock::now();

	for (size_t index = 0; index < visibleActors.size(); ++index)
		visibleActors[index]->Render();
	const Clock::time_point renderEnd = Clock::now();
	m_TransformTimeMs =
		std::chrono::duration<double, std::milli>(transformEnd - transformStart).count();
	m_SortTimeMs = std::chrono::duration<double, std::milli>(sortEnd - transformEnd).count();
	m_ActorRenderTimeMs = std::chrono::duration<double, std::milli>(renderEnd - sortEnd).count();
}

Actor* SceneGraph::Root()
{
	return &m_Root;
}

unsigned int SceneGraph::VisibleActorCount() const
{
	return m_VisibleActorCount;
}

unsigned int SceneGraph::CulledActorCount() const
{
	return m_CulledActorCount;
}

unsigned int SceneGraph::ActorPoolSize() const
{
	return static_cast<unsigned int>(m_ActorPool.size());
}

double SceneGraph::TransformTimeMs() const
{
	return m_TransformTimeMs;
}

double SceneGraph::SortTimeMs() const
{
	return m_SortTimeMs;
}

double SceneGraph::ActorRenderTimeMs() const
{
	return m_ActorRenderTimeMs;
}
