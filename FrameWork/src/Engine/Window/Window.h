#pragma once
#include <windows.h>
#include <cstdint>
#include <functional>
#include <string>

//--------------------------------------------------------------------------------------------------
// Window class
// This class manages a window in a Windows application.
// This class is responsible for creating, displaying, resizing and handling messages for a window.
//--------------------------------------------------------------------------------------------------

class Window
{
public:

	// Display mode of the window (Windowed or Borderless Fullscreen)
	enum class Mode
	{
		Windowed,
		BorderlessFullscreen
	};

	struct Size
	{
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// This is used to allow the application to handle window messages in a custom way.
	// (e.g., for handling ImGui input events on EditorApp, for end app by Escape key on GameApp)
	using MessageCallback = std::function<bool(
		HWND hwnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		LRESULT& outResult
		)>;

	struct InitDesc
	{
		HINSTANCE instance = nullptr;
		std::wstring className = L"101EngineWindow";
		std::wstring title = L"101Engine";
		uint32_t clientWidth = 1920;
		uint32_t clientHeight = 1080;
		bool resizable = true;
		bool allowFullscreenToggle = false;
		MessageCallback messageCallback;
	};

public:
	Window() = default;
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	bool Initialize(const InitDesc& desc);
	void Terminate();
	void Show(int command = SW_SHOW) const;

	HWND GetHandle() const { return m_hwnd; }
	Size GetClientSize() const { return m_clientSize; }
	uint32_t GetWidth() const { return m_clientSize.width; }
	uint32_t GetHeight() const { return m_clientSize.height; }
	float GetAspectRatio() const;
	bool IsMinimized() const { return m_isMinimized; }

	bool GetResizeRequest(Size& outSize) const;
	void CommitResize();
	void RequestMode(Mode mode);
	void RequestToggleFullscreen();
	bool ApplyPendingModeChange();
	Mode GetMode() const { return m_mode; }

private:
	HINSTANCE m_instance = nullptr;
	HWND m_hwnd = nullptr;
	std::wstring m_className;
	MessageCallback m_messageCallback;

	Size m_clientSize{};
	Size m_requestedSize{};
	bool m_hasResizeRequest = false;
	bool m_isMinimized = false;
	bool m_classRegistered = false;
	bool m_allowFullscreenToggle = false;
	
private:
	Mode m_mode = Mode::Windowed;
	Mode m_requestedMode = Mode::Windowed;
	bool m_hasModeChangeRequest = false;

	LONG_PTR m_windowedStyle = 0;
	WINDOWPLACEMENT m_windowedPlacement{ sizeof(WINDOWPLACEMENT) };
	bool m_hasWindowedPlacement = false;

private:
	static LRESULT CALLBACK StaticWindowProcedure(
		HWND hwnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam);

	LRESULT HandleMessage(
		UINT message,
		WPARAM wParam,
		LPARAM lParam);

	void RequestResize(uint32_t width, uint32_t height);
	bool EnterBorderlessFullscreen();
	bool ExitBorderlessFullscreen();
	bool TryGetWindowStyle(LONG_PTR& outStyle) const;
	bool RefreshWindowFrame() const;
};
