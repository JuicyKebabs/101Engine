#pragma once
#include <d3d12.h>  // DirectX12を使用するため
#include <Xinput.h> // Xboxこんとろーらー入力を可能にする
#include <array>	// std::arrayを使用するため

// XInput.libをリンク	あるならいらない
#pragma comment(lib, "xinput.lib")

//前方宣言
struct ControllerInputInfo;

// コントローラーが接続できる最大数　
static constexpr int CONTROLLERS_MAX = 4;
static constexpr float DEADZONE_L = 0.4f;
static constexpr float DEADZONE_R = 0.4f;

// 1つのコントローラーの状態を表す構造体
struct ControllerState
{
	bool isConnected = false;		// 接続状態
	XINPUT_STATE state = {};		// 現在フレームの状態データ
	XINPUT_STATE prevState = {};	// １フレーム前の状態データ
};

// コントローラー管理クラス
class Controller
{
private:
	// 4つのコントローラーの状態
	std::array<ControllerState, CONTROLLERS_MAX> m_controllers{};

public:
	Controller();	// コンストラクタ
	~Controller();	// デストラクタ

	// 初期化
	void Initialize();

	// 更新処理
	void Update(ControllerInputInfo* inputInfo);

	// 前回の状態を現在の状態にコピー
	void CopyState();

	// 指定したインデックスのコントローラー状態を取得（0~3）
	const ControllerState& GetState(int index) const;

	// Vibration Functions
	void SetVibration(int index, float leftMotor, float rightMotor);
	void SetAllVibrations(float leftMotor, float rightMotor);
	void StopVibration(int index);
	void StopAllVibrations();

private:
	//Update Input State Functions
	void UpdateTriggerState(ControllerState& contState, ControllerInputInfo& inputInfo);	//Button Trigger
	void UpdateDownState(ControllerState& contState, ControllerInputInfo& inputInfo);		//Button Down
	void UpdateUpState(ControllerState& contState, ControllerInputInfo& inputInfo);			//Button Up
	void UpdateStickState(ControllerState& contState, ControllerInputInfo& inputInfo);		//Stick State

	//Input Helper Functions
	bool IsButtonTriggered(WORD buttonFlag, const XINPUT_GAMEPAD& currentState, const XINPUT_GAMEPAD& previousState) const;	//Button Trigger
	bool IsButtonDown(WORD buttonFlag, const XINPUT_GAMEPAD& currentState, const XINPUT_GAMEPAD& previousState) const;		//Button Down
	bool IsButtonUp(WORD buttonFlag, const XINPUT_GAMEPAD& currentState, const XINPUT_GAMEPAD& previousState) const;		//Button Up
	DirectX::XMFLOAT2 ProcessStickInput(SHORT rawX, SHORT rawY, float deadZone) const;										//Stick Processing
};