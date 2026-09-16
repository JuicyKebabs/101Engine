#include <cstdlib>
#include <iostream>

#include <windows.h>

#include "Engine/Input/InputManager.h"
#include "Engine/Input/Mouse.h"

namespace
{
	int failures = 0;

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAILED: " << message << '\n';
		++failures;
	}

	LPARAM MousePosition(int x, int y)
	{
		return MAKELPARAM(static_cast<short>(x), static_cast<short>(y));
	}
}

int main()
{
	Mouse mouse;
	MouseInputInfo input{};
	mouse.Initialize();

	mouse.ProcessMessage(WM_MOUSEMOVE, 0, MousePosition(120, 80));
	mouse.Update(input);
	Check(input.clientPositionPixels.x == 120 && input.clientPositionPixels.y == 80,
		"mouse position uses client coordinates");
	Check(input.deltaPixels.x == 0 && input.deltaPixels.y == 0,
		"first mouse position has zero delta");

	mouse.ProcessMessage(WM_LBUTTONDOWN, 0, MousePosition(120, 80));
	mouse.Update(input);
	Check(input.left.trigger && input.left.down && !input.left.up,
		"left button reports its press edge");
	Check(input.anyButton.trigger && input.anyButton.down,
		"any button includes the left button");
	mouse.CopyState();

	mouse.ProcessMessage(WM_MOUSEMOVE, MK_LBUTTON, MousePosition(-5, 110));
	mouse.Update(input);
	Check(input.left.down && !input.left.trigger && !input.left.up,
		"left button remains down without another press edge");
	Check(input.clientPositionPixels.x == -5 && input.clientPositionPixels.y == 110,
		"mouse position preserves signed coordinates");
	Check(input.deltaPixels.x == -125 && input.deltaPixels.y == 30,
		"mouse delta compares current and previous frame positions");
	Check(input.lookDelta.x == -125 && input.lookDelta.y == 30,
		"editor look input keeps using cursor movement");
	mouse.SetRawLookEnabled(true);
	mouse.Update(input);
	Check(input.lookDelta.x == 0 && input.lookDelta.y == 0,
		"raw look input does not reuse cursor movement");
	mouse.SetRawLookEnabled(false);
	mouse.CopyState();

	mouse.ProcessMessage(WM_LBUTTONUP, 0, MousePosition(-5, 110));
	mouse.Update(input);
	Check(!input.left.trigger && !input.left.down && input.left.up,
		"left button reports its release edge");
	Check(input.anyButton.up, "any button includes the left release edge");
	mouse.CopyState();
	mouse.ProcessMessage(WM_RBUTTONDOWN, 0, MousePosition(-5, 110));
	mouse.Update(input);
	Check(input.right.trigger && input.right.down && !input.right.up,
		"right button reports its press edge");
	mouse.CopyState();
	mouse.ProcessMessage(WM_RBUTTONUP, 0, MousePosition(-5, 110));
	mouse.Update(input);
	Check(!input.right.trigger && !input.right.down && input.right.up,
		"right button reports its release edge");
	mouse.CopyState();

	mouse.ProcessMessage(WM_MBUTTONDOWN, 0, MousePosition(-5, 110));
	mouse.Update(input);
	Check(input.middle.trigger && input.middle.down && !input.middle.up,
		"middle button reports its press edge");
	mouse.CopyState();
	mouse.ProcessMessage(WM_MBUTTONUP, 0, MousePosition(-5, 110));
	mouse.Update(input);
	Check(!input.middle.trigger && !input.middle.down && input.middle.up,
		"middle button reports its release edge");
	mouse.CopyState();

	mouse.ProcessMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
	mouse.ProcessMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA / 2), 0);
	mouse.Update(input);
	Check(input.wheelDelta == 1.5f, "mouse wheel events accumulate within a frame");
	mouse.CopyState();
	mouse.Update(input);
	Check(input.wheelDelta == 0.0f, "mouse wheel delta clears at the frame boundary");
	Check(input.deltaPixels.x == 0 && input.deltaPixels.y == 0,
		"mouse delta is zero without movement");

	mouse.ProcessMessage(WM_RBUTTONDOWN, 0, MousePosition(20, 30));
	mouse.ProcessMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), 0);
	mouse.ProcessMessage(WM_ACTIVATEAPP, FALSE, 0);
	mouse.Update(input);
	Check(!input.right.trigger && !input.right.down && !input.right.up,
		"deactivation clears button state without a release edge");
	Check(input.wheelDelta == 0.0f, "deactivation clears accumulated wheel input");
	Check(input.deltaPixels.x == 0 && input.deltaPixels.y == 0,
		"deactivation clears position history");

	if (failures == 0)
	{
		std::cout << "MouseInputTests passed\n";
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
