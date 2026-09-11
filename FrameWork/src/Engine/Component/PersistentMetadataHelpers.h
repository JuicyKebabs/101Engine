#pragma once
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Math/ValueValidation.h"
#include <memory>

namespace PersistentMetadata
{
	template<class T> void AddComponentName(TypeMetadataBuilder<T>& builder)
	{
		builder.Property("name", &T::GetName, &T::SetName);
	}
	template<class T> std::unique_ptr<TypeMetadata> Finish(TypeMetadataBuilder<T>& builder)
	{
		auto metadata = builder.Build();
		return metadata ? std::make_unique<TypeMetadata>(std::move(*metadata)) : nullptr;
	}
}
