#include "PipelineState.h"
#include "d3dx12.h"
#include <d3dcompiler.h>
#include "Engine.h"

//コンストラクタ
PipelineState::PipelineState(ID3D12Device* pDevice)
{
	m_pDevice = pDevice;	//デバイスの保存

	//パイプラインステートの設定
	m_desc = {}; //パイプラインステートの設定を初期化
	m_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);		//ラスタライザーステートの設定
	m_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;					//カリングしない
	m_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);	//デプスステンシルステートの設定
	m_desc.SampleMask = UINT_MAX;											//サンプルマスクの設定
	m_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	//プリミティブトポロジーの設定(三角形)
	m_desc.NumRenderTargets = 1;											//レンダーターゲットの数
	m_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;						//レンダーターゲットのフォーマット設定(sRGB)
	m_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;								//デプスステンシルビューのフォーマット設定
	m_desc.SampleDesc.Count = 1;											//マルチサンプリングしない
	m_desc.SampleDesc.Quality = 0;											//クオリティレベル0

	//ブレンドステートの設定
	m_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);					//ブレンドステートの設定
	m_desc.BlendState.AlphaToCoverageEnable = FALSE;						//アルファトゥカバレッジ無効
	m_desc.BlendState.IndependentBlendEnable = FALSE;						//独立ブレンド無効
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{//デフォルトのレンダーターゲットブレンドステート設定
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	//デフォルトのレンダーターゲットブレンドステート設定
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		m_desc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;
	}

	auto& rt0 = m_desc.BlendState.RenderTarget[0];				//レンダーターゲット0のブレンドステート設定
	rt0.BlendEnable = TRUE;										//ブレンドを有効にする
	rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;	//RGBA全てのチャンネルを書き込む
	rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;						//ソースのブレンド係数
	rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;					//デスティネーションのブレンド係数
	rt0.BlendOp = D3D12_BLEND_OP_ADD;							//ブレンド演算
	rt0.SrcBlendAlpha = D3D12_BLEND_ONE;						//アルファ値のソースのブレンド係数
	rt0.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;				//アルファ値のデスティネーションのブレンド係数
	rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;						//アルファ値のブレンド演算
}

//入力レイアウトを設定
void PipelineState::SetInputLayout(const D3D12_INPUT_LAYOUT_DESC& inputLayout)
{
	m_desc.InputLayout = inputLayout;	//入力レイアウトを設定
}

//ルートシグネチャを設定
void PipelineState::SetRootSignature(ID3D12RootSignature* pRootSignature)
{
	m_desc.pRootSignature = pRootSignature;	//ルートシグネチャを設定
}

//頂点シェーダーを設定
void PipelineState::SetVertexShader(const std::wstring& filename)
{
	//頂点シェーダーのコンパイル
	result = D3DCompileFromFile(
		filename.c_str(),									//シェーダーコードのファイル名
		nullptr,											//マクロ定義
		D3D_COMPILE_STANDARD_FILE_INCLUDE,					//インクルード可能にする
		"BasicVS",											//エントリーポイント関数名
		"vs_5_0",											//シェーダーモデル指定(今回はバージョン5.0の頂点シェーダー)
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,	//デバッグ用設定
		0,													//追加オプション
		&m_vsBlob,											//コンパイル後のバイナリ格納先
		&m_pErrorBlob										//エラーメッセージ格納先
	);

	if (FAILED(result))
	{
		return;
	}

	//頂点シェーダーの設定
	m_desc.VS = CD3DX12_SHADER_BYTECODE(m_vsBlob.Get());
}

//頂点シェーダーを設定（エントリーポイント指定版）
void PipelineState::SetVertexShader(const std::wstring& filename, const std::string& entryPoint)
{
	//頂点シェーダーのコンパイル
	result = D3DCompileFromFile(
		filename.c_str(),									//シェーダーコードのファイル名
		nullptr,											//マクロ定義
		D3D_COMPILE_STANDARD_FILE_INCLUDE,					//インクルード可能にする
		entryPoint.c_str(),									//エントリーポイント関数名
		"vs_5_0",											//シェーダーモデル指定(今回はバージョン5.0の頂点シェーダー)
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,	//デバッグ用設定
		0,													//追加オプション
		&m_vsBlob,											//コンパイル後のバイナリ格納先
		&m_pErrorBlob										//エラーメッセージ格納先
	);

	if (FAILED(result))
	{
		return;
	}

	//頂点シェーダーの設定
	m_desc.VS = CD3DX12_SHADER_BYTECODE(m_vsBlob.Get());
}

//ピクセルシェーダーを設定
void PipelineState::SetPixelShader(const std::wstring& filename)
{
	result = D3DCompileFromFile(
		filename.c_str(),									//シェーダーコードのファイル名
		nullptr,											//マクロ定義
		D3D_COMPILE_STANDARD_FILE_INCLUDE,					//インクルード可能にする
		"BasicWorldPS",											//エントリーポイント関数名
		"ps_5_0",											//シェーダーモデル指定(今回はバージョン5.0のピクセルシェーダー)
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,	//デバッグ用設定
		0,													//追加オプション
		&m_psBlob,											//コンパイル後のバイナリ格納先
		&m_pErrorBlob										//エラーメッセージ格納先
	);

	//ピクセルシェーダーの設定
	m_desc.PS = CD3DX12_SHADER_BYTECODE(m_psBlob.Get());
}

//ピクセルシェーダーを設定（エントリーポイント指定版）
void PipelineState::SetPixelShader(const std::wstring& filename, const std::string& entryPoint)
{
	result = D3DCompileFromFile(
		filename.c_str(),									//シェーダーコードのファイル名
		nullptr,											//マクロ定義
		D3D_COMPILE_STANDARD_FILE_INCLUDE,					//インクルード可能にする
		entryPoint.c_str(),									//エントリーポイント関数名
		"ps_5_0",											//シェーダーモデル指定(今回はバージョン5.0のピクセルシェーダー)
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,	//デバッグ用設定
		0,													//追加オプション
		&m_psBlob,											//コンパイル後のバイナリ格納先
		&m_pErrorBlob										//エラーメッセージ格納先
	);

	//ピクセルシェーダーの設定
	m_desc.PS = CD3DX12_SHADER_BYTECODE(m_psBlob.Get());
}

//パイプラインステートの作成
void PipelineState::Create()
{
	//パイプラインステートの作成
	result = m_pDevice->CreateGraphicsPipelineState(
		&m_desc,												//パイプラインステートの設定
		IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())	//作成したパイプラインステートオブジェクトのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	if (FAILED(result))
	{
		m_isValid = false; //パイプラインステート生成に失敗
		return;	
	}

	m_isValid = true; //パイプラインステート生成に成功

}

//アルファブレンドを有効化
void PipelineState::EnableAlphaBlend(
	bool enable,	//有効化フラグ
	ALPHA_MODE mode	//アルファモード
	)
{
	auto& rt0 = m_desc.BlendState.RenderTarget[0];				//レンダーターゲット0のブレンドステート設定

	if(enable)
	{
		switch (mode)
		{
		case ALPHA_MODE::STRAIGHT:
			rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			break;
		case ALPHA_MODE::PREMULTIPLIED:
			rt0.SrcBlend = D3D12_BLEND_ONE;
			break;
		default:
			break;
		}

		rt0.BlendEnable = TRUE;
		rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt0.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	}
	else
	{
		rt0.BlendEnable = FALSE;
		rt0.SrcBlend = D3D12_BLEND_ONE;
		rt0.DestBlend = D3D12_BLEND_ZERO;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt0.DestBlendAlpha = D3D12_BLEND_ZERO;
		rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	}
}

//深度ステンシルを有効化
void PipelineState::EnableDepthWrite(bool enable)
{
	m_desc.DepthStencilState.DepthWriteMask = enable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO; //深度書き込みマスクの設定
}

//深度テストを有効化
void PipelineState::EnableDepthTest(bool enable)
{
	m_desc.DepthStencilState.DepthEnable = enable; //深度テストの有効化設定
}

//カリングモードを設定
void PipelineState::SetCullMode(D3D12_CULL_MODE mode)
{
	m_desc.RasterizerState.CullMode = mode; //カリングモードを設定
}

//パイプラインステートオブジェクトを取得
ID3D12PipelineState* PipelineState::GetPipelineState() const
{
	return m_pipelineState.Get(); //パイプラインステートオブジェクトを取得
}

bool PipelineState::IsValid() const
{
	return m_isValid;
}
