#include "Engine/EngineComponentrRegistration.h"
#include "Engine/Scene/ComponentRegistry.h"

static bool s_init = []() {
    DBG("EngineComponentRegistration.cpp loaded");
    return true;
    }();

// Built-in engine components registration mactros.
REGISTER_COMPONENT(MeshRenderer)
REGISTER_COMPONENT(SpriteRenderer)
REGISTER_COMPONENT(UIRenderer)
REGISTER_COMPONENT(UIImage)
REGISTER_COMPONENT(Canvas)
REGISTER_COMPONENT(Camera)
REGISTER_COMPONENT(RectTransform)
REGISTER_COMPONENT(Collider)