#include "WinApp.h"
#include <cassert>
#ifdef USE_IMGUI
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif

void WinApp::Initialize() {
	const wchar_t* title = L"GE3";

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr));

	instance_ = GetModuleHandle(nullptr);

	windowClass_.lpfnWndProc = WinApp::WindowProc;
	windowClass_.lpszClassName = kWindowClassName;
	windowClass_.hInstance = instance_;
	windowClass_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	const ATOM registeredClass = RegisterClass(&windowClass_);
	assert(registeredClass != 0);

	RECT windowRect{ 0, 0, kClientWidth, kClientHeight };
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

void WinApp::Update() {

}

LRESULT CALLBACK WinApp::WindowProc(
	HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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