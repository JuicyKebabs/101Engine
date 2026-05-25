#pragma once
#include <cstdint>

class WindowInfo
{
public:
	// Prohibit copying and assignment
	WindowInfo(const WindowInfo&) = delete;
	void operator=(const WindowInfo&) = delete;

	static WindowInfo& GetInstance() {
		static WindowInfo instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	void SetWindowSize(uint32_t width, uint32_t height) {
		this->width = width;
		this->height = height;
	}

	uint32_t GetWidth() const { return width; }
	uint32_t GetHeight() const { return height; }
	float GetAspectRatio() const { return (height != 0 && width != 0) ? static_cast<float>(width) / height : 1.0f; }

private:
	uint32_t width = 1920;	// Window width
	uint32_t height = 1080;	// Window height

private:
	WindowInfo() = default;	// Constructor
};