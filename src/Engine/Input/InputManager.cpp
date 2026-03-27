#include "InputManager.h"
#include "Keyboard.h"

//初期化
void InputManager::Initialize()
{
	Keyboard_Initialize(); //キーボード初期化
	m_controller.Initialize();	//追加コントローラー初期化
}

//更新
void InputManager::Update()
{
	UpdateTriggerKeyInfo();	// Update Trigger Key Info
	UpdateDownKeyInfo();	// Update Down Key Info
	UpdateUpKeyInfo();		// Update Up Key Info

	m_controller.Update(m_inputInfo.controller);	// Update Controller Info

}

//キー情報コピー
void InputManager::Copy()
{
	keycopy(); //キーボードキー情報コピー
	m_controller.CopyState();	//コントローラー状態子コピー
}

//入力情報構造体取得
InputInfo* InputManager::GetInputInfo()
{
	return &m_inputInfo;
}

//コントローラー振動セット
void InputManager::SetControllerVibration(int index, float leftMotor, float rightMotor)
{
	m_controller.SetVibration(index, leftMotor, rightMotor);
}

//全コントローラー振動セット
void InputManager::SetAllControllerVibrations(float leftMotor, float rightMotor)
{
	m_controller.SetAllVibrations(leftMotor, rightMotor);
}

//コントローラー振動停止
void InputManager::StopControllerVibration(int index)
{
	m_controller.StopVibration(index);
}

//全コントローラー振動停止
void InputManager::StopAllControllerVibrations()
{
	m_controller.StopAllVibrations();
}


//トリガー情報更新
void InputManager::UpdateTriggerKeyInfo()
{
	m_inputInfo.key.a.trigger = Keyboard_IsKeyDownTrigger(KK_A);
	m_inputInfo.key.s.trigger = Keyboard_IsKeyDownTrigger(KK_S);
	m_inputInfo.key.d.trigger = Keyboard_IsKeyDownTrigger(KK_D);
	m_inputInfo.key.w.trigger = Keyboard_IsKeyDownTrigger(KK_W);
	m_inputInfo.key.t.trigger = Keyboard_IsKeyDownTrigger(KK_T);
	m_inputInfo.key.f.trigger = Keyboard_IsKeyDownTrigger(KK_F);
	m_inputInfo.key.g.trigger = Keyboard_IsKeyDownTrigger(KK_G);
	m_inputInfo.key.h.trigger = Keyboard_IsKeyDownTrigger(KK_H);
	m_inputInfo.key.i.trigger = Keyboard_IsKeyDownTrigger(KK_I);
	m_inputInfo.key.j.trigger = Keyboard_IsKeyDownTrigger(KK_J);
	m_inputInfo.key.k.trigger = Keyboard_IsKeyDownTrigger(KK_K);
	m_inputInfo.key.l.trigger = Keyboard_IsKeyDownTrigger(KK_L);
	m_inputInfo.key.p.trigger = Keyboard_IsKeyDownTrigger(KK_P);
	m_inputInfo.key.z.trigger = Keyboard_IsKeyDownTrigger(KK_Z);
	m_inputInfo.key.c.trigger = Keyboard_IsKeyDownTrigger(KK_C);
	m_inputInfo.key.n.trigger = Keyboard_IsKeyDownTrigger(KK_N);
	m_inputInfo.key.q.trigger = Keyboard_IsKeyDownTrigger(KK_Q);
	m_inputInfo.key.e.trigger = Keyboard_IsKeyDownTrigger(KK_E);
	m_inputInfo.key.up.trigger = Keyboard_IsKeyDownTrigger(KK_UP);
	m_inputInfo.key.down.trigger = Keyboard_IsKeyDownTrigger(KK_DOWN);
	m_inputInfo.key.left.trigger = Keyboard_IsKeyDownTrigger(KK_LEFT);
	m_inputInfo.key.right.trigger = Keyboard_IsKeyDownTrigger(KK_RIGHT);
	m_inputInfo.key.space.trigger = Keyboard_IsKeyDownTrigger(KK_SPACE);
	m_inputInfo.key.enter.trigger = Keyboard_IsKeyDownTrigger(KK_ENTER);
	m_inputInfo.key.rightCtrl.trigger = Keyboard_IsKeyDownTrigger(KK_RIGHTCONTROL);
	m_inputInfo.key.one.trigger = Keyboard_IsKeyDownTrigger(KK_D1);
	m_inputInfo.key.two.trigger = Keyboard_IsKeyDownTrigger(KK_D2);

	m_inputInfo.key.any.trigger =
		m_inputInfo.key.a.trigger ||
		m_inputInfo.key.s.trigger ||
		m_inputInfo.key.d.trigger ||
		m_inputInfo.key.w.trigger ||
		m_inputInfo.key.t.trigger ||
		m_inputInfo.key.f.trigger ||
		m_inputInfo.key.g.trigger ||
		m_inputInfo.key.h.trigger ||
		m_inputInfo.key.i.trigger ||
		m_inputInfo.key.j.trigger ||
		m_inputInfo.key.k.trigger ||
		m_inputInfo.key.l.trigger ||
		m_inputInfo.key.p.trigger ||
		m_inputInfo.key.z.trigger ||
		m_inputInfo.key.c.trigger ||
		m_inputInfo.key.n.trigger ||
		m_inputInfo.key.q.trigger ||
		m_inputInfo.key.e.trigger ||
		m_inputInfo.key.up.trigger ||
		m_inputInfo.key.down.trigger ||
		m_inputInfo.key.left.trigger ||
		m_inputInfo.key.right.trigger ||
		m_inputInfo.key.space.trigger ||
		m_inputInfo.key.enter.trigger ||
		m_inputInfo.key.rightCtrl.trigger;
}

//押下情報更新
void InputManager::UpdateDownKeyInfo()
{
	m_inputInfo.key.w.down = Keyboard_IsKeyDown(KK_W);
	m_inputInfo.key.a.down = Keyboard_IsKeyDown(KK_A);
	m_inputInfo.key.s.down = Keyboard_IsKeyDown(KK_S);
	m_inputInfo.key.d.down = Keyboard_IsKeyDown(KK_D);
	m_inputInfo.key.t.down = Keyboard_IsKeyDown(KK_T);
	m_inputInfo.key.f.down = Keyboard_IsKeyDown(KK_F);
	m_inputInfo.key.g.down = Keyboard_IsKeyDown(KK_G);
	m_inputInfo.key.h.down = Keyboard_IsKeyDown(KK_H);
	m_inputInfo.key.i.down = Keyboard_IsKeyDown(KK_I);
	m_inputInfo.key.j.down = Keyboard_IsKeyDown(KK_J);
	m_inputInfo.key.k.down = Keyboard_IsKeyDown(KK_K);
	m_inputInfo.key.l.down = Keyboard_IsKeyDown(KK_L);
	m_inputInfo.key.p.down = Keyboard_IsKeyDown(KK_P);
	m_inputInfo.key.z.down = Keyboard_IsKeyDown(KK_Z);
	m_inputInfo.key.c.down = Keyboard_IsKeyDown(KK_C);
	m_inputInfo.key.n.down = Keyboard_IsKeyDown(KK_N);
	m_inputInfo.key.q.down = Keyboard_IsKeyDown(KK_Q);
	m_inputInfo.key.e.down = Keyboard_IsKeyDown(KK_E);
	m_inputInfo.key.up.down = Keyboard_IsKeyDown(KK_UP);
	m_inputInfo.key.down.down = Keyboard_IsKeyDown(KK_DOWN);
	m_inputInfo.key.left.down = Keyboard_IsKeyDown(KK_LEFT);
	m_inputInfo.key.right.down = Keyboard_IsKeyDown(KK_RIGHT);
	m_inputInfo.key.space.down = Keyboard_IsKeyDown(KK_SPACE);
	m_inputInfo.key.enter.down = Keyboard_IsKeyDown(KK_ENTER);
	m_inputInfo.key.rightCtrl.down = Keyboard_IsKeyDown(KK_RIGHTCONTROL);
	m_inputInfo.key.one.trigger = Keyboard_IsKeyDown(KK_D1);
	m_inputInfo.key.two.trigger = Keyboard_IsKeyDown(KK_D2);

	m_inputInfo.key.any.down =
		m_inputInfo.key.a.down ||
		m_inputInfo.key.s.down ||
		m_inputInfo.key.d.down ||
		m_inputInfo.key.w.down ||
		m_inputInfo.key.t.down ||
		m_inputInfo.key.f.down ||
		m_inputInfo.key.g.down ||
		m_inputInfo.key.h.down ||
		m_inputInfo.key.i.down ||
		m_inputInfo.key.j.down ||
		m_inputInfo.key.k.down ||
		m_inputInfo.key.l.down ||
		m_inputInfo.key.p.down ||
		m_inputInfo.key.z.down ||
		m_inputInfo.key.c.down ||
		m_inputInfo.key.n.down ||
		m_inputInfo.key.q.down ||
		m_inputInfo.key.e.down ||
		m_inputInfo.key.up.down ||
		m_inputInfo.key.down.down ||
		m_inputInfo.key.left.down ||
		m_inputInfo.key.right.down ||
		m_inputInfo.key.space.down ||
		m_inputInfo.key.enter.down ||
		m_inputInfo.key.rightCtrl.down;
}

//離上情報更新
void InputManager::UpdateUpKeyInfo()
{
	m_inputInfo.key.w.up = !Keyboard_IsKeyUp(KK_W);
	m_inputInfo.key.a.up = !Keyboard_IsKeyUp(KK_A);
	m_inputInfo.key.s.up = !Keyboard_IsKeyUp(KK_S);
	m_inputInfo.key.d.up = !Keyboard_IsKeyUp(KK_D);
	m_inputInfo.key.t.up = Keyboard_IsKeyUp(KK_T);
	m_inputInfo.key.f.up = Keyboard_IsKeyUp(KK_F);
	m_inputInfo.key.g.up = Keyboard_IsKeyUp(KK_G);
	m_inputInfo.key.h.up = Keyboard_IsKeyUp(KK_H);
	m_inputInfo.key.i.up = Keyboard_IsKeyUp(KK_I);
	m_inputInfo.key.j.up = Keyboard_IsKeyUp(KK_J);
	m_inputInfo.key.k.up = Keyboard_IsKeyUp(KK_K);
	m_inputInfo.key.l.up = Keyboard_IsKeyUp(KK_L);
	m_inputInfo.key.p.up = !Keyboard_IsKeyUp(KK_P);
	m_inputInfo.key.z.up = Keyboard_IsKeyUp(KK_Z);
	m_inputInfo.key.c.up = Keyboard_IsKeyUp(KK_C);
	m_inputInfo.key.n.up = Keyboard_IsKeyUp(KK_N);
	m_inputInfo.key.q.up = Keyboard_IsKeyUp(KK_Q);
	m_inputInfo.key.e.up = Keyboard_IsKeyUp(KK_E);
	m_inputInfo.key.up.up = !Keyboard_IsKeyUp(KK_UP);
	m_inputInfo.key.down.up = !Keyboard_IsKeyUp(KK_DOWN);
	m_inputInfo.key.left.up = !Keyboard_IsKeyUp(KK_LEFT);
	m_inputInfo.key.right.up = !Keyboard_IsKeyUp(KK_RIGHT);
	m_inputInfo.key.space.up = !Keyboard_IsKeyUp(KK_SPACE);
	m_inputInfo.key.enter.up = !Keyboard_IsKeyUp(KK_ENTER);
	m_inputInfo.key.rightCtrl.up = Keyboard_IsKeyUp(KK_RIGHTCONTROL);
	m_inputInfo.key.one.trigger = Keyboard_IsKeyDownTrigger(KK_D1);
	m_inputInfo.key.two.trigger = Keyboard_IsKeyDownTrigger(KK_D2);

	m_inputInfo.key.any.up =
		m_inputInfo.key.a.up ||
		m_inputInfo.key.s.up ||
		m_inputInfo.key.d.up ||
		m_inputInfo.key.w.up ||
		m_inputInfo.key.t.up ||
		m_inputInfo.key.f.up ||
		m_inputInfo.key.g.up ||
		m_inputInfo.key.h.up ||
		m_inputInfo.key.i.up ||
		m_inputInfo.key.j.up ||
		m_inputInfo.key.k.up ||
		m_inputInfo.key.l.up ||
		m_inputInfo.key.p.up ||
		m_inputInfo.key.z.up ||
		m_inputInfo.key.c.up ||
		m_inputInfo.key.n.up ||
		m_inputInfo.key.q.up ||
		m_inputInfo.key.e.up ||
		m_inputInfo.key.up.up ||
		m_inputInfo.key.down.up ||
		m_inputInfo.key.left.up ||
		m_inputInfo.key.right.up ||
		m_inputInfo.key.space.up ||
		m_inputInfo.key.enter.up ||
		m_inputInfo.key.rightCtrl.up;
}