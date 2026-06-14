#pragma once
#include <functional>
#include <string>

class MenuBar
{
public:
    struct Callbacks
    {
        std::function<void()> onNewScene;
        std::function<void()> onOpenScene;
        std::function<void()> onSaveScene;
        std::function<void()> onBuildGame;
        std::function<void(const std::string&)> onCreateBehavior;
    };

    void Render(const Callbacks& callbacks);

private:
    char m_newBehaviorNameBuffer[128] = "";
    bool m_showCreateBehaviorPopup = false;
};
