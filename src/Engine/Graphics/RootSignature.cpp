#include "RootSignature.h"
#include "Engine/Engine.h"

//デフォルトコンストラクタ
RootSignature::RootSignature(ID3D12Device* pDevice)
{
	//ルートシグネチャフラグの設定
	auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // アプリケーションの入力アセンブラを使用する
	flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS; // ドメインシェーダーのルートシグネチャへんアクセスを拒否する
	flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS; // ハルシェーダーのルートシグネチャへんアクセスを拒否する
	flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS; // ジオメトリシェーダーのルートシグネチャへんアクセスを拒否する

	//b0の定数バッファを設定
	CD3DX12_ROOT_PARAMETER rootParam[3] = {};	//ルートパラメータ
	rootParam[0].InitAsConstantBufferView(
		0,							//シェーダーレジスタb0
		0,							//レジスタスペース0
		D3D12_SHADER_VISIBILITY_ALL	//全てのシェーダーステージから見える
	);

	//t0のシェーダーリソースビューを設定
	CD3DX12_DESCRIPTOR_RANGE tableRange[1] = {}; //ディスクリプタテーブル

	//ディスクリプタレンジの設定
	tableRange[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,	//シェーダーリソースビュー
		1,									//ディスクリプタ数1
		0									//シェーダーレジスタt0
	);

	//ディスクリプタテーブルを設定
	rootParam[1].InitAsDescriptorTable(
		std::size(tableRange),		//ディスクリプタレンジの数
		tableRange,					//ディスクリプタレンジ
		D3D12_SHADER_VISIBILITY_ALL	//全てのシェーダーステージから見える
	);

	//b1の定数バッファを設定
	rootParam[2].InitAsConstantBufferView(
		1							//シェーダーレジスタb1
	);

	//スタティックサンプラーの設定
	auto sampler = CD3DX12_STATIC_SAMPLER_DESC(
		0,								//シェーダーレジスタs0
		D3D12_FILTER_MIN_MAG_MIP_LINEAR	//フィルタリング方法(バイリニアフィルタリング)
	);

	//ルートシグネチャの設定
	CD3DX12_ROOT_SIGNATURE_DESC desc = {};	//設定構造体
	desc.NumParameters = _countof(rootParam);	//ルートパラメータの数
	desc.NumStaticSamplers = 1;					//スタティックサンプラーの数
	desc.pParameters = rootParam;				//ルートパラメータ
	desc.pStaticSamplers = &sampler;			//スタティックサンプラー
	desc.Flags = flag;							//ルートシグネチャフラグ

	//シリアライズ
	ComPtr<ID3DBlob> pBlob;		//シグネチャ
	ComPtr<ID3DBlob> pError;	//エラー

	result = D3D12SerializeRootSignature(
		&desc,							//ルートシグネチャの設定
		D3D_ROOT_SIGNATURE_VERSION_1,	//ルートシグネチャのバージョン
		&pBlob,							//シグネチャ
		&pError							//エラー
	);

	if(FAILED(result))
	{
		//シリアライズに失敗した場合はエラー内容を表示して終了
		if (pError)
		{
			OutputDebugStringA((char*)pError->GetBufferPointer());
			m_isValid = false;
		}
		return;
	}

	//ルートシグネチャの生成
	result = pDevice->CreateRootSignature(
		0,								//ノードマスク(シングルGPUなので0)
		pBlob->GetBufferPointer(),		//シグネチャのバッファポインタ
		pBlob->GetBufferSize(),			//シグネチャのバッファサイズ
		IID_PPV_ARGS(&m_rootSignature)	//ルートシグネチャのアドレスを取得
	);

	if (FAILED(result))
	{
		OutputDebugStringA("ルートシグネチャの生成に失敗しました。");
		m_isValid = false;
		return;
	}

	m_isValid = true;	//ルートシグネチャ生成成功
}

//ルートシグネチャを取得
ID3D12RootSignature* RootSignature::GetRootSignature() const
{
	return m_rootSignature.Get();
}
