#include "Mouse.h"

#include <windowsx.h>

#include "InputManager.h"

void Mouse::Initialize()
{
	m_current = {};
	m_previous = {};
	m_wheelAccumulator = 0.0f;
	m_rawLookDelta = {};
}

void Mouse::SetRawLookEnabled(bool enabled)
{
	m_rawLookEnabled = enabled;
	m_rawLookDelta = {};
}

void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_MOUSEMOVE:
		UpdateClientPosition(lParam);
		break;
	case WM_INPUT:
		if (m_rawLookEnabled)
		{
			RAWINPUT raw{};
			UINT size = sizeof(raw);
			if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
				&raw, &size, sizeof(RAWINPUTHEADER)) == sizeof(raw) &&
				raw.header.dwType == RIM_TYPEMOUSE &&
				!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
			{
				m_rawLookDelta.x += raw.data.mouse.lLastX;
				m_rawLookDelta.y += raw.data.mouse.lLastY;
			}
		}
		break;
	case WM_LBUTTONDOWN:
		UpdateClientPosition(lParam);
		m_current.left = true;
		break;
	case WM_LBUTTONUP:
		UpdateClientPosition(lParam);
		m_current.left = false;
		break;
	case WM_RBUTTONDOWN:
		UpdateClientPosition(lParam);
		m_current.right = true;
		break;
	case WM_RBUTTONUP:
		UpdateClientPosition(lParam);
		m_current.right = false;
		break;
	case WM_MBUTTONDOWN:
		UpdateClientPosition(lParam);
		m_current.middle = true;
		break;
	case WM_MBUTTONUP:
		UpdateClientPosition(lParam);
		m_current.middle = false;
		break;
	case WM_MOUSEWHEEL:
		m_wheelAccumulator += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
			static_cast<float>(WHEEL_DELTA);
		break;
	case WM_ACTIVATEAPP:
		if (!wParam)
		{
			Initialize();
		}
		break;
	}
}

void Mouse::Update(MouseInputInfo& inputInfo) const
{
	auto updateButton = [](bool current, bool previous, InputState& state)
		{
			state.trigger = current && !previous;
			state.down = current;
			state.up = !current && previous;
		};

	updateButton(m_current.left, m_previous.left, inputInfo.left);
	updateButton(m_current.right, m_previous.right, inputInfo.right);
	updateButton(m_current.middle, m_previous.middle, inputInfo.middle);

	inputInfo.anyButton.trigger =
		inputInfo.left.trigger || inputInfo.right.trigger || inputInfo.middle.trigger;
	inputInfo.anyButton.down =
		inputInfo.left.down || inputInfo.right.down || inputInfo.middle.down;
	inputInfo.anyButton.up =
		inputInfo.left.up || inputInfo.right.up || inputInfo.middle.up;

	inputInfo.clientPositionPixels = m_current.clientPositionPixels;
	inputInfo.deltaPixels = {};
	if (m_current.hasPosition && m_previous.hasPosition)
	{
		inputInfo.deltaPixels.x =
			m_current.clientPositionPixels.x - m_previous.clientPositionPixels.x;
		inputInfo.deltaPixels.y =
			m_current.clientPositionPixels.y - m_previous.clientPositionPixels.y;
	}
	inputInfo.lookDelta = m_rawLookEnabled ? m_rawLookDelta : inputInfo.deltaPixels;

	inputInfo.wheelDelta = m_wheelAccumulator;
}

void Mouse::CopyState()
{
	m_previous = m_current;
	m_wheelAccumulator = 0.0f;
	m_rawLookDelta = {};
}

void Mouse::UpdateClientPosition(LPARAM lParam)
{
	m_current.clientPositionPixels =
	{
		GET_X_LPARAM(lParam),
		GET_Y_LPARAM(lParam)
	};
	m_current.hasPosition = true;
}
