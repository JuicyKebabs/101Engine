#pragma once
#include <functional>

//----------------------------------------------------------------------
// Toolbar class
// Draws the editor toolbar and dispatches editor execution controls.
//----------------------------------------------------------------------
class Toolbar
{
public:
	struct Callbacks
	{
		std::function<void()> onPlay;
		std::function<void()> onStop;
		std::function<void(bool)> onShowCollidersChanged;

		bool canPlay = false;
		bool canStop = false;
		bool showColliders = false;
	};

	void Render(const Callbacks& callbacks);
};
