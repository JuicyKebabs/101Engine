#pragma once
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/EngineComponentRegistry.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/UI/UIRenderer.h"
#include "Engine/UI/UIImage.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Collider.h"

//------------------------------------------------------------------------------------------------------------------------------
// Built-in component registration definitions
// This file contains the table which contains pairs of component names and function pointers to add the component to an actor. 
// This is used for adding components to actors at startup based on string names (e.g. from JSON data).
//------------------------------------------------------------------------------------------------------------------------------

inline void RegisterEngineComponents()
{
    auto& reg = EngineComponentRegistry::Get();
    reg.Register("MeshRenderer", [](Actor* a) { a->AddComponent<MeshRenderer>(); });
    reg.Register("SpriteRenderer", [](Actor* a) { a->AddComponent<SpriteRenderer>(); });
	reg.Register("UIRenderer", [](Actor* a) { a->AddComponent<UIRenderer>(); });
	reg.Register("UIImage", [](Actor* a) { a->AddComponent<UIImage>(); });
	reg.Register("Canvas", [](Actor* a) { a->AddComponent<Canvas>(); });
    reg.Register("Camera", [](Actor* a) { a->AddComponent<Camera>(); });
    reg.Register("RectTransform", [](Actor* a) { a->AddComponent<RectTransform>(); });
    reg.Register("Collider", [](Actor* a) { a->AddComponent<Collider>(); });
}