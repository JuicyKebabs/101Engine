#include "Engine/EngineComponentrRegistration.h"
#include "Engine/Component/PersistentComponentMetadata.h"
#include "Engine/Scene/ComponentRegistry.h"

#define REGISTER_BUILTIN_COMPONENT(T) \
	static const bool registered##T = ComponentRegistry::Get().RegisterReflected<T>( \
		#T, PersistentComponentMetadata::T(#T));

REGISTER_BUILTIN_COMPONENT(Transform)
REGISTER_BUILTIN_COMPONENT(MeshRenderer)
REGISTER_BUILTIN_COMPONENT(SpriteRenderer)
REGISTER_BUILTIN_COMPONENT(UIRenderer)
REGISTER_BUILTIN_COMPONENT(UIImage)
REGISTER_BUILTIN_COMPONENT(Canvas)
REGISTER_BUILTIN_COMPONENT(Camera)
REGISTER_BUILTIN_COMPONENT(RectTransform)
REGISTER_BUILTIN_COMPONENT(Collider)

#undef REGISTER_BUILTIN_COMPONENT
