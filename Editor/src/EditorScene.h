#pragma once
#include "Engine/Scene/SceneBase.h"

// EditorScene
// Minimal SceneBase subclass used by EditorApp. The editor doesn't define
// any game-specific initial setup; scenes are populated either by
// EditorApp::NewScene() (creates a MainCamera-tagged DefaultCamera) or by
// SceneLoader::LoadScene() (reads actors from a .scene JSON file).
class EditorScene : public SceneBase
{
private:
    void InitializeOverride(EngineContext& context) override {}
};
