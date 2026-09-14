#pragma once

#include <string>
#include <vector>
#include "PrototypeRenderer.h"

struct WorldPoint
{
	float x;
	float y;
};

struct Obstacle
{
	float x;
	float y;
	float halfWidth;
	float halfHeight;
	float visualHeight;
	float r;
	float g;
	float b;
};

struct Enemy
{
	WorldPoint position;
	int health;
	bool active;
};

struct Projectile
{
	WorldPoint position;
	WorldPoint velocity;
	float life;
};

class Game
{
public:
	explicit Game(PrototypeRenderer* renderer);
	void Reset();
	void Update(float deltaSeconds);
	void Render();
	void Resize(int width, int height);

	void KeyDown(unsigned char key);
	void KeyUp(unsigned char key);
	void SpecialDown(int key);
	void SpecialUp(int key);
	void MouseMove(int x, int y);
	void MouseButton(int button, int state);

private:
	enum GameMode { OnFoot, ShipControl, Transition, Complete };
	enum QuestStage { InspectTerminal, FindCore, ReturnToCockpit, FlyToSignal, QuestComplete };

	void UpdateOnFoot(float deltaSeconds);
	void UpdateShip(float deltaSeconds);
	void UpdateEnemies(float deltaSeconds);
	void UpdateProjectiles(float deltaSeconds);
	void HandleInteraction();
	void HandleAttack();
	void SetMessage(const std::wstring& message, float seconds);
	bool CanMoveTo(float x, float y) const;
	bool IsNear(const WorldPoint& point, float distance) const;
	bool AllEnemiesDefeated() const;
	ScreenPoint WorldToScreen(float x, float y, float height = 0.0f) const;
	std::wstring ObjectiveText() const;
	std::wstring InteractionText() const;

	void RenderInterior();
	void RenderSpace();
	void RenderDestination();
	void RenderInterface();
	void DrawWorldBlock(const Obstacle& obstacle);
	void DrawCharacter(const WorldPoint& point, float r, float g, float b, bool enemy);
	void DrawShip(float x, float y, float angle, float scale, float r, float g, float b);

	PrototypeRenderer* m_Renderer;
	GameMode m_Mode;
	QuestStage m_Quest;
	bool m_Paused;
	bool m_CoreRecovered;
	bool m_RecordRead;
	bool m_SignalChecked;
	bool m_MedicalDroneUsed;
	bool m_Keys[256];
	bool m_KeyPressed[256];
	bool m_SpecialKeys[512];
	bool m_MousePressed;
	int m_MouseX;
	int m_MouseY;

	WorldPoint m_Player;
	WorldPoint m_LastMoveDirection;
	float m_PlayerHealth;
	float m_AttackCooldown;
	float m_AttackFlash;
	float m_DodgeCooldown;
	float m_DodgeTimer;
	float m_DamageCooldown;
	float m_TotalTime;
	bool m_PlayerMoving;
	std::vector<Obstacle> m_Obstacles;
	std::vector<Enemy> m_Enemies;
	std::vector<Projectile> m_Projectiles;
	float m_WeaponEnergy;
	float m_ReloadTimer;

	WorldPoint m_ShipPosition;
	WorldPoint m_ShipVelocity;
	float m_ShipAngle;
	float m_ShipHealth;
	float m_TransitionTimer;
	int m_CurrentShip;
	int m_TravelDestinationShip;
	std::wstring m_Message;
	float m_MessageTimer;
};
