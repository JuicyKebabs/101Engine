#include "Test.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Input/InputManager.h"

void Test::Start() 
{
	DBG("Test::Start()");
}

void Test::PreUpdate() {}

void Test::Update() 
{
	if(InputManager::GetInstance().GetInputInfo().key.rightCtrl.trigger)
	{
		ChangeScene("test");
	}

}

void Test::LateUpdate() {}

void Test::Destroy() {}
