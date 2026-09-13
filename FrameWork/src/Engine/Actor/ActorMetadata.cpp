#include "ActorMetadata.h"
#include "Actor.h"
#include "ActorTag.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"

const TypeMetadata& GetActorMetadata()
{
	static const TypeMetadata metadata = []
	{
		TypeMetadataBuilder<Actor> builder("Actor");
		builder.Property("name", &Actor::GetName, &Actor::SetName);
		builder.Property("is_active", &Actor::IsActive, &Actor::SetActive);
		builder.Property("tag",
			[](const Actor& actor) { return TagRegistry::Get().GetName(actor.GetTag()); },
			[](Actor& actor, const std::string& name)
			{
				actor.SetTag(name == "None" ? TAG_NONE : TagRegistry::Get().GetId(name));
			});
		return *builder.Build();
	}();
	return metadata;
}

