//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
//
//*********************************************************

#pragma once
#include "pch.h"
#include "DirectXPanelBase.h"
#include <DirectXMath.h>
#include <math.h>
#include <ppltasks.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include "DirectXHelper.h"

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input::Inking;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Interop;
using namespace Concurrency;
using namespace DirectX;
using namespace D2D1;
using namespace EquirectangularImageViewer;
using namespace DX;

static const float _dipsPerInch = 96.0f;

DirectXPanelBase::DirectXPanelBase() :
    _loadingComplete(false),
    _backgroundColor(D2D1::ColorF(D2D1::ColorF::White)), // Default to white background.
    _alphaMode(DXGI_ALPHA_MODE_UNSPECIFIED), // Default to ignore alpha, which can provide better performance if transparency is not required.
    _compositionScaleX(1.0f),
    _compositionScaleY(1.0f),
    _height(1.0f),
    _width(1.0f)
{
    this->SizeChanged += ref new Windows::UI::Xaml::SizeChangedEventHandler(this, &DirectXPanelBase::OnSizeChanged);
    this->CompositionScaleChanged += ref new Windows::Foundation::TypedEventHandler<SwapChainPanel^, Object^>(this, &DirectXPanelBase::OnCompositionScaleChanged);
    Application::Current->Suspending += ref new SuspendingEventHandler(this, &DirectXPanelBase::OnSuspending);
    Application::Current->Resuming += ref new EventHandler<Object^>(this, &DirectXPanelBase::OnResuming);
}

void DirectXPanelBase::CreateDeviceIndependentResources()
{
    D2D1_FACTORY_OPTIONS options;
    ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
    // Enable D2D debugging via SDK Layers when in debug mode.
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    ThrowIfFailed(
        D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory2),
        &options,
        &_d2dFactory
        )
        );
}

void DirectXPanelBase::CreateDeviceResources()
{
    // This flag adds support for surfaces with a different color channel ordering than the API default.
    // It is recommended usage, and is required for compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // This array defines the set of DirectX hardware feature levels this app will support.
    // Note the ordering should be preserved.
    // Don't forget to declare your application's minimum required feature level in its
    // description.  All applications are assumed to support 9.1 unless otherwise stated.
    D3D_FEATURE_LEVEL featureLevels [] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    // Create the DX11 API device object, and get a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ThrowIfFailed(
        D3D11CreateDevice(
			nullptr,                    // Specify null to use the default adapter.
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			creationFlags,              // Optionally set debug and Direct2D compatibility flags.
			featureLevels,              // List of feature levels this app can support.
			ARRAYSIZE(featureLevels),
			D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
			&device,                    // Returns the Direct3D device created.
			NULL,                       // Returns feature level of device created.
			&context)					// Returns the device immediate context.
        );

    // Get D3D11.1 device
    ThrowIfFailed(
        device.As(&_d3dDevice)
        );

    // Get D3D11.1 context
    ThrowIfFailed(
        context.As(&_d3dContext)
        );

    // Get underlying DXGI device of D3D device
    ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(
        _d3dDevice.As(&dxgiDevice)
        );

    // Get D2D device
    ThrowIfFailed(
        _d2dFactory->CreateDevice(dxgiDevice.Get(), &_d2dDevice)
        );

    // Get D2D context
    ThrowIfFailed(
        _d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &_d2dContext
        )
        );

    // Set D2D text anti-alias mode to Grayscale to ensure proper rendering of text on intermediate surfaces.
    _d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

void DirectXPanelBase::CreateSizeDependentResources()
{
    // Ensure dependent objects have been released.
    _d2dContext->SetTarget(nullptr);
    _d2dTargetBitmap = nullptr;
    _d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
    _d3dContext->Flush();

    // Set render target size to the rendered size of the panel including the composition scale, 
    // defaulting to the minimum of 1px if no size was specified.
    _renderTargetWidth = _width * _compositionScaleX;
    _renderTargetHeight = _height * _compositionScaleY;

    // If the swap chain already exists, then resize it.
    if (_swapChain != nullptr)
    {
        HRESULT hr = _swapChain->ResizeBuffers(
            2,
            static_cast<UINT>(_renderTargetWidth),
            static_cast<UINT>(_renderTargetHeight),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0
            );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();
            return;

        }
        else
        {
            ThrowIfFailed(hr);
        }
    }
    else // Otherwise, create a new one.
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
        swapChainDesc.Width = static_cast<UINT>(_renderTargetWidth);      // Match the size of the panel.
        swapChainDesc.Height = static_cast<UINT>(_renderTargetHeight);
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;                  // This is the most common swap chain format.
        swapChainDesc.Stereo = false;
        swapChainDesc.SampleDesc.Count = 1;                                 // Don't use multi-sampling.
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;                                      // Use double buffering to enable flip.
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;        // All Windows Store apps must use this SwapEffect.
        swapChainDesc.Flags = 0;
        swapChainDesc.AlphaMode = _alphaMode;

        // Get underlying DXGI Device from D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        ThrowIfFailed(
            _d3dDevice.As(&dxgiDevice)
            );

        // Get adapter.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        ThrowIfFailed(
            dxgiDevice->GetAdapter(&dxgiAdapter)
            );

        // Get factory.
        ComPtr<IDXGIFactory2> dxgiFactory;
        ThrowIfFailed(
            dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
            );

        ComPtr<IDXGISwapChain1> swapChain;
        // Create swap chain.
        ThrowIfFailed(
            dxgiFactory->CreateSwapChainForComposition(
            _d3dDevice.Get(),
            &swapChainDesc,
            nullptr,
            &swapChain
            )
            );
        swapChain.As(&_swapChain);

        // Ensure that DXGI does not queue more than one frame at a time. This both reduces 
        // latency and ensures that the application will only render after each VSync, minimizing 
        // power consumption.
        ThrowIfFailed(
            dxgiDevice->SetMaximumFrameLatency(1)
            );

        Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([=]()
        {
            //Get backing native interface for SwapChainPanel.
            ComPtr<ISwapChainPanelNative> panelNative;
            ThrowIfFailed(
                reinterpret_cast<IUnknown*>(this)->QueryInterface(IID_PPV_ARGS(&panelNative))
                );

            // Associate swap chain with SwapChainPanel.  This must be done on the UI thread.
            ThrowIfFailed(
                panelNative->SetSwapChain(_swapChain.Get())
                );
        }, CallbackContext::Any));
    }

    // Ensure the physical pixel size of the swap chain takes into account both the XAML SwapChainPanel's logical layout size and 
    // any cumulative composition scale applied due to zooming, render transforms, or the system's current scaling plateau.
    // For example, if a 100x100 SwapChainPanel has a cumulative 2x scale transform applied, we instead create a 200x200 swap chain 
    // to avoid artifacts from scaling it up by 2x, then apply an inverse 1/2x transform to the swap chain to cancel out the 2x transform.
    DXGI_MATRIX_3X2_F inverseScale = { 0 };
    inverseScale._11 = 1.0f / _compositionScaleX;
    inverseScale._22 = 1.0f / _compositionScaleY;

    _swapChain->SetMatrixTransform(&inverseScale);

    D2D1_BITMAP_PROPERTIES1 bitmapProperties =
        BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        _dipsPerInch * _compositionScaleX,
        _dipsPerInch * _compositionScaleY
        );

    // Direct2D needs the DXGI version of the backbuffer surface pointer.
    ComPtr<IDXGISurface> dxgiBackBuffer;
    DX::ThrowIfFailed(
        _swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
        );

    // Get a D2D surface from the DXGI back buffer to use as the D2D render target.
    ThrowIfFailed(
        _d2dContext->CreateBitmapFromDxgiSurface(
        dxgiBackBuffer.Get(),
        &bitmapProperties,
        &_d2dTargetBitmap
        )
        );

    _d2dContext->SetDpi(_dipsPerInch * _compositionScaleX, _dipsPerInch * _compositionScaleY);
    _d2dContext->SetTarget(_d2dTargetBitmap.Get());
}

void DirectXPanelBase::Present()
{
    DXGI_PRESENT_PARAMETERS parameters = { 0 };
    parameters.DirtyRectsCount = 0;
    parameters.pDirtyRects = nullptr;
    parameters.pScrollRect = nullptr;
    parameters.pScrollOffset = nullptr;

    HRESULT hr = S_OK;

    hr = _swapChain->Present1(1, 0, &parameters);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OnDeviceLost();
    }
    else
    {
        ThrowIfFailed(hr);
    }
}

void DirectXPanelBase::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
    critical_section::scoped_lock lock(_criticalSection);
    ComPtr<IDXGIDevice3> dxgiDevice;
    _d3dDevice.As(&dxgiDevice);

    // Hints to the driver that the app is entering an idle state and that its memory can be used temporarily for other apps.
    dxgiDevice->Trim();
}

void DirectXPanelBase::OnSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    if (_width != e->NewSize.Width || _height != e->NewSize.Height)
    {        
        critical_section::scoped_lock lock(_criticalSection);

        // Store values so they can be accessed from a background thread.
        _width = max(e->NewSize.Width, 1.0f);
        _height = max(e->NewSize.Height, 1.0f);

        // Recreate size-dependent resources when the panel's size changes.
        CreateSizeDependentResources();
    }
}

void DirectXPanelBase::OnCompositionScaleChanged(SwapChainPanel ^sender, Object ^args)
{
    if (_compositionScaleX != CompositionScaleX || _compositionScaleY != CompositionScaleY)
    {        
        critical_section::scoped_lock lock(_criticalSection);

        // Store values so they can be accessed from a background thread.
        _compositionScaleX = this->CompositionScaleX;
        _compositionScaleY = this->CompositionScaleY;
        
        // Recreate size-dependent resources when the composition scale changes.
        CreateSizeDependentResources();
    }    
}

void DirectXPanelBase::OnDeviceLost()
{
    _loadingComplete = false;

    _swapChain = nullptr;

    // Make sure the rendering state has been released.
    _d3dContext->OMSetRenderTargets(0, nullptr, nullptr);

    _d2dContext->SetTarget(nullptr);
    _d2dTargetBitmap = nullptr;

    _d2dContext = nullptr;
    _d2dDevice = nullptr;

    _d3dContext->Flush();

    CreateDeviceResources();
    CreateSizeDependentResources();
}