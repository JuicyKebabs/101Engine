#include "TestBehavior.h"

void TestBehavior::Start()
{
    DBG("TestBehavior::Start()");
}

void TestBehavior::Update()
{
    if (InputManager::GetInstance().GetInputInfo().key.a.trigger)
    {
		DBG("A key pressed, changing scene to 'output'");
        ChangeScene("output");
    }
}
