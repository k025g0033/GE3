#pragma once
#include <Windows.h>

// WindowsAPI
class WinApp
{
public: // 静的メンバ関数
	static LRESULT CALLBACK WindowProc(
		HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
public: // メンバ関数
	// 初期化
	void Initialize();
	// 更新
	void Update();

	// 作成したウィンドウのハンドルを取得
	HWND GetHwnd() const { return hwnd_; }
private:

	static constexpr const wchar_t* kWindowClassName =
		L"CG2WindowClass";

	HINSTANCE instance_ = nullptr;
	HWND hwnd_ = nullptr;
};

