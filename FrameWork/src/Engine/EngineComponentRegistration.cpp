#include "Engine/EngineComponentrRegistration.h"
#include "Engine/Component/PersistentComponentMetadata.h"
#include "Engine/Scene/ComponentRegistry.h"

static bool s_init = []() {
    DBG("EngineComponentRegistration.cpp loaded");
    return true;
    }();

// Built-in engine components registration macros.
REGISTER_COMPONENT(Transform)
REGISTER_COMPONENT(MeshRenderer)
REGISTER_COMPONENT(SpriteRenderer)
REGISTER_COMPONENT(UIRenderer)
REGISTER_COMPONENT(UIImage)
REGISTER_COMPONENT(Canvas)
REGISTER_COMPONENT(Camera)
REGISTER_COMPONENT(RectTransform)
REGISTER_COMPONENT(Collider)

static bool s_registerPersistentMetadata = []()
{
	auto& registry = ComponentRegistry::Get();
	return registry.RegisterMetadata("Transform", PersistentComponentMetadata::Transform()) &&
		registry.RegisterMetadata("MeshRenderer", PersistentComponentMetadata::MeshRenderer()) &&
		registry.RegisterMetadata("SpriteRenderer", PersistentComponentMetadata::SpriteRenderer()) &&
		registry.RegisterMetadata("UIRenderer", PersistentComponentMetadata::UIRenderer()) &&
		registry.RegisterMetadata("UIImage", PersistentComponentMetadata::UIImage()) &&
		registry.RegisterMetadata("Canvas", PersistentComponentMetadata::Canvas()) &&
		registry.RegisterMetadata("Camera", PersistentComponentMetadata::Camera()) &&
		registry.RegisterMetadata("RectTransform", PersistentComponentMetadata::RectTransform()) &&
		registry.RegisterMetadata("Collider", PersistentComponentMetadata::Collider());
}();
