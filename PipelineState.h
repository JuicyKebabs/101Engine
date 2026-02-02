#pragma once
#include "d3dx12.h"
#include <string>
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
	void SetVertexShader(const std::wstring& filename);					//頂点シェーダーを設定
	void SetVertexShader(												//頂点シェーダーを設定（エントリーポイント指定版）
		const std::wstring& filename,	//シェーダーファイル名
		const std::string& entryPoint	//エントリーポイント名
	);
	void SetPixelShader(const std::wstring& filename);					//ピクセルシェーダーを設定
	void SetPixelShader(												//ピクセルシェーダーを設定（エントリーポイント指定版）
		const std::wstring& filename,	//シェーダーファイル名
		const std::string& entryPoint	//エントリーポイント名
	);
	void Create();														//パイプラインステートオブジェクトを作成

	void EnableAlphaBlend(					//アルファブレンドを有効化
		bool enable,
		ALPHA_MODE mode = ALPHA_MODE::STRAIGHT
	);
	void EnableDepthWrite(bool enable);		//深度ステンシルを有効化
	void EnableDepthTest(bool enable);		//深度テストを有効化
	void SetCullMode(D3D12_CULL_MODE mode);	//カリングモードを設定

	ID3D12PipelineState* GetPipelineState() const; //パイプラインステートオブジェクトを取得
	bool IsValid() const; //パイプラインステート生成に成功したかを取得
};