#pragma once
#include "Camera.h"
#include "ComPtr.h"
#include "SharedStruct.h"
#include "RenderData.h"
#include "EffectManager.h"
#include "EventType.h"
#include "Actor.h"
#include "Canvas.h"
#include "CollisionManager.h"
#include "Context.h"

// Scene base class
// Base class for all scene classes
class SceneBase
{
public:
	static constexpr DirectX::XMFLOAT3 SKY_BOX_SIZE = { 50.0f, 50.0f, 50.0f }; // Skybox size
public:
	SceneBase(float window_width, float window_height);	// Constructor
	~SceneBase();										// Destructor

	// Main processing functions
	void Initialize(EngineContext& context);
	void Update();										// Update
	void Draw(EngineContext& context);					// Draw
	void Finalize();									// Finalize
	virtual void ResolveCollisions() = 0;				// Resolve collisions

	// Getter
	CameraInfo* GetCameraInfo() const;			// Get camera information

protected:
	// Subsystems
	Camera* m_pCamera = nullptr;						// Camera
	CollisionManager* m_pCollisionManager = nullptr;	// Collision manager
	EffectManager* m_pEffectManager = nullptr;			// Effect manager
	DirectionalLight m_directionalLight;				// Directional light

	bool m_drawColliders = true;						// Collider draw flag

protected:
	virtual void InitializeOverride(EngineContext& context) = 0;	// Scene-specific initialization
	virtual void UpdateOverride() = 0;								// Scene-specific update
	virtual void FinalizeOverride() = 0;							// Scene-specific finalize

	void AddObject(std::unique_ptr<Actor> object); // Add object to scene
	void AddEvent() {};									// Add event subscription

private:
	std::vector<std::unique_ptr<Actor>> m_objectList;			// Object list in the scene
	std::vector<std::unique_ptr<Canvas>> m_canvasList;				// Canvas list in the scene

	std::vector<std::unique_ptr<Actor>> m_pendingObjects;		// Pending objects to be added

	std::vector<EventData> m_eventDataList;							// Subscribed event data list

	WorldRenderModel m_skyboxRenderInfo;	// Skybox render information

private:


};