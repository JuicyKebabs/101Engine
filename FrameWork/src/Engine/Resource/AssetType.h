#pragma once

enum class AssetType
{
	Mesh,
	Texture,
	Audio,
	ActorImprint,
	Scene,
	//Material,
	//Shader,
	//Font,
	//Animation,
	//Scene,
	//Script,
	Unknown
};

// Asset category tags used by AssetReference<T>.
// They represent catalog asset types, not loaded GPU resources.
struct MeshAsset {};
struct TextureAsset {};
struct AudioAsset {};
struct SceneAsset {};
