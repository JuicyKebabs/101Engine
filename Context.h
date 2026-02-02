#pragma once

// Forward declaration of classes used in Context
class Renderer;			// Renderer
class TextureManager;	// Texture manager
class MeshManager;		// Mesh manager
class CollisionManager;	// Collision manager

// Engine context structure
struct EngineContext
{
	Renderer* pRenderer = nullptr;				// Pointer to the renderer
	TextureManager* pTextureManager = nullptr;	// Pointer to the texture manager
	MeshManager* pMeshManager = nullptr;		// Pointer to the mesh manager
};

// Frame context structure
struct FrameContext
{
};

// Game context structure inheriting from EngineContext
struct GameContext : public EngineContext
{
	CollisionManager* pCollisionManager = nullptr;	// Pointer to the collision manager
};
