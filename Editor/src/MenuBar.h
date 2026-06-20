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
        std::function<void()> onNewScene;
        std::function<void()> onOpenScene;
        std::function<void()> onSaveScene;
        std::function<void()> onBuildGame;
		std::function<void(bool)> onReloadGameCode;
        std::function<void(const std::string&, bool isBehavior)> onCreateScript;
    };

    void Render(const Callbacks& callbacks);

private:
    char m_newScriptNameBuffer[128] = "";
    bool m_showCreateScriptPopup = false;
    bool m_createAsBehavior = true;
};
