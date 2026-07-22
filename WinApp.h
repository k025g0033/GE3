#pragma once

#include <Windows.h>
#include <cstdint>

// Windows固有のウィンドウ生成とメッセージ処理を担当するクラス。
class WinApp {
public:
	WinApp() = default;
	~WinApp();

	WinApp(const WinApp&) = delete;
	WinApp& operator=(const WinApp&) = delete;

	void Initialize(const wchar_t* title, int32_t clientWidth, int32_t clientHeight);
	bool ProcessMessage();

	HWND GetHwnd() const { return hwnd_; }

private:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	static constexpr const wchar_t* kWindowClassName = L"CG2WindowClass";
	HINSTANCE instance_ = nullptr;
	HWND hwnd_ = nullptr;
};
