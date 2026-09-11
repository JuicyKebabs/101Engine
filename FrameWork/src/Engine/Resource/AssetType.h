#pragma once

enum class AssetType
{
	Mesh,
	Texture,
	//Material,
	//Shader,
	//Audio,
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
