#include "app/platform/GraphicsWarmup.h"

#include <QString>
#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <iterator>
#include <thread>
#pragma comment(lib, "d3d11.lib")
#endif

namespace loupe::app::platform {

#ifdef Q_OS_WIN
namespace {

std::thread& warmupThread()
{
    static std::thread thread;
    return thread;
}

ID3D11Device*& warmDevice()
{
    static ID3D11Device* device = nullptr;
    return device;
}

// Another backend or adapter chosen through Qt's own variables: the warm-up would prepare
// the wrong one, so it is not done.
bool usesDefaultDirect3D11()
{
    const auto backend = qEnvironmentVariable("QSG_RHI_BACKEND");
    if (!backend.isEmpty() && backend.compare(QStringLiteral("d3d11"), Qt::CaseInsensitive) != 0) return false;
    return !qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX") && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")
        && !qEnvironmentVariableIsSet("QSG_RHI_PREFER_SOFTWARE_RENDERER");
}

} // namespace
#endif

void startGraphicsWarmup()
{
#ifdef Q_OS_WIN
    if (warmupThread().joinable() || !usesDefaultDirect3D11()) return;
    warmupThread() = std::thread([] {
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        ID3D11Device* device = nullptr;
        // As Qt asks for its device: the default hardware adapter, with BGRA support.
        if (SUCCEEDED(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                          levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &device,
                                          nullptr, nullptr))) {
            warmDevice() = device;
        }
    });
#endif
}

void finishGraphicsWarmup()
{
#ifdef Q_OS_WIN
    if (warmupThread().joinable()) warmupThread().join();
    if (auto*& device = warmDevice()) {
        device->Release();
        device = nullptr;
    }
#endif
}

} // namespace loupe::app::platform
