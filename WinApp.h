#pragma once
#include <Windows.h>
#include <cstdint>

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
	// 終了
	void Finalize();

	// 作成したウィンドウのハンドルを取得
	HWND GetHwnd() const { return hwnd_; }
	// アプリのハンドルを取得
	HINSTANCE GetHInstance() const {
		return windowClass_.hInstance;
	}
public: // 定数
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:

	// ウィンドウクラスの設定
	WNDCLASS windowClass_{};

	static constexpr const wchar_t* kWindowClassName =
		L"CG2WindowClass";

	HINSTANCE instance_ = nullptr;
	HWND hwnd_ = nullptr;
};

