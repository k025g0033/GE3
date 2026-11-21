#include "D3DResourceLeakChecker.h"
#ifdef _DEBUG
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>
#pragma comment(lib, "dxguid.lib")
#endif

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_DXGI, DXGI_DEBUG_RLO_ALL);
    }
#endif
}
