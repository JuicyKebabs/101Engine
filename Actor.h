#pragma once
#include <DirectXMath.h>
#include <vector>
#include "ComponentsContainer.h"
#include "SharedStruct.h"

// Forward declaration
class Renderer;
class TextureManager;
class MeshManager;

// Actor Class
// This defines the all elements that exist in the game world
class Actor
{
public:
	Actor(	// Constructor
		DirectX::XMFLOAT3 position = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f },	// Position
		DirectX::XMFLOAT3 rotation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f },	// Rotation
		DirectX::XMFLOAT3 scale = DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f },	// Scale
		bool isActive = true,												// Active flag
		OBJECT_TAG tag = OBJECT_TAG::NONE									// Object tag
	);
	~Actor();	// Destructor

	void Update(float deltaTime);	// Update
	void LateUpdate();				// Late update

	void SetActive(bool isActive);	// Set active flag
	void SetTag(OBJECT_TAG tag);	// Set object tag
	const bool IsActive() const;	// Get active status
	OBJECT_TAG GetTag() const;		// Get object tag

	// Add a component of type T to the container
	template<class T, class... Args>
	T* AddComponent(Args&&... args)
	{
		return m_pComponentsContainer->AddComponent<T>(std::forward<Args>(args)...);
	}

	// Get a component of type T from the container
	template<class T, class... Args>
	T* GetComponent(Args&&... args)
	{
		return m_pComponentsContainer->GetComponent<T>(std::forward<Args>(args)...);
	}

private:	// Non-public member variables
	bool m_isActive = false;	// Active flag

	OBJECT_TAG m_tag = OBJECT_TAG::NONE; // Object tag

	std::unique_ptr<ComponentsContainer> m_pComponentsContainer = nullptr;	// Components container

	int m_passedFrames = 0; // Elapsed frame count
};