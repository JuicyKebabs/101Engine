#pragma once
#include "Engine/Actor/Actor.h"

class InspectorPanel
{
public:
    void Render(Actor* selectedActor);

private:
    char m_componentNameBuffer[128] = "";
};
