#pragma once
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Collider.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/UI/Canvas.h"
template<>
struct EnumReflection<AnchorMode>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto metadata = EnumMetadataBuilder<AnchorMode>()
			.Add("TopLeft", AnchorMode::TopLeft).Add("TopCenter", AnchorMode::TopCenter)
			.Add("TopRight", AnchorMode::TopRight).Add("MiddleLeft", AnchorMode::MiddleLeft)
			.Add("MiddleCenter", AnchorMode::MiddleCenter).Add("MiddleRight", AnchorMode::MiddleRight)
			.Add("BottomLeft", AnchorMode::BottomLeft).Add("BottomCenter", AnchorMode::BottomCenter)
			.Add("BottomRight", AnchorMode::BottomRight).Build();
		return metadata;
	}
};
template<> struct EnumReflection<CAMERA_FOLLOW_MODE>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto metadata = EnumMetadataBuilder<CAMERA_FOLLOW_MODE>()
		.Add("FOLLOW_MODE_FIXED", CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED)
		.Add("FOLLOW_MODE_OWNER", CAMERA_FOLLOW_MODE::FOLLOW_MODE_OWNER)
		.Add("FOLLOW_MODE_TARGET", CAMERA_FOLLOW_MODE::FOLLOW_MODE_TARGET).Build();
		return metadata;
	}
};
template<> struct EnumReflection<CAMERA_ROTATION_MODE>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto metadata = EnumMetadataBuilder<CAMERA_ROTATION_MODE>()
		.Add("ROTATION_MODE_FIXED", CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED)
		.Add("ROTATION_MODE_MATCH_OWNER", CAMERA_ROTATION_MODE::ROTATION_MODE_MATCH_OWNER)
		.Add("ROTATION_MODE_LOOK_AT_TARGET", CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET).Build();
		return metadata;
	}
};
template<> struct EnumReflection<PROJECTION_TYPE>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto metadata = EnumMetadataBuilder<PROJECTION_TYPE>()
		.Add("PROJECTION_TYPE_PERSPECTIVE", PROJECTION_TYPE::PROJECTION_TYPE_PERSPECTIVE)
		.Add("PROJECTION_TYPE_ORTHOGRAPHIC", PROJECTION_TYPE::PROJECTION_TYPE_ORTHOGRAPHIC).Build();
		return metadata;
	}
};
template<> struct EnumReflection<ColliderType>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto metadata = EnumMetadataBuilder<ColliderType>()
		.Add("BOX", ColliderType::BOX).Add("SPHERE", ColliderType::SPHERE)
		.Add("CAPSULE", ColliderType::CAPSULE).Add("None", ColliderType::None).Build();
		return metadata;
	}
};
template<> struct EnumReflection<CollisionLayer>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto metadata = EnumMetadataBuilder<CollisionLayer>()
		.Add("Default", CollisionLayer::Default).Add("PLAYER", CollisionLayer::PLAYER)
		.Add("ENEMY", CollisionLayer::ENEMY).Add("WALL", CollisionLayer::WALL)
		.Add("PLAYER_BULLET", CollisionLayer::PLAYER_BULLET).Add("PLAYER_RAY", CollisionLayer::PLAYER_RAY)
		.Add("ENEMY_BULLET", CollisionLayer::ENEMY_BULLET).Build();
		return metadata;
	}
};
template<> struct EnumReflection<BillboardType>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto value = EnumMetadataBuilder<BillboardType>()
			.Add("None", BillboardType::None).Add("Spherical", BillboardType::Spherical)
			.Add("Cylindrical", BillboardType::Cylindrical).Build();
		return value;
	}
};
template<> struct EnumReflection<CanvasRenderMode>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto value = EnumMetadataBuilder<CanvasRenderMode>()
			.Add("ScreenSpace", CanvasRenderMode::ScreenSpace).Add("WorldSpace", CanvasRenderMode::WorldSpace).Build();
		return value;
	}
};
template<> struct EnumReflection<CanvasScaleMode>
{
	static const std::optional<EnumMetadata>& Get()
	{
		static const auto value = EnumMetadataBuilder<CanvasScaleMode>()
			.Add("ConstantPixelSize", CanvasScaleMode::ConstantPixelSize)
			.Add("ScaleWithScreenSize", CanvasScaleMode::ScaleWithScreenSize).Build();
		return value;
	}
};
