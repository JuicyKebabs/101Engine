#pragma once
#include "ComPtr.h"
#include <d3d12.h>

// ルートシグネチャクラス
class RootSignature
{
private:
	ComPtr<ID3D12RootSignature> m_rootSignature;	//ルートシグネチャ
	bool m_isValid = false;							//ルートシグネチャ生成に成功したか

private:	//結果コード
	HRESULT result = S_OK;	//HRESULT(成功/失敗コード)

public:
	RootSignature(ID3D12Device* pDevice);	//デフォルトコンストラクタ
	~RootSignature() {};					//デストラクタ

	ID3D12RootSignature* GetRootSignature() const; //ルートシグネチャを取得
};