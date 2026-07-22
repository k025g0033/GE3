#include "WinApp.h"

#include <cassert>

#ifdef USE_IMGUI
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif

WinApp::~WinApp() {
	if (hwnd_ && IsWindow(hwnd_)) {
		DestroyWindow(hwnd_);
	}
	if (instance_) {
		UnregisterClass(kWindowClassName, instance_);
	}
}

void WinApp::Initialize(const wchar_t* title, int32_t clientWidth, int32_t clientHeight) {
	instance_ = GetModuleHandle(nullptr);

	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = kWindowClassName;
	wc.hInstance = instance_;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	const ATOM registeredClass = RegisterClass(&wc);
	assert(registeredClass != 0);

	RECT windowRect{0, 0, clientWidth, clientHeight};
	const BOOL adjusted = AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);
	assert(adjusted != FALSE);

	hwnd_ = CreateWindow(
		kWindowClassName,
		title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		instance_,
		nullptr);
	assert(hwnd_ != nullptr);

	ShowWindow(hwnd_, SW_SHOW);
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.message == WM_QUIT;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}
