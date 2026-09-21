#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct ActorTransform
{
	float x;
	float y;
	float height;
	float rotation;
	float scaleX;
	float scaleY;
};

class Actor
{
  public:
	typedef std::function<void(const ActorTransform&)> RenderFunction;

	explicit Actor(const std::string& name = "Actor");

	Actor* CreateChild(const std::string& name);
	void SetRenderFunction(const RenderFunction& renderFunction);
	void UpdateWorldTransform(const ActorTransform& parentTransform);
	void CollectVisibleActors(std::vector<Actor*>& actors);
	void Render() const;

	const std::string& Name() const;
	ActorTransform& LocalTransform();
	const ActorTransform& WorldTransform() const;
	void SetVisible(bool visible);
	bool IsVisible() const;
	void SetLayer(int layer);
	int Layer() const;
	void SetSortOrder(float sortOrder);
	float SortOrder() const;

  private:
	std::string m_Name;
	ActorTransform m_LocalTransform;
	ActorTransform m_WorldTransform;
	bool m_Visible;
	int m_Layer;
	float m_SortOrder;
	RenderFunction m_RenderFunction;
	std::vector<std::unique_ptr<Actor>> m_Children;
};
