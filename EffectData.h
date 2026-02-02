#pragma once
#include <DirectXMath.h>
#include "RenderData.h"

//effect type enumeration
enum class EFFECT_TYPE
{
	NONE = 0,			// none
	PARTICLE_POINT,		// particle point
	EXPLOSION_SMALL,	// small explosion
	EXPLOSION_MEDIUM,	// medium explosion
	EXPLOSION_LARGE,	// large explosion
	SPARK,				// spark
	MAX					// maximum
};

//effect command structure
struct EffectCommand
{
	EFFECT_TYPE type = EFFECT_TYPE::NONE;		//effect type
	DirectX::XMFLOAT3 position{};				//position
	DirectX::XMFLOAT2 size{};					//size
	bool isChase = false;						//chase flag
	DirectX::XMFLOAT3* chaseTarget = nullptr;	//chase target position
};

//effect template structure
struct EffectTemplate
{
	//rendering related
	EFFECT_TYPE type = EFFECT_TYPE::NONE;	//effect type
	MESH_TYPE meshType{};					//mesh data
	const wchar_t* texPath = L"";			//texture path
	TexSplitInfo texSplitInfo{};			//texture split info
	BLEND_MODE blendMode =
		BLEND_MODE::BLEND_TRANSPARENT;		//blend mode

	//object related
	DirectX::XMFLOAT2 baseSize{};			//base size
	DirectX::XMFLOAT4 baseColor{};			//base color RGBA
	float lifeTime = 0.0f;					//life time
};

//scene-specific effect template list
extern const EffectTemplate g_effectTemplateListGame[];

//effect template set structure
struct EffectTemplateSet
{
	EffectTemplate* pTemplates = nullptr;	//effect template list pointer
	int templateCount = 0;					//effect template count
};

//effect template set retrieval function
EffectTemplateSet GetEffectTemplate();