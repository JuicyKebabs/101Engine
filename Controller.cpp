#include "App.h"
#include "Controller.h"
#include <cmath>
#include <cassert>
#include "InputManager.h"

using namespace DirectX;

// Initialization
void Controller::Initialize()
{
	// Clear controller states
	ZeroMemory(m_controllers.data(), sizeof(ControllerState) * CONTROLLERS_MAX);
}

// Update controller states
void Controller::Update(ControllerInputInfo* inputInfo)
{
	// Process each of the four controllers
	for (int i = 0; i < CONTROLLERS_MAX; ++i)
	{
		ControllerState& cs = m_controllers[i];			// Get the state of controller 0-3
		DWORD dwResult = XInputGetState(i, &cs.state);	// Get the state of the controller

		if (dwResult == ERROR_SUCCESS)
		{
			cs.isConnected = true;	// Connected

			// Update trigger information
			UpdateTriggerState(cs, inputInfo[i]);	// Compare current and previous states to determine trigger state
			// Update button down information
			UpdateDownState(cs, inputInfo[i]);		// Compare current and previous states to determine button down state
			// Update button up information
			UpdateUpState(cs, inputInfo[i]);		// Compare current and previous states to determine button up state
			// Update stick information
			UpdateStickState(cs, inputInfo[i]);	// Update stick state
		}
		else
		{
			// Not connected
			cs.isConnected = false;
			// Clear state (initialize to prevent old state from lingering)
			cs.state = {};
			cs.prevState = {};
			inputInfo[i] = ControllerInputInfo{};
		}
	}
}

// Copy previous state to current state
void Controller::CopyState()
{
	for (int i = 0; i < CONTROLLERS_MAX; ++i)
	{
		ControllerState& cs = m_controllers[i];

		if (cs.isConnected)
		{// If connected
			cs.prevState = cs.state;	// Copy current state to previous state
		}
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

// Get controller state by index
const ControllerState& Controller::GetState(int index) const
{
	assert(index >= 0 && index < CONTROLLERS_MAX);
	return m_controllers[index];	
}

// Update Button Trigger State
void Controller::UpdateTriggerState(ControllerState& cs, ControllerInputInfo& inputInfo)
{
	const XINPUT_GAMEPAD& current = cs.state.Gamepad;
	const XINPUT_GAMEPAD& previous = cs.prevState.Gamepad;

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

	inputInfo.UP.trigger= IsButtonTriggered(XINPUT_GAMEPAD_DPAD_UP, current, previous);
	inputInfo.DOWN.trigger= IsButtonTriggered(XINPUT_GAMEPAD_DPAD_DOWN, current, previous);
	inputInfo.LEFT.trigger= IsButtonTriggered(XINPUT_GAMEPAD_DPAD_LEFT, current, previous);
	inputInfo.RIGHT.trigger = IsButtonTriggered(XINPUT_GAMEPAD_DPAD_RIGHT, current, previous);

	inputInfo.anyButton.trigger =
		inputInfo.A.trigger || inputInfo.B.trigger || inputInfo.X.trigger || inputInfo.Y.trigger ||
		inputInfo.START.trigger || inputInfo.BACK.trigger ||
		inputInfo.LSHOULDER.trigger || inputInfo.RSHOULDER.trigger ||
		inputInfo.LTHUMB.trigger || inputInfo.RTHUMB.trigger ||
		inputInfo.UP.trigger || inputInfo.DOWN.trigger ||
		inputInfo.LEFT.trigger || inputInfo.RIGHT.trigger;
}

// Update Button Down State
void Controller::UpdateDownState(ControllerState& contState, ControllerInputInfo& inputInfo)
{
	const XINPUT_GAMEPAD& current = contState.state.Gamepad;
	const XINPUT_GAMEPAD& previous = contState.prevState.Gamepad;

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
	return (currentState.wButtons & buttonFlag) && !(previousState.wButtons & buttonFlag);
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
