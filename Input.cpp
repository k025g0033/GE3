#include "Input.h"

#include <cassert>
#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
	// DirectInputの初期化
	HRESULT hr = DirectInput8Create(
		hInstance,
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
		hwnd,
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	hr = directInput_->CreateDevice(
		GUID_SysMouse,
		mouse_.GetAddressOf(),
		nullptr);
	assert(SUCCEEDED(hr));

	hr = mouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(hr));

	hr = mouse_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
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
	HRESULT mouseResult = mouse_->Acquire();
	if (SUCCEEDED(mouseResult)) {
		mouseResult = mouse_->GetDeviceState(
			sizeof(DIMOUSESTATE2),
			&mouseState_);
	}
	if (FAILED(mouseResult)) {
		mouseState_ = {};
	}
}

bool Input::IsKeyPressed(uint8_t keyNumber) const {
	return key_[keyNumber] != 0;
}

bool Input::IsKeyReleased(uint8_t keyNumber) const {
	return key_[keyNumber] == 0;
}

bool Input::IsKeyTriggered(uint8_t keyNumber) const {
	return key_[keyNumber] != 0 && keyPre_[keyNumber] == 0;
}

bool Input::IsKeyReleasedTriggered(uint8_t keyNumber) const {
	return key_[keyNumber] == 0 && keyPre_[keyNumber] != 0;
}
