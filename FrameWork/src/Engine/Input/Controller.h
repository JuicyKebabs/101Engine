#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <Xinput.h>
#include <array>
#pragma comment(lib, "xinput.lib")

// Forward declaration
struct ControllerInputInfo;

// Constants
static constexpr int CONTROLLERS_MAX = 4;
static constexpr float DEADZONE_L = 0.4f;
static constexpr float DEADZONE_R = 0.4f;

// Struct to hold the state of a controller
struct ControllerState
{
	bool isConnected = false;		// Connection status
	XINPUT_STATE state = {};		// Current frame state data
	XINPUT_STATE prevState = {};	// Previous frame state data
};

// Controller management class
class Controller
{
private:
	// Array to hold the state of multiple controllers
	std::array<ControllerState, CONTROLLERS_MAX> m_controllers{};

public:
	Controller() {};	// Constructor
	~Controller() {};	// Destructor

	void Initialize();								// Initialization
	void Update(ControllerInputInfo* inputInfo);	// Update controller states

	void CopyState();	// Copy previous state to current state

	const ControllerState& GetState(int index) const;	// Get controller state by index

	// Vibration Functions
	void SetVibration(int index, float leftMotor, float rightMotor);	// Set vibration for selected controller
	void SetAllVibrations(float leftMotor, float rightMotor);			// Set vibration for all controllers
	void StopVibration(int index);										// Stop vibration for selected controller
	void StopAllVibrations();											// Stop vibration for all controllers

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