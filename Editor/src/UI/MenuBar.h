#pragma once
#include <functional>
#include <string>
//--------------------------------------------------------------------------------
// MenuBar class
// This class encapsulates the rendering and logic of the editor's main menu bar.
// Menu bar is consisted of File, Assets, and Build menus.
//--------------------------------------------------------------------------------

class MenuBar
{
public:
    struct Callbacks
    {
		std::function<void()> onSaveDocument;

        std::function<void()> onUndo;
        std::function<void()> onRedo;
        std::function<void()> onResetLayout;

        std::function<void()> onBuildGame;
        std::function<void(bool)> onReloadGameCode;
		std::function<void(const std::string&, bool isBehavior)> onCreateScript;
		std::function<void()> onCreateActorImprint;

        bool canUndo = false;
        bool canRedo = false;
		bool canSave = false;
        bool canModifyScripts = true;
		bool canModifyActorImprints = true;
        bool canBuild = true;
    };

	static bool DispatchCreateActorImprint(const Callbacks& callbacks)
	{
		if (!callbacks.canModifyActorImprints || !callbacks.onCreateActorImprint) return false;
		callbacks.onCreateActorImprint();
		return true;
	}
	static bool DispatchSaveShortcut(const Callbacks& callbacks,
		bool controlDown, bool savePressed, bool textInputActive)
	{
		if (!controlDown || !savePressed || textInputActive ||
			!callbacks.canSave || !callbacks.onSaveDocument) return false;
		callbacks.onSaveDocument();
		return true;
	}

    void Render(const Callbacks& callbacks);

private:
    char m_newScriptNameBuffer[128] = "";
    bool m_showCreateScriptPopup = false;
    bool m_createAsBehavior = true;
};
