#include "Engine/EngineComponentrRegistration.h"

static bool s_init = []() {
    DBG("EngineComponentRegistration.cpp loaded");
    return true;
    }();

// Built-in engine components registration mactros.

REGISTER_ENGINE_COMPONENT(MeshRenderer)
REGISTER_ENGINE_COMPONENT(SpriteRenderer)
REGISTER_ENGINE_COMPONENT(UIRenderer)
REGISTER_ENGINE_COMPONENT(UIImage)
REGISTER_ENGINE_COMPONENT(Canvas)
REGISTER_ENGINE_COMPONENT(Camera)
REGISTER_ENGINE_COMPONENT(RectTransform)
REGISTER_ENGINE_COMPONENT(Collider)