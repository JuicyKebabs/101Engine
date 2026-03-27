#include "PipelineState.h"
#include "Engine/Engine.h"

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
	rt0.BlendEnable = FALSE;									//ブレンドを無効にする
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

//頂点シェーダーを設定（エントリーポイント指定版）
void PipelineState::SetVertexShader(ID3DBlob* blob)
{
	m_vsBlob = blob;
	m_desc.VS = { blob->GetBufferPointer(), blob->GetBufferSize() };
}

//ピクセルシェーダーを設定（エントリーポイント指定版）
void PipelineState::SetPixelShader(ID3DBlob* blob)
{
	m_psBlob = blob;
	m_desc.PS = { blob->GetBufferPointer(), blob->GetBufferSize() };
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

//ブレンドモードを設定
void PipelineState::SetBlendMode(BlendMode mode)
{
	auto& rt0 = m_desc.BlendState.RenderTarget[0];

	rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	rt0.SrcBlendAlpha = D3D12_BLEND_ZERO;
	rt0.DestBlendAlpha = D3D12_BLEND_ONE;
	rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	switch (mode)
	{
	case BlendMode::Opaque:
		rt0.BlendEnable = FALSE;
		rt0.SrcBlend = D3D12_BLEND_ONE;
		rt0.DestBlend = D3D12_BLEND_ZERO;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Alpha:
		rt0.BlendEnable = TRUE;
		rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::AddAlpha:
		rt0.BlendEnable = TRUE;
		rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt0.DestBlend = D3D12_BLEND_ONE;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Add:
		rt0.BlendEnable = TRUE;
		rt0.SrcBlend = D3D12_BLEND_ONE;
		rt0.DestBlend = D3D12_BLEND_ONE;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Multiply:
		rt0.BlendEnable = TRUE;
		rt0.SrcBlend = D3D12_BLEND_DEST_COLOR;
		rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt0.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	default:
		break;
	}
}

//深度モードを設定
void PipelineState::SetDepthMode(DepthMode mode)
{
	switch (mode)
	{
	case DepthMode::Disable:
		m_desc.DepthStencilState.DepthEnable = FALSE; //深度テスト無効
		m_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; //深度書き込み無効
		m_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //深度関数をLESS_EQUALに設定
		break;
	case DepthMode::TestWrite:
		m_desc.DepthStencilState.DepthEnable = TRUE; //深度テスト有効
		m_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; //深度書き込み有効
		m_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //深度関数をLESS_EQUALに設定
		break;
	case DepthMode::TestNoWrite:
		m_desc.DepthStencilState.DepthEnable = TRUE; //深度テスト有効
		m_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; //深度書き込み無効
		m_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //深度関数をLESS_EQUALに設定
		break;
	default:
		break;
	}
}

//カリングモードを設定
void PipelineState::SetCullMode(CullMode mode)
{
	switch (mode)
	{
	case CullMode::None:
		m_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; //カリングしない
		break;
	case CullMode::Front:
		m_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; //フロントカリング
		break;
	case CullMode::Back:
		m_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; //バックカリング
		break;
	default:
		break;
	}
}

//レンダーターゲットのフォーマットを設定
void PipelineState::SetFormat(RenderTargetFormat format)
{
	switch (format)
	{
	case RenderTargetFormat::LDR:
		m_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //LDRフォーマット
		break;
	case RenderTargetFormat::HDR:
		m_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; //HDRフォーマット
		break;
	default:
		break;
	}
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
