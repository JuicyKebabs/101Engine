#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include "ComPtr.h"
#include "SharedStruct.h"
#include "InputManager.h"

// カメラクラス
class Camera
{
public:	//公開定数
	static constexpr  DirectX::XMFLOAT3 DEFAULT_POSITION = DirectX::XMFLOAT3(0.0f, 14.0f, -14.0f);		//デフォルトのカメラ位置
	static constexpr  DirectX::XMFLOAT3 DEFAULT_TARGET = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);		//デフォルトの注視点
	static constexpr  DirectX::XMFLOAT3 DEFAULT_UP = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);			//デフォルトの上方向ベクトル

	static constexpr float DEFAULT_FOV = DirectX::XM_PIDIV2 * 1.1f;	//デフォルトの垂直視野角(45度)
	static constexpr float DEFAULT_NEAR_Z = 0.1f;				//デフォルトのニアクリップ距離
	static constexpr float DEFAULT_FAR_Z = 150.0f;				//デフォルトのファークリップ距離

public:	//公開メンバ関数
	Camera() {};	//デフォルトコンストラクタ
	Camera(float window_width, float window_height);	//コンストラクタ
	~Camera() {};	// デストラクタ

	void Initialize();	//初期化
	void Update();		//カメラ更新

	void SetPosition(const DirectX::XMFLOAT3& position);	//カメラの位置を設定
	void SetTarget(const DirectX::XMFLOAT3& target);		//カメラの注視点を設定
	void SetUp(const DirectX::XMFLOAT3& up);				//カメラの上方向ベクトルを設定
	void SetFov(float fov);									//垂直視野角を設定
	void SetAspectRatio(float aspectRatio);					//アスペクト比を設定
	void SetNearZ(float nearZ);								//ニアクリップ距離を設定
	void SetFarZ(float farZ);								//ファークリップ距離を設定

	CameraInfo& GetCameraInfo(); // カメラ情報構造体を取得

	static bool WorldToScreen( // ワールド座標をスクリーン座標に変換
		const CameraInfo& cameraInfo,		//カメラ情報構造体
		const DirectX::XMFLOAT3& worldPos,	//ワールド座標
		DirectX::XMFLOAT2& screenPos,		//スクリーン座標
		float screenWidth,					//画面幅
		float screenHeight					//画面高さ
	);

	void CallShakeCamera(int time, float strength);	// Shake camera

private:	//非公開メンバ変数
	DirectX::XMFLOAT3 m_position{};	//カメラの位置
	DirectX::XMFLOAT3 m_target{};	//カメラの注視点
	DirectX::XMFLOAT3 m_up{};		//カメラの上方向ベクトル
	DirectX::XMFLOAT3 m_right{};	//カメラの右方向ベクトル
	DirectX::XMFLOAT3 m_forward{};	//カメラの前方向ベクトル

	float m_fov = 0.0f;			//垂直視野角
	float m_aspectRatio = 0.0f;	//アスペクト比
	float m_nearZ = 0.0f;		//ニアクリップ距離
	float m_farZ = 0.0f;		//ファークリップ距離

	CameraInfo m_cameraInfo{}; // カメラ情報構造体

	InputInfo* m_pInputInfo = nullptr; // 入力管理クラスのポインタ

	// Parameters for shaking camera
	bool m_isShakeActive = false;			// Active flag of shake
	DirectX::XMFLOAT3 m_originPosition{};	// Originl position
	int m_shakeTime = 0;					// Length of time to keep shaking
	int m_shakeElapsedTime = 0;				// Elapsed time of shaking
	float m_shakeStrength = 0.0f;			// Strenght of shake

private:	//非公開メンバ関数
	void UpdateCameraInfo(); // カメラ情報構造体を更新
	void UpdateShake();
};