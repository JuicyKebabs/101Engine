#pragma once
#include "d3dx12.h"
#include <string>
#include "RenderData.h"
#include "ComPtr.h"

//アルファモード
enum class ALPHA_MODE
{
	STRAIGHT,		//ストレート
	PREMULTIPLIED,	//プレマルチプライド
};

class PipelineState
{
private:
	ComPtr<ID3D12PipelineState> m_pipelineState;	//パイプラインステートオブジェクト
	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_desc{};	//パイプラインステートの設定
	ComPtr<ID3D10Blob> m_vsBlob;					//頂点シェーダーブロブ
	ComPtr<ID3D10Blob> m_psBlob;					//ピクセルシェーダーブロブ
	ComPtr<ID3D10Blob> m_pErrorBlob;				//エラーメッセージブロブ
	bool m_isValid = false;							//パイプラインステート生成に成功したか

	ID3D12Device* m_pDevice = nullptr;				//デバイス

private:	//結果コード
	HRESULT result = S_OK;	//HRESULT(成功/失敗コード)

public:
	PipelineState(ID3D12Device* pDevice);	//コンストラクタ
	~PipelineState() {};					//デストラクタ

	void SetInputLayout(const D3D12_INPUT_LAYOUT_DESC& inputLayout);	//入力レイアウトを設定
	void SetRootSignature(ID3D12RootSignature* pRootSignature);			//ルートシグネチャを設定
	void SetVertexShader(ID3DBlob* blob);								//頂点シェーダーを設定
	void SetPixelShader(ID3DBlob* blob);								//ピクセルシェーダーを設定
	void Create();														//パイプラインステートオブジェクトを作成

	void SetBlendMode(BLEND_MODE mode);	//ブレンドモードを設定
	void SetDepthMode(DEPTH_MODE mode);	//深度モードを設定
	void SetCullMode(CULL_MODE mode);	//カリングモードを設定

	ID3D12PipelineState* GetPipelineState() const; //パイプラインステートオブジェクトを取得
	bool IsValid() const; //パイプラインステート生成に成功したかを取得
};