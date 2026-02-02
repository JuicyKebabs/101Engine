#include "EffectData.h"

using namespace DirectX;

// Contents of TexSplitInfo from SharedStruct.h (Omitted in this file for clarity)
//struct TexSplitInfo
//{
//	int index = 0;			//Sprite index
//	int cols = 1;			//Number of columns
//	int rows = 1;			//Number of rows
//	int total = 1;			//Total number of frames (columns * rows)
//	int frameCount = 0;		//Current frame count
//	int updateRate = 0;		//Update rate (how many frames to advance)
//}

//effect template list
const EffectTemplate g_effectTemplateListGame[] =
{
	{//particle point
			EFFECT_TYPE::PARTICLE_POINT,				//type
			MESH_TYPE::SPHERE,					        //mesh type
			L"asset/texture/white.png",	            //texture path
			{ 1,1 },									//tex split info
			BLEND_MODE::BLEND_TRANSPARENT,				//blend mode
			{ 1.0f,1.0f },								//base size
			{ 1.0f,0.0f,0.0f,0.2f },					//base color RGBA
			20.0f										//life time
    },

	{// small explosion
		EFFECT_TYPE::EXPLOSION_SMALL,				//type
			MESH_TYPE::QUAD,						//mesh type
			L"asset/effect/explosion_small.png",	//texture path
			{ 0,8,1,8,0,4},						//tex split info
			BLEND_MODE::BLEND_TRANSPARENT,			//blend mode
			{ 5.0f,5.0f },							//base size
			{ 1.0f,1.0f,1.0f,1.0f },				//base color RGBA
			32.0f									//life time
	},
	{// medium explosion
		EFFECT_TYPE::EXPLOSION_MEDIUM,				//type
			MESH_TYPE::QUAD,						//mesh type
			L"asset/effect/explosion_medium.png",	//texture path
			{ 0,7,1,7,0,5},						//tex split info
			BLEND_MODE::BLEND_TRANSPARENT,			//blend mode
			{ 5.0f,5.0f },							//base size
			{ 1.0f,1.0f,1.0f,1.0f },				//base color RGBA
			35.0f									//life time
	},
	{// large explosion
		EFFECT_TYPE::EXPLOSION_LARGE,				//type
			MESH_TYPE::QUAD,						//mesh type
			L"asset/effect/explosion_large.png",	//texture path
			{ 0,12,1,12,0,4},						//tex split info
			BLEND_MODE::BLEND_TRANSPARENT,			//blend mode
			{ 5.0f,5.0f },							//base size
			{ 1.0f,1.0f,1.0f,1.0f },				//base color RGBA
			48.0f									//life time
	},

	{//spark
		EFFECT_TYPE::SPARK,						//type
			MESH_TYPE::QUAD,					//mesh type
			L"asset/effect/spark.png",			//texture path
			{ 0,5,1,5,0,3},						//tex split info
			BLEND_MODE::BLEND_TRANSPARENT,		//blend mode
			{ 3.0f,3.0f },						//base size
			{ 0.0f,2.0f,1.0f,1.0f },			//base color RGBA
			15.0f								//life time
	},
};

//effect template list getter
EffectTemplateSet GetEffectTemplate()
{
    EffectTemplateSet set;
    set.pTemplates = const_cast<EffectTemplate*>(g_effectTemplateListGame);
    set.templateCount = sizeof(g_effectTemplateListGame) / sizeof(EffectTemplate);
    return set;
}