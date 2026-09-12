#include "Test.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Input/InputManager.h"

REGISTER_GAME_COMPONENT(Test)

std::optional<TypeMetadata> Test::BuildMetadata()
{
	TypeMetadataBuilder<Test> builder("Test");

	/*---- Describe the property registration process ----*/
	// Example: register a private member declared in Test.
	// builder.Property("speed", &Test::m_speed);

	return builder.Build();
}

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
