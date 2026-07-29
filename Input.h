#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <cstdint>

class Input {
public:
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	void Update();
	bool IsKeyPressed(uint8_t keyNumber) const;
	bool IsKeyReleased(uint8_t keyNumber) const;
	bool IsKeyTriggered(uint8_t keyNumber) const;
	bool IsKeyReleasedTriggered(uint8_t keyNumber) const;
	const DIMOUSESTATE2& GetMouseState() const { return mouseState_; }

private:
	Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;
	BYTE key_[256]{};
	BYTE keyPre_[256]{};
	DIMOUSESTATE2 mouseState_{};
};
