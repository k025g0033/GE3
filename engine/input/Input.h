#pragma once

#include "WinApp.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>
#include <wrl.h>

// 入力
class Input
{
public: // メンバ関数

	// このクラス内ではComPtrと省略できる
	template <class T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 初期化
	void Initialize(WinApp* winApp);
	// 更新
	void Update();

	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);

private: // メンバ変数
	ComPtr<IDirectInput8> directInput_;
	ComPtr<IDirectInputDevice8> keyboard_;

	BYTE key_[256]{};     // 今フレームのキー状態
	BYTE keyPre_[256]{};  // 前フレームのキー状態

	// WindowsAPI
	WinApp* winApp = nullptr;
};

