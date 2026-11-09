#include "Input.h"
#include <cassert>
#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

void Input::Initialize(WinApp* winApp) {
	// 借りてきたWinAppのインスタンスを記録
	this->winApp = winApp;

	// DirectInputの初期化
	HRESULT hr = DirectInput8Create(
		winApp->GetHInstance(),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(directInput_.GetAddressOf()),
		nullptr);
	assert(SUCCEEDED(hr));

	// キーボードデバイスの生成
	hr = directInput_->CreateDevice(
		GUID_SysKeyboard,
		keyboard_.GetAddressOf(),
		nullptr);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard_->SetCooperativeLevel(
		winApp->GetHwnd(),
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

}

void Input::Update() {

	// 前フレームのキー入力を保存
	std::memcpy(keyPre_, key_, sizeof(key_));

	// キーボード情報の取得開始
	HRESULT hr = keyboard_->Acquire();
	if (FAILED(hr)) {
		std::memset(key_, 0, sizeof(key_));
		return;
	}

	// 全キーの入力状態を取得
	hr = keyboard_->GetDeviceState(sizeof(key_), key_);

	if (FAILED(hr)) {
		std::memset(key_, 0, sizeof(key_));
	}

}

bool Input::PushKey(BYTE keyNumber) {
	// 指定キーを押していればtrueを返す
	if (key_[keyNumber]) {
		return true;
	}
	// そうでなければfalseを返す
	return false;
}

bool Input::TriggerKey(BYTE keyNumber) {
	return keyPre_[keyNumber] == 0
		&& key_[keyNumber] != 0;
}