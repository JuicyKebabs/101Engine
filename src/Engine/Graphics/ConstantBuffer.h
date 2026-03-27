#pragma once
#include <d3d12.h>
#include "Engine/Core/ComPtr/ComPtr.h"

// 定数バッファ構造体
struct ConstantBuffer
{
private:
    bool m_IsValid = false;                     //定数バッファ生成に成功したか
    ComPtr<ID3D12Resource> m_pBuffer;           //定数バッファ
    D3D12_CONSTANT_BUFFER_VIEW_DESC m_Desc{};   //定数バッファビューの設定
	void* m_pMappedPtr = nullptr;               //マッピング後のポインタ

	ConstantBuffer(const ConstantBuffer&) = delete;     //コピーコンストラクタ禁止
	void operator = (const ConstantBuffer&) = delete;	//代入演算子禁止
private:    //結果コード
	HRESULT result = S_OK;    //HRESULT(成功/失敗コード)

public:
    ConstantBuffer(ID3D12Device* pDevice, size_t size); //コンストラクタで定数バッファを生成
    ~ConstantBuffer() {};                               //デストラクタ

	//ゲッター
    bool GetIsValid();                              //バッファ生成に成功したかを返す
    D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const;   //バッファのGPU上のアドレスを返す
    D3D12_CONSTANT_BUFFER_VIEW_DESC GetViewDesc();  //定数バッファビューを返す
    void* GetPtr() const;                           //定数バッファにマッピングされたポインタを返す

    template<typename T>
    T* GetPtr()
    {
        return reinterpret_cast<T*>(GetPtr());
    }
};
