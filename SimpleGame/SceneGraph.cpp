#include "stdafx.h"
#include "SceneGraph.h"

#include <algorithm>

SceneGraph::SceneGraph()
{
	Clear();
}

void SceneGraph::Clear()
{
	m_Root.reset(new Actor("SceneRoot"));
}

Actor* SceneGraph::CreateActor(const std::string& name, Actor* parent)
{
	Actor* actualParent = parent != nullptr ? parent : m_Root.get();
	return actualParent->CreateChild(name);
}

void SceneGraph::Render()
{
	const ActorTransform identity = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
	m_Root->UpdateWorldTransform(identity);

	std::vector<Actor*> visibleActors;
	m_Root->CollectVisibleActors(visibleActors);
	std::stable_sort(visibleActors.begin(),
		visibleActors.end(),
		[](const Actor* left, const Actor* right)
		{
			if (left->Layer() != right->Layer())
				return left->Layer() < right->Layer();
			return left->SortOrder() < right->SortOrder();
		});

	for (size_t index = 0; index < visibleActors.size(); ++index)
		visibleActors[index]->Render();
}

Actor* SceneGraph::Root()
{
	return m_Root.get();
}
