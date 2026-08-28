#include "Engine/Window/Window.h"

#include "Engine/Core/Debug/Debug.h"

Window::~Window()
{
	Terminate();
}

bool Window::Initialize(const InitDesc& desc)
{
	if (m_hwnd || m_classRegistered)
	{
		DBG("Window::Initialize: Window is already initialized.");
		return false;
	}

	if (desc.clientWidth == 0 || desc.clientHeight == 0 || desc.className.empty())
	{
		DBG("Window::Initialize: Invalid initialization parameters.");
		return false;
	}

	m_instance = desc.instance ? desc.instance : GetModuleHandle(nullptr);
	m_className = desc.className;
	m_messageCallback = desc.messageCallback;
	m_clientSize = { desc.clientWidth, desc.clientHeight };
	m_allowFullscreenToggle = desc.allowFullscreenToggle;

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &Window::StaticWindowProcedure;
	windowClass.hInstance = m_instance;
	windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	windowClass.lpszClassName = m_className.c_str();

	if (!RegisterClassExW(&windowClass))
	{
		DBG("Window::Initialize: Failed to register window class. Error: %lu", GetLastError());
		Terminate();
		return false;
	}

	m_classRegistered = true;

	const DWORD style = desc.resizable
		? WS_OVERLAPPEDWINDOW
		: (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

	RECT windowRect
	{
		0,
		0,
		static_cast<LONG>(desc.clientWidth),
		static_cast<LONG>(desc.clientHeight)
	};

	if (!AdjustWindowRectEx(&windowRect, style, FALSE, 0))
	{
		DBG("Window::Initialize: Failed to calculate window size. Error: %lu", GetLastError());
		Terminate();
		return false;
	}

	m_hwnd = CreateWindowExW(
		0,
		m_className.c_str(),
		desc.title.c_str(),
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		m_instance,
		this);

	if (!m_hwnd)
	{
		DBG("Window::Initialize: Failed to create window. Error: %lu", GetLastError());
		Terminate();
		return false;
	}

	return true;
}

void Window::Terminate()
{
	// The callback may refer to App-owned systems such as ImGui. Stop forwarding
	// messages before destroying the native window during application shutdown.
	m_messageCallback = {};

	if (m_hwnd)
	{
		DestroyWindow(m_hwnd);
		m_hwnd = nullptr;
	}

	if (m_classRegistered)
	{
		UnregisterClassW(m_className.c_str(), m_instance);
		m_classRegistered = false;
	}

	m_instance = nullptr;
	m_className.clear();
	m_clientSize = {};
	m_requestedSize = {};
	m_hasResizeRequest = false;
	m_isMinimized = false;
	m_allowFullscreenToggle = false;
	m_mode = Mode::Windowed;
	m_requestedMode = Mode::Windowed;
	m_hasModeChangeRequest = false;
	m_windowedStyle = 0;
	m_windowedPlacement = { sizeof(WINDOWPLACEMENT) };
	m_hasWindowedPlacement = false;
}

void Window::Show(int command) const
{
	if (m_hwnd)
	{
		ShowWindow(m_hwnd, command);
	}
}

float Window::GetAspectRatio() const
{
	if (m_clientSize.width == 0 || m_clientSize.height == 0)
	{
		return 1.0f;
	}

	return static_cast<float>(m_clientSize.width) /
		static_cast<float>(m_clientSize.height);
}

bool Window::GetResizeRequest(Size& outSize) const
{
	if (!m_hasResizeRequest)
	{
		return false;
	}

	outSize = m_requestedSize;
	return true;
}

void Window::CommitResize()
{
	if (!m_hasResizeRequest)
	{
		return;
	}

	m_clientSize = m_requestedSize;
	m_hasResizeRequest = false;
}

void Window::RequestMode(Mode mode)
{
	m_requestedMode = mode;
	m_hasModeChangeRequest = m_requestedMode != m_mode;
}

void Window::RequestToggleFullscreen()
{
	if (!m_allowFullscreenToggle)
	{
		return;
	}

	const Mode baseMode = m_hasModeChangeRequest ? m_requestedMode : m_mode;
	RequestMode(baseMode == Mode::Windowed ? Mode::BorderlessFullscreen : Mode::Windowed);
}

bool Window::ApplyPendingModeChange()
{
	if (!m_hasModeChangeRequest)
	{
		return true;
	}

	const Mode requestedMode = m_requestedMode;
	m_hasModeChangeRequest = false;

	if (requestedMode == m_mode)
	{
		return true;
	}

	const bool succeeded = requestedMode == Mode::BorderlessFullscreen
		? EnterBorderlessFullscreen()
		: ExitBorderlessFullscreen();

	if (succeeded)
	{
		m_mode = requestedMode;
	}

	return succeeded;
}

LRESULT CALLBACK Window::StaticWindowProcedure(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	Window* window = nullptr;

	if (message == WM_NCCREATE)
	{
		auto* createInfo = reinterpret_cast<CREATESTRUCT*>(lParam);
		window = static_cast<Window*>(createInfo->lpCreateParams);

		if (window)
		{
			window->m_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		}
	}
	else
	{
		window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (!window)
	{
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}

	const LRESULT result = window->HandleMessage(message, wParam, lParam);

	if (message == WM_NCDESTROY)
	{
		SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
		window->m_hwnd = nullptr;
	}

	return result;
}

LRESULT Window::HandleMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	bool handled = false;
	LRESULT result = 0;

	// Window lifecycle state is updated before application-specific handling.
	switch (message)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			m_isMinimized = true;
		}
		else
		{
			m_isMinimized = false;
			RequestResize(LOWORD(lParam), HIWORD(lParam));
		}
		handled = true;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		handled = true;
		break;

	case WM_SYSKEYDOWN:
	{
		const auto keyState = static_cast<uintptr_t>(lParam);
		const bool altIsDown = (keyState & (uintptr_t{ 1 } << 29)) != 0;
		const bool wasPreviouslyDown = (keyState & (uintptr_t{ 1 } << 30)) != 0;

		if (m_allowFullscreenToggle && wParam == VK_RETURN && altIsDown)
		{
			if (!wasPreviouslyDown)
			{
				RequestToggleFullscreen();
			}
			handled = true;
		}
		break;
	}
	}

	if (m_messageCallback)
	{
		LRESULT callbackResult = 0;
		if (m_messageCallback(m_hwnd, message, wParam, lParam, callbackResult))
		{
			return callbackResult;
		}
	}

	if (handled)
	{
		return result;
	}

	return DefWindowProcW(m_hwnd, message, wParam, lParam);
}

void Window::RequestResize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	m_requestedSize = { width, height };
	m_hasResizeRequest = true;
}

bool Window::EnterBorderlessFullscreen()
{
	if (!m_hwnd)
	{
		DBG("Window::EnterBorderlessFullscreen: Native window is not initialized.");
		return false;
	}

	WINDOWPLACEMENT placement{ sizeof(WINDOWPLACEMENT) };
	if (!GetWindowPlacement(m_hwnd, &placement))
	{
		DBG("Window::EnterBorderlessFullscreen: Failed to get window placement. Error: %lu", GetLastError());
		return false;
	}

	LONG_PTR currentStyle = 0;
	if (!TryGetWindowStyle(currentStyle))
	{
		return false;
	}

	const HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo{ sizeof(MONITORINFO) };

	if (!monitor || !GetMonitorInfo(monitor, &monitorInfo))
	{
		DBG("Window::EnterBorderlessFullscreen: Failed to get monitor information. Error: %lu", GetLastError());
		return false;
	}

	m_windowedStyle = currentStyle;
	m_windowedPlacement = placement;
	m_hasWindowedPlacement = true;

	const LONG_PTR fullscreenStyle =
		(currentStyle & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) |
		static_cast<LONG_PTR>(WS_POPUP);

	SetLastError(ERROR_SUCCESS);
	const LONG_PTR previousStyle = SetWindowLongPtrW(m_hwnd, GWL_STYLE, fullscreenStyle);
	if (previousStyle == 0 && GetLastError() != ERROR_SUCCESS)
	{
		DBG("Window::EnterBorderlessFullscreen: Failed to set window style. Error: %lu", GetLastError());
		m_hasWindowedPlacement = false;
		return false;
	}

	const RECT& monitorRect = monitorInfo.rcMonitor;
	if (!SetWindowPos(
		m_hwnd,
		HWND_TOP,
		monitorRect.left,
		monitorRect.top,
		monitorRect.right - monitorRect.left,
		monitorRect.bottom - monitorRect.top,
		SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
	{
		DBG("Window::EnterBorderlessFullscreen: Failed to resize window. Error: %lu", GetLastError());
		SetWindowLongPtrW(m_hwnd, GWL_STYLE, m_windowedStyle);
		SetWindowPlacement(m_hwnd, &m_windowedPlacement);
		RefreshWindowFrame();
		m_hasWindowedPlacement = false;
		return false;
	}

	return true;
}

bool Window::ExitBorderlessFullscreen()
{
	if (!m_hwnd || !m_hasWindowedPlacement)
	{
		DBG("Window::ExitBorderlessFullscreen: Windowed placement is not available.");
		return false;
	}

	LONG_PTR fullscreenStyle = 0;
	if (!TryGetWindowStyle(fullscreenStyle))
	{
		return false;
	}

	SetLastError(ERROR_SUCCESS);
	const LONG_PTR previousStyle = SetWindowLongPtrW(m_hwnd, GWL_STYLE, m_windowedStyle);
	if (previousStyle == 0 && GetLastError() != ERROR_SUCCESS)
	{
		DBG("Window::ExitBorderlessFullscreen: Failed to restore window style. Error: %lu", GetLastError());
		return false;
	}

	if (!SetWindowPlacement(m_hwnd, &m_windowedPlacement) || !RefreshWindowFrame())
	{
		DBG("Window::ExitBorderlessFullscreen: Failed to restore window placement. Error: %lu", GetLastError());
		SetWindowLongPtrW(m_hwnd, GWL_STYLE, fullscreenStyle);
		RefreshWindowFrame();
		return false;
	}

	m_hasWindowedPlacement = false;
	return true;
}

bool Window::TryGetWindowStyle(LONG_PTR& outStyle) const
{
	SetLastError(ERROR_SUCCESS);
	outStyle = GetWindowLongPtrW(m_hwnd, GWL_STYLE);

	if (outStyle == 0 && GetLastError() != ERROR_SUCCESS)
	{
		DBG("Window::TryGetWindowStyle: Failed to get window style. Error: %lu", GetLastError());
		return false;
	}

	return true;
}

bool Window::RefreshWindowFrame() const
{
	return SetWindowPos(
		m_hwnd,
		nullptr,
		0,
		0,
		0,
		0,
		SWP_NOMOVE |
		SWP_NOSIZE |
		SWP_NOZORDER |
		SWP_NOOWNERZORDER |
		SWP_FRAMECHANGED) != FALSE;
}
