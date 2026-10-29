#pragma once

namespace rendering { class Renderer; }
class RenderAssetResources;
class AssetManager;
class AudioManager;
class ActorImprintSystem;
class CollisionSystem;

// Borrowed App services. Scenes/Components end before these owners are reset.
// No native Device, CommandList, descriptor allocator or old GPU manager escapes.
struct EngineContext
{
    rendering::Renderer* pRenderer = nullptr;
    RenderAssetResources* pRenderAssets = nullptr;
    AssetManager* pAssetManager = nullptr;
    ActorImprintSystem* pActorImprintSystem = nullptr;
    AudioManager* pAudioManager = nullptr;
};

struct FrameContext
{
};

struct GameContext : public EngineContext
{
    CollisionSystem* pCollisionManager = nullptr;
};
