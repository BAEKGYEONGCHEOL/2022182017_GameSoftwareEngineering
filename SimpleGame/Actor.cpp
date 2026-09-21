#include "stdafx.h"
#include "Actor.h"

#include <cmath>
#include <utility>

namespace
{
ActorTransform IdentityTransform()
{
	ActorTransform transform = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
	return transform;
}
} // namespace

Actor::Actor(const std::string& name)
	: m_Name(name)
	, m_LocalTransform(IdentityTransform())
	, m_WorldTransform(IdentityTransform())
	, m_Visible(true)
	, m_Layer(0)
	, m_SortOrder(0.0f)
{
}

Actor* Actor::CreateChild(const std::string& name)
{
	std::unique_ptr<Actor> child(new Actor(name));
	Actor* childPointer = child.get();
	m_Children.push_back(std::move(child));
	return childPointer;
}

void Actor::SetRenderFunction(const RenderFunction& renderFunction)
{
	m_RenderFunction = renderFunction;
}

void Actor::UpdateWorldTransform(const ActorTransform& parentTransform)
{
	const float cosine = std::cos(parentTransform.rotation);
	const float sine = std::sin(parentTransform.rotation);
	const float scaledX = m_LocalTransform.x * parentTransform.scaleX;
	const float scaledY = m_LocalTransform.y * parentTransform.scaleY;

	m_WorldTransform.x = parentTransform.x + scaledX * cosine - scaledY * sine;
	m_WorldTransform.y = parentTransform.y + scaledX * sine + scaledY * cosine;
	m_WorldTransform.height = parentTransform.height + m_LocalTransform.height;
	m_WorldTransform.rotation = parentTransform.rotation + m_LocalTransform.rotation;
	m_WorldTransform.scaleX = parentTransform.scaleX * m_LocalTransform.scaleX;
	m_WorldTransform.scaleY = parentTransform.scaleY * m_LocalTransform.scaleY;

	for (size_t index = 0; index < m_Children.size(); ++index)
		m_Children[index]->UpdateWorldTransform(m_WorldTransform);
}

void Actor::CollectVisibleActors(std::vector<Actor*>& actors)
{
	if (!m_Visible)
		return;

	if (m_RenderFunction)
		actors.push_back(this);

	for (size_t index = 0; index < m_Children.size(); ++index)
		m_Children[index]->CollectVisibleActors(actors);
}

void Actor::Render() const
{
	if (m_Visible && m_RenderFunction)
		m_RenderFunction(m_WorldTransform);
}

const std::string& Actor::Name() const
{
	return m_Name;
}

ActorTransform& Actor::LocalTransform()
{
	return m_LocalTransform;
}

const ActorTransform& Actor::WorldTransform() const
{
	return m_WorldTransform;
}

void Actor::SetVisible(bool visible)
{
	m_Visible = visible;
}

bool Actor::IsVisible() const
{
	return m_Visible;
}

void Actor::SetLayer(int layer)
{
	m_Layer = layer;
}

int Actor::Layer() const
{
	return m_Layer;
}

void Actor::SetSortOrder(float sortOrder)
{
	m_SortOrder = sortOrder;
}

float Actor::SortOrder() const
{
	return m_SortOrder;
}
