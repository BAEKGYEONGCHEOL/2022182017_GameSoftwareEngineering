#pragma once

#include <string>
#include <vector>

#include "ModelLibrary.h"
#include "PrototypeRenderer.h"
#include "SceneGraph.h"

struct WorldPoint
{
	float x;
	float y;
};

enum InteriorObjectType
{
	CargoCrate,
	ControlConsole,
	MedicalPod,
	PowerRelay,
	CoolantPipe,
	EngineModule,
	HullWall
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
	InteriorObjectType type;
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
	WorldPoint previousPosition;
	WorldPoint velocity;
	float life;
	float distanceTravelled;
	float maximumDistance;
	int damage;
	bool charged;
};

struct ExperienceOrb
{
	WorldPoint position;
	float life;
};

struct StoryDocument
{
	WorldPoint position;
	int documentId;
	bool read;
};

struct LevelSurvivor
{
	WorldPoint position;
	int survivorId;
	bool talked;
};

struct MagazinePickup
{
	WorldPoint position;
	bool collected;
};

struct NeutralNpc
{
	WorldPoint position;
	bool dead;
	float animationOffset;
	int dialogueId;
	bool talked;
};

struct Asteroid
{
	WorldPoint position;
	WorldPoint velocity;
	float radius;
	float rotation;
	float rotationSpeed;
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
	enum GameMode
	{
		OnFoot,
		ShipControl,
		Transition,
		FirstLevel,
		Complete
	};
	enum QuestStage
	{
		InspectTerminal,
		FindCore,
		ReturnToCockpit,
		FlyToSignal,
		QuestComplete,
		CollectLevelWeapon,
		EarnExperience,
		AllocateStats,
		FirstLevelComplete
	};

	void UpdateOnFoot(float deltaSeconds);
	void UpdateShip(float deltaSeconds);
	void UpdateAsteroids(float deltaSeconds);
	void UpdateEnemies(float deltaSeconds);
	void UpdateProjectiles(float deltaSeconds);
	void UpdateFirstLevel(float deltaSeconds);
	void GenerateFirstLevel();
	bool IsFirstLevelConnected(const std::vector<int>& tiles) const;
	bool IsFirstLevelWalkable(float x, float y) const;
	void AwardExperience(int amount, const WorldPoint& position);
	void ApplyStatInput();
	void HandleInteraction();
	void HandleAttack();
	void SetMessage(const std::wstring& message, float seconds);
	bool CanMoveTo(float x, float y) const;
	bool IsNear(const WorldPoint& point, float distance) const;
	bool AllEnemiesDefeated() const;
	ScreenPoint WorldToScreen(float x, float y, float height = 0.0f) const;
	std::wstring ObjectiveText() const;
	std::wstring InteractionText() const;
	Actor* CreateSceneActor(const std::string& name,
		float x,
		float y,
		float height,
		int layer,
		float sortOrder,
		const Actor::RenderFunction& renderFunction,
		Actor* parent = nullptr);

	void RenderInterior();
	void RenderSpace();
	void RenderDestination();
	void RenderFirstLevel();
	void RenderInterface();
	void DrawWorldBlock(const Obstacle& obstacle);
	void DrawCharacter(const WorldPoint& point, float r, float g, float b, bool enemy);
	void DrawNeutralNpc(const NeutralNpc& npc);
	void DrawAsteroid(const Asteroid& asteroid);
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
	std::vector<NeutralNpc> m_NeutralNpcs;
	std::vector<Projectile> m_Projectiles;
	std::vector<ExperienceOrb> m_ExperienceOrbs;
	float m_WeaponEnergy;
	float m_ReloadTimer;
	bool m_LevelWeaponCollected;
	int m_AmmoInMagazine;
	int m_SpareMagazines;
	int m_ShotsFired;
	int m_PlayerLevel;
	int m_Experience;
	int m_ExperienceToNextLevel;
	int m_StatPoints;
	int m_Firepower;
	int m_Agility;
	int m_Vitality;
	float m_PlayerMaxHealth;
	float m_LevelUpFlash;
	std::vector<int> m_FirstLevelTiles;
	std::vector<WorldPoint> m_LevelCorpses;
	std::vector<StoryDocument> m_StoryDocuments;
	std::vector<LevelSurvivor> m_LevelSurvivors;
	std::vector<MagazinePickup> m_MagazinePickups;
	int m_FirstLevelSize;
	unsigned int m_LevelSeed;
	ModelLibrary m_ModelLibrary;
	SceneGraph m_SceneGraph;

	WorldPoint m_ShipPosition;
	WorldPoint m_PreviousShipPosition;
	WorldPoint m_ShipVelocity;
	float m_ShipAngle;
	float m_ShipHealth;
	float m_ShipImpactFlash;
	float m_TransitionTimer;
	std::vector<Asteroid> m_Asteroids;
	int m_CurrentShip;
	int m_TravelDestinationShip;
	std::wstring m_Message;
	float m_MessageTimer;
};
