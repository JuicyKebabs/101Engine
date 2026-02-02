#include "Camera.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "EventType.h"

using namespace DirectX;

Camera::Camera(float window_width, float window_height)
{
	// デフォルトのカメラ設定
	m_position = DEFAULT_POSITION;					//カメラ位置
	m_target = DEFAULT_TARGET;						//注視点
	m_up = DEFAULT_UP;								//上方向ベクトル
	m_fov = DEFAULT_FOV;							//垂直視野角
	m_aspectRatio = window_width / window_height;	//アスペクト比
	m_nearZ = DEFAULT_NEAR_Z;						//ニアクリップ距離
	m_farZ = DEFAULT_FAR_Z;							//ファークリップ距離
}

//初期化
void Camera::Initialize()
{
	using args = std::pair<int, float>;
	EventManager::GetInstance()->Subscribe<args>(
		EventType::CALL_CAMERA_SHAKE,
		[this](std::shared_ptr<args> data)
		{
			CallShakeCamera(data->first, data->second);
		}
	);
}

//カメラ更新
void Camera::Update()
{

	XMFLOAT3 viewDir =  //カメラの注視点方向ベクトルを計算
	{
		m_target.x - m_position.x,
		m_target.y - m_position.y,
		m_target.z - m_position.z
	};

	viewDir = Normalize(viewDir); //正規化

	m_right = Normalize(Cross(viewDir, m_up));	//右方向ベクトルを計算

	UpdateCameraInfo(); //カメラ情報構造体を更新
}

//カメラの位置を設定
void Camera::SetPosition(const DirectX::XMFLOAT3& position)
{
	m_position = position;
}

//カメラの注視点を設定
void Camera::SetTarget(const DirectX::XMFLOAT3& target)
{
	m_target = target;

	//avoid zero direction vector
	if (m_position.x == m_target.x &&
		m_position.y == m_target.y &&
		m_position.z == m_target.z)
	{
		m_target.z += 0.01f;
	}
}

//カメラの上方向ベクトルを設定
void Camera::SetUp(const DirectX::XMFLOAT3& up)
{
	m_up = up;
}

//垂直視野角を設定
void Camera::SetFov(float fov)
{
	m_fov = fov;
}

//アスペクト比を設定
void Camera::SetAspectRatio(float aspectRatio)
{
	m_aspectRatio = aspectRatio;
}

//ニアクリップ距離を設定
void Camera::SetNearZ(float nearZ)
{
	m_nearZ = nearZ;
}

//ファークリップ距離を設定
void Camera::SetFarZ(float farZ)
{
	m_farZ = farZ;
}

//カメラ情報構造体を取得
CameraInfo& Camera::GetCameraInfo()
{
	UpdateCameraInfo(); //カメラ情報構造体を更新

	return m_cameraInfo;	//カメラ情報構造体を返す
}

//ワールド座標をスクリーン座標に変換
bool Camera::WorldToScreen(
	const CameraInfo& cameraInfo,
	const DirectX::XMFLOAT3& worldPos,
	DirectX::XMFLOAT2& screenPos,
	float screenWidth,
	float screenHeight)
{
	using namespace DirectX;

	// Validate projection parameters to avoid DirectXMath debug asserts
	if (!std::isfinite(cameraInfo.fov) || !(cameraInfo.fov > 0.0f && cameraInfo.fov < XM_PI))
	{
		return false;
	}
	if (!std::isfinite(cameraInfo.aspectRatio) || cameraInfo.aspectRatio <= 0.0f)
	{
		return false;
	}
	if (!std::isfinite(cameraInfo.nearZ) || !(cameraInfo.nearZ > 0.0f))
	{
		return false;
	}
	if (!std::isfinite(cameraInfo.farZ) || !(cameraInfo.farZ > cameraInfo.nearZ))
	{
		return false;
	}

	// View行列
	XMMATRIX view = XMMatrixLookAtLH(
		XMLoadFloat3(&cameraInfo.position), //カメラ位置
		XMLoadFloat3(&cameraInfo.target),   //注視点
		XMLoadFloat3(&cameraInfo.up)        //上方向ベクトル
	);

	// Projection行列
	XMMATRIX proj = XMMatrixPerspectiveFovLH(
		cameraInfo.fov,         //垂直視野角
		cameraInfo.aspectRatio, //アスペクト比
		cameraInfo.nearZ,       //ニアクリップ
		cameraInfo.farZ         //ファークリップ
	);

	// world * view * proj
	XMMATRIX viewProj = view * proj;

	// world座標 → clip座標
	XMVECTOR clipPos = XMVector4Transform(
		XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f),
		viewProj
	);

	float w = XMVectorGetW(clipPos);
	if (w <= 0.0f)
	{
		// カメラ背面 → 描画しない
		return false;
	}

	float x = XMVectorGetX(clipPos) / w; // -1 ～ +1
	float y = XMVectorGetY(clipPos) / w; // -1 ～ +1

	// ★ここが「中心 (0,0)」版
	screenPos.x = x * (screenWidth * 0.5f);
	screenPos.y = y * (screenHeight * 0.5f); // 下を+にしたいなら -y、上を+にしたいなら y

	return true;
}

void Camera::CallShakeCamera(int time, float strength)
{
	m_isShakeActive = true;
	m_originPosition = m_position;
	m_shakeTime = time;
	m_shakeStrength = strength;
	m_shakeElapsedTime = 0;
}

//カメラ情報構造体を更新
void Camera::UpdateCameraInfo()
{
	m_cameraInfo.position = m_position;			//カメラの位置
	m_cameraInfo.target = m_target;				//カメラの注視点
	m_cameraInfo.up = m_up;						//カメラの上方向ベクトル
	m_cameraInfo.right = m_right;				//カメラの右方向ベクトル
	m_cameraInfo.fov = m_fov;					//垂直視野角
	m_cameraInfo.aspectRatio = m_aspectRatio;	//アスペクト比
	m_cameraInfo.nearZ = m_nearZ;				//ニアクリップ距離
	m_cameraInfo.farZ = m_farZ;					//ファークリップ距離
}

void Camera::UpdateShake()
{
	if (!m_isShakeActive) return;

	if (m_shakeElapsedTime < m_shakeTime)
	{
		auto position = m_position;

		//シェイク処理
		float shakeAmount = m_shakeStrength;	//シェイクの強さ
		position.x = m_originPosition.x + (rand() % (int)(shakeAmount * 2)) - shakeAmount;
		position.y = m_originPosition.y + (rand() % (int)(shakeAmount * 2)) - shakeAmount;
		m_position = position;

		m_shakeElapsedTime++;
	}
	else
	{
		//元の位置に戻す
		m_position.x = m_originPosition.x;
		m_position.y = m_originPosition.y;

		m_isShakeActive = false;
	}
}
