#include "App.h"
#include "Controller.h"
#include <cmath>	//デッドゾーン処理などで使用する
#include <cassert>
#include "InputManager.h"

using namespace DirectX;

//コンストラクタ
Controller::Controller() {}		
//デスコンストラクタ
Controller::~Controller() {}	

void Controller::Initialize()
{
	// 特になし
}

// 更新
void Controller::Update(ControllerInputInfo* inputInfo)
{
	//最大４つのこんとろーらーを順に確認する（０～３）
	for (int i = 0; i < CONTROLLERS_MAX; ++i)
	{
		ControllerState& cs = m_controllers[i];	//０～３の状態を取得

		// コントローラーの状態を取得
		DWORD dwResult = XInputGetState(i, &cs.state);

		if (dwResult == ERROR_SUCCESS)
		{
			// 接続されている
			cs.isConnected = true;

			// トリガー情報の更新
			UpdateTriggerState(cs, inputInfo[i]);	//現在の状態と前回の状態を比較してトリガー判定をする
			// 押下情報の更新
			UpdateDownState(cs, inputInfo[i]);		//現在の状態と前回の状態を比較して押下判定をする
			// 離下情報の更新
			UpdateUpState(cs, inputInfo[i]);		//現在の状態と前回の状態を比較して離下判定をする
			// スティック情報の更新
			UpdateStickState(cs, inputInfo[i]);	//スティックの状態を更新
		}
		else
		{
			// 接続されていない
			cs.isConnected = false;
			// 状態をクリア　（古い状態が残らないよう初期化）
			cs.state = {};
			cs.prevState = {};
			inputInfo[i] = ControllerInputInfo{};
		}
	}
}

// 状態コピー
void Controller::CopyState()
{//４つのコントローラーを順に処理する
	for (int i = 0; i < CONTROLLERS_MAX; ++i)
	{
		ControllerState& cs = m_controllers[i];
		if (cs.isConnected)	//接続されているコントローラーのみ
		{
			// 現在の状態を前回の状態として保存
			cs.prevState = cs.state;
		}
		// 接続されていない場合はUpdateでクリアされているため処理は不要
	}
}

// Set Vibration for selected controller
void Controller::SetVibration(int index, float leftMotor, float rightMotor)
{
	XINPUT_VIBRATION vibration = {};
	vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
	vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);
	XInputSetState(index, &vibration);
}

// Set Vibration for all controllers
void Controller::SetAllVibrations(float leftMotor, float rightMotor)
{
	for (int i = 0; i < CONTROLLERS_MAX; ++i)
	{
		SetVibration(i, leftMotor, rightMotor);
	}
}

// Stop Vibration for selected controller
void Controller::StopVibration(int index)
{
	XINPUT_VIBRATION vibration = {};
	vibration.wLeftMotorSpeed = 0;
	vibration.wRightMotorSpeed = 0;
	XInputSetState(index, &vibration);
}

// Stop Vibration for all controllers
void Controller::StopAllVibrations()
{
	for (int i = 0; i < CONTROLLERS_MAX; ++i)
	{
		StopVibration(i);
	}
}

// 指定したインデックスのコントローラー状態を取得
const ControllerState& Controller::GetState(int index) const
{
	// 範囲チェックは省略するが、通常はここでアサートなどを行うべき（だそうです）
	// indexで指定されたこんとろーらー状態を返す
	assert(index >= 0 && index < CONTROLLERS_MAX);
	return m_controllers[index];	
}

// ボタンのトリガー状態を更新
void Controller::UpdateTriggerState(ControllerState& cs, ControllerInputInfo& inputInfo)
{
	const XINPUT_GAMEPAD& current = cs.state.Gamepad;
	const XINPUT_GAMEPAD& previous = cs.prevState.Gamepad;

	// トリガー状態の計算
	// IsButtonTriggered(ボタンのフラグ, 現在のGamepad, 前回のGamepad)

	// ボタン　ABXYor〇×△□
	inputInfo.A.trigger = IsButtonTriggered(XINPUT_GAMEPAD_A, current, previous);
	inputInfo.B.trigger = IsButtonTriggered(XINPUT_GAMEPAD_B, current, previous);
	inputInfo.X.trigger = IsButtonTriggered(XINPUT_GAMEPAD_X, current, previous);
	inputInfo.Y.trigger = IsButtonTriggered(XINPUT_GAMEPAD_Y, current, previous);
	
	inputInfo.START.trigger = IsButtonTriggered(XINPUT_GAMEPAD_START, current, previous);
	inputInfo.BACK.trigger = IsButtonTriggered(XINPUT_GAMEPAD_BACK, current, previous);
	inputInfo.LSHOULDER.trigger = IsButtonTriggered(XINPUT_GAMEPAD_LEFT_SHOULDER, current, previous);
	inputInfo.RSHOULDER.trigger = IsButtonTriggered(XINPUT_GAMEPAD_RIGHT_SHOULDER, current, previous);
	inputInfo.LTHUMB.trigger = IsButtonTriggered(XINPUT_GAMEPAD_LEFT_THUMB, current, previous);
	inputInfo.RTHUMB.trigger = IsButtonTriggered(XINPUT_GAMEPAD_RIGHT_THUMB, current, previous);

	// 方向キーのトリガー判定
	inputInfo.UP.trigger= IsButtonTriggered(XINPUT_GAMEPAD_DPAD_UP, current, previous);
	inputInfo.DOWN.trigger= IsButtonTriggered(XINPUT_GAMEPAD_DPAD_DOWN, current, previous);
	inputInfo.LEFT.trigger= IsButtonTriggered(XINPUT_GAMEPAD_DPAD_LEFT, current, previous);
	inputInfo.RIGHT.trigger = IsButtonTriggered(XINPUT_GAMEPAD_DPAD_RIGHT, current, previous);

	// 任意のボタンが押されたかどうかを判定
	inputInfo.anyButton.trigger =
		inputInfo.A.trigger || inputInfo.B.trigger || inputInfo.X.trigger || inputInfo.Y.trigger ||
		inputInfo.START.trigger || inputInfo.BACK.trigger ||
		inputInfo.LSHOULDER.trigger || inputInfo.RSHOULDER.trigger ||
		inputInfo.LTHUMB.trigger || inputInfo.RTHUMB.trigger ||
		inputInfo.UP.trigger || inputInfo.DOWN.trigger ||
		inputInfo.LEFT.trigger || inputInfo.RIGHT.trigger;

	// Note: スティックとトリガーはアナログ入力のため、
	// `trigger` (押された瞬間) の概念を適用するのは一般的ではありません。
	// 必要であれば、デッドゾーンを超えた瞬間をトリガーとみなすなどのカスタムロジックをここに追加できます。
}

// ボタンの押下状態を更新
void Controller::UpdateDownState(ControllerState& contState, ControllerInputInfo& inputInfo)
{
	const XINPUT_GAMEPAD& current = contState.state.Gamepad;
	const XINPUT_GAMEPAD& previous = contState.prevState.Gamepad;

	// ボタンの押下状態を更新
	inputInfo.A.down = IsButtonDown(XINPUT_GAMEPAD_A, current, previous);
	inputInfo.B.down = IsButtonDown(XINPUT_GAMEPAD_B, current, previous);
	inputInfo.X.down = IsButtonDown(XINPUT_GAMEPAD_X, current, previous);
	inputInfo.Y.down = IsButtonDown(XINPUT_GAMEPAD_Y, current, previous);

	inputInfo.START.down = IsButtonDown(XINPUT_GAMEPAD_START, current, previous);
	inputInfo.BACK.down = IsButtonDown(XINPUT_GAMEPAD_BACK, current, previous);
	inputInfo.LSHOULDER.down = IsButtonDown(XINPUT_GAMEPAD_LEFT_SHOULDER, current, previous);
	inputInfo.RSHOULDER.down = IsButtonDown(XINPUT_GAMEPAD_RIGHT_SHOULDER, current, previous);
	inputInfo.LTHUMB.down = IsButtonDown(XINPUT_GAMEPAD_LEFT_THUMB, current, previous);
	inputInfo.RTHUMB.down = IsButtonDown(XINPUT_GAMEPAD_RIGHT_THUMB, current, previous);

	// 方向キーの押下判定
	inputInfo.UP.down = IsButtonDown(XINPUT_GAMEPAD_DPAD_UP, current, previous);
	inputInfo.DOWN.down = IsButtonDown(XINPUT_GAMEPAD_DPAD_DOWN, current, previous);
	inputInfo.LEFT.down = IsButtonDown(XINPUT_GAMEPAD_DPAD_LEFT, current, previous);
	inputInfo.RIGHT.down = IsButtonDown(XINPUT_GAMEPAD_DPAD_RIGHT, current, previous);

	inputInfo.anyButton.down =
		inputInfo.A.down || inputInfo.B.down || inputInfo.X.down || inputInfo.Y.down ||
		inputInfo.START.down || inputInfo.BACK.down ||
		inputInfo.LSHOULDER.down || inputInfo.RSHOULDER.down ||
		inputInfo.LTHUMB.down || inputInfo.RTHUMB.down ||
		inputInfo.UP.down || inputInfo.DOWN.down ||
		inputInfo.LEFT.down || inputInfo.RIGHT.down;
}

// Update Button Up State
void Controller::UpdateUpState(ControllerState& contState, ControllerInputInfo& inputInfo)
{
	const XINPUT_GAMEPAD& current = contState.state.Gamepad;
	const XINPUT_GAMEPAD& previous = contState.prevState.Gamepad;

	// ボタンの離下状態を更新
	inputInfo.A.up = IsButtonUp(XINPUT_GAMEPAD_A, current, previous);
	inputInfo.B.up = IsButtonUp(XINPUT_GAMEPAD_B, current, previous);
	inputInfo.X.up = IsButtonUp(XINPUT_GAMEPAD_X, current, previous);
	inputInfo.Y.up = IsButtonUp(XINPUT_GAMEPAD_Y, current, previous);
	inputInfo.START.up = IsButtonUp(XINPUT_GAMEPAD_START, current, previous);
	inputInfo.BACK.up = IsButtonUp(XINPUT_GAMEPAD_BACK, current, previous);
	inputInfo.LSHOULDER.up = IsButtonUp(XINPUT_GAMEPAD_LEFT_SHOULDER, current, previous);
	inputInfo.RSHOULDER.up = IsButtonUp(XINPUT_GAMEPAD_RIGHT_SHOULDER, current, previous);
	inputInfo.LTHUMB.up = IsButtonUp(XINPUT_GAMEPAD_LEFT_THUMB, current, previous);
	inputInfo.RTHUMB.up = IsButtonUp(XINPUT_GAMEPAD_RIGHT_THUMB, current, previous);

	// 方向キーの離下判定
	inputInfo.UP.up = IsButtonUp(XINPUT_GAMEPAD_DPAD_UP, current, previous);
	inputInfo.DOWN.up = IsButtonUp(XINPUT_GAMEPAD_DPAD_DOWN, current, previous);
	inputInfo.LEFT.up = IsButtonUp(XINPUT_GAMEPAD_DPAD_LEFT, current, previous);
	inputInfo.RIGHT.up = IsButtonUp(XINPUT_GAMEPAD_DPAD_RIGHT, current, previous);

	inputInfo.anyButton.up =
		inputInfo.A.up || inputInfo.B.up || inputInfo.X.up || inputInfo.Y.up ||
		inputInfo.START.up || inputInfo.BACK.up ||
		inputInfo.LSHOULDER.up || inputInfo.RSHOULDER.up ||
		inputInfo.LTHUMB.up || inputInfo.RTHUMB.up ||
		inputInfo.UP.up || inputInfo.DOWN.up ||
		inputInfo.LEFT.up || inputInfo.RIGHT.up;
}

// Update Stick State
void Controller::UpdateStickState(ControllerState& contState, ControllerInputInfo& inputInfo)
{
	XINPUT_GAMEPAD& pad = contState.state.Gamepad;	//Get current gamepad state

	inputInfo.leftStickPast = inputInfo.leftStick;		//Store previous left stick state
	inputInfo.rightStickPast = inputInfo.rightStick;	//Store previous right stick state

	// Process left and right stick inputs with deadzone handling
	inputInfo.leftStick = ProcessStickInput(
		pad.sThumbLX,
		pad.sThumbLY,
		DEADZONE_L
	);

	// Right stick
	inputInfo.rightStick = ProcessStickInput(
		pad.sThumbRX,
		pad.sThumbRY,
		DEADZONE_R
	);
}

// Check if a specific button was triggered
bool Controller::IsButtonTriggered(WORD buttonFlag, const XINPUT_GAMEPAD& currentState, const XINPUT_GAMEPAD& previousState) const
{
	// 現在押されている **かつ** 前回押されていなかった場合にtrue
	return (currentState.wButtons & buttonFlag) && !(previousState.wButtons & buttonFlag);
	// 現在の状態でボタンが押されている (currentState.wButtons & buttonFlag)
	// かつ
	// 前回の状態でボタンが押されていなかった (!(previousState.wButtons & buttonFlag))
	// の両方を満たす場合に、押された瞬間としてtrueを返す

}

// Check if a specific button is held down
bool Controller::IsButtonDown(WORD buttonFlag, const XINPUT_GAMEPAD& currentState, const XINPUT_GAMEPAD& previousState) const
{
	return currentState.wButtons & buttonFlag;
}

// Check if a specific button was released
bool Controller::IsButtonUp(WORD buttonFlag, const XINPUT_GAMEPAD& currentState, const XINPUT_GAMEPAD& previousState) const
{
	return !(currentState.wButtons & buttonFlag) && (previousState.wButtons & buttonFlag);
}

// Process Stick Input
DirectX::XMFLOAT2 Controller::ProcessStickInput(SHORT rawX, SHORT rawY, float deadZone) const
{
	float x = static_cast<float>(rawX);	//X stick value
	float y = static_cast<float>(rawY);	//Y stick value
	
	float magnitude = std::sqrt(x * x + y * y);	//slopw and direction

	constexpr float STICK_MAX = 32767.0f;	//Max value for stick input

	//Normalize magnitude to range 0.0f - 1.0f
	float normalized = (magnitude - deadZone) / (STICK_MAX - deadZone);
	if (normalized > 1.0f) normalized = 1.0f;

	float dirX = 0.0f;
	float dirY = 0.0f;

	//Calculate direction
	if(magnitude < 0.0001f)
	{
		//Avoid division by zero
		return DirectX::XMFLOAT2(0.0f, 0.0f);
	}
	else
	{
		dirX = x / magnitude;	//direction x
		dirY = y / magnitude;	//direction y
	}

	//Apply deadzone
	if (fabs(dirX) < deadZone) dirX = 0.0f;
	if (fabs(dirY) < deadZone) dirY = 0.0f;

	return DirectX::XMFLOAT2(dirX * normalized, dirY * normalized);
}
