#pragma once

#include <windows.h>
#include <DirectXMath.h>

struct MouseInputInfo;

class Mouse
{
public:
	void Initialize();
	void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void Update(MouseInputInfo& inputInfo) const;
	void CopyState();
	void SetRawLookEnabled(bool enabled);

private:
	struct State
	{
		bool left = false;
		bool right = false;
		bool middle = false;
		DirectX::XMINT2 clientPositionPixels{};
		bool hasPosition = false;
	};

	void UpdateClientPosition(LPARAM lParam);

	State m_current{};
	State m_previous{};
	float m_wheelAccumulator = 0.0f;
	DirectX::XMINT2 m_rawLookDelta{};
	bool m_rawLookEnabled = false;
};
