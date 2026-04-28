#pragma once
#include <memory>
#include "Controller.h"

//キー入力状態構造体
struct InputState
{
	bool trigger = false;
	bool down = false;
	bool up = false;
};

struct KeyInputInfo
{
	InputState w;
	InputState a;
	InputState s;
	InputState d;
	InputState t;
	InputState f;
	InputState g;
	InputState h;
	InputState i;
	InputState j;
	InputState k;
	InputState l;
	InputState p;
	InputState z;
	InputState c;
	InputState n;
	InputState q;
	InputState e;
	InputState up;
	InputState down;
	InputState left;
	InputState right;
	InputState space;
	InputState enter;
	InputState rightCtrl;
	InputState one;
	InputState two;

	InputState any;
};

struct ControllerInputInfo
{
	InputState A;			//Aボタンor〇ボタン
	InputState B;			//Bボタンor×ボタン
	InputState X;			//Xボタンor△ボタン
	InputState Y;			//Yボタンor□ボタン
	InputState START;		//STARTボタン
	InputState BACK;		//BACKボタン
	InputState LSHOULDER;	//左肩ボタン
	InputState RSHOULDER;	//右肩ボタン
	InputState LTHUMB;		//左スティックの押下
	InputState RTHUMB;		//右スティックの押下
	InputState UP;			//上ボタン
	InputState DOWN;		//下ボタン
	InputState LEFT;		//左ボタン
	InputState RIGHT;		//右ボタン
	InputState anyButton;	//任意のボタン

	DirectX::XMFLOAT2 leftStick;	//Left stick(normalized)
	DirectX::XMFLOAT2 rightStick;	//Right stick(normalized)
	DirectX::XMFLOAT2 leftStickPast;	//Left stick(normalized)
	DirectX::XMFLOAT2 rightStickPast;	//Right stick(normalized)
};

//入力情報構造体
struct InputInfo
{
	KeyInputInfo key;
	ControllerInputInfo controller[CONTROLLERS_MAX];
};

//入力管理クラス
class InputManager
{
public:
	void Initialize();	//初期化
	void Update();		//更新
	void Copy();		//キー情報コピー

	//シングルトンパターン
	static InputManager& GetInstance(){
		if(!m_instance){
			m_instance = std::unique_ptr<InputManager>(new InputManager());
		}
		return *m_instance;
	}

	//ゲッター
	const InputInfo& GetInputInfo() const;	//入力情報構造体取得

	void SetControllerVibration(int index, float leftMotor, float rightMotor); //コントローラー振動セット
	void SetAllControllerVibrations(float leftMotor, float rightMotor); //全コントローラー振動セット
	void StopControllerVibration(int index); //コントローラー振動停止
	void StopAllControllerVibrations(); //全コントローラー振動停止

private:
	static inline std::unique_ptr<InputManager> m_instance;	//シングルトンインスタンス
	InputInfo m_inputInfo{};	//入力情報構造体
	Controller m_controller;	//コントローラー管理クラス

private:
	InputManager() = default;	//コンストラクタ
	void UpdateTriggerKeyInfo();	//トリガー情報更新
	void UpdateDownKeyInfo();		//押下情報更新
	void UpdateUpKeyInfo();			//離上情報更新
};