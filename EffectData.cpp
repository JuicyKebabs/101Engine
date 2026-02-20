#include "EffectData.h"

using namespace DirectX;


//エフェクトテンプレートリスト
const EffectTemplate g_effectTemplateListGame[] =
{
//パーティクルポイント
    {
         EFFECT_TYPE::PARTICLE_POINT,				//エフェクトタイプ
         MESH_TYPE::SPHERE,					        //メッシュデータ
         L"asset/texture/white.png",	            //テクスチャパス
         { 1,1 },									//テクスチャ分割情報
         PSO_KEY_MASKED,                                  //PSOキー
         { 1.0f,1.0f, 1.0f },						//基本サイズ
         { 1.0f,0.0f,0.0f,0.2f },					//基本色RGBA
         20.0f										//寿命
    },
    {
         EFFECT_TYPE::FIRE_FLASH,				    //エフェクトタイプ
         MESH_TYPE::QUAD,					        //メッシュデータ
         L"asset/effect/FireFlash.png",	    //テクスチャパス
         { 0, 5, 2, 10, 0, 2 },						//テクスチャ分割情報
         PSO_KEY_MASKED,                                  //PSOキー
         { 1.0f,1.0f, 1.0f },						//基本サイズ
         { 1.0f,1.0f,1.0f,1.0f },					//基本色RGBA
         20.0f,										//寿命
    },
    {
         EFFECT_TYPE::EXPLOSION,				    //エフェクトタイプ
         MESH_TYPE::QUAD,					        //メッシュデータ
         L"asset/effect/explosion_EF.png",	//テクスチャパス
         { 0, 9, 1, 9, 0, 2 },						//テクスチャ分割情報
         PSO_KEY_MASKED,                                  //PSOキー
         { 1.0f,1.0f, 1.0f },						//基本サイズ
         { 1.0f,1.0f,1.0f,1.0f },					//基本色RGBA
         18.0f,										//寿命
    },
    {
         EFFECT_TYPE::WIND,				            //エフェクトタイプ
         MESH_TYPE::QUAD,					        //メッシュデータ
         L"asset/effect/wind_EF.png",	    //テクスチャパス
         { 0, 6, 5, 30, 0, 2 },						//テクスチャ分割情報
         PSO_KEY_ADDITIVE,                          //PSOキー
         { 1.0f,1.0f, 1.0f },						//基本サイズ
         { 1.0f,1.0f,1.0f,1.0f },					//基本色RGBA
         -1.0f,										//寿命
    }
};

//エフェクトテンプレートリスト
EffectTemplateSet GetEffectTemplate()
{
    EffectTemplateSet set;
    set.pTemplates = const_cast<EffectTemplate*>(g_effectTemplateListGame);
    set.templateCount = sizeof(g_effectTemplateListGame) / sizeof(EffectTemplate);
    return set;
}
