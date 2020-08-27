//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
//
//*********************************************************

#pragma once
#include "pch.h"
#include <concrt.h>

namespace EquirectangularImageViewer
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class DirectXPanelBase : public Windows::UI::Xaml::Controls::SwapChainPanel
	{
	protected private:
		DirectXPanelBase();

		virtual void CreateDeviceIndependentResources();
		virtual void CreateDeviceResources();
		virtual void CreateSizeDependentResources();

		virtual void OnDeviceLost();
		virtual void OnSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
		virtual void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel ^sender, Platform::Object ^args);
		virtual void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		virtual void OnResuming(Platform::Object^ sender, Platform::Object^ args) { };

		virtual void Render() { };
		virtual void Present();

		Microsoft::WRL::ComPtr<ID3D11Device1>                               _d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1>                        _d3dContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain2>                             _swapChain;

		Microsoft::WRL::ComPtr<ID2D1Factory2>                               _d2dFactory;
		Microsoft::WRL::ComPtr<ID2D1Device>                                 _d2dDevice;
		Microsoft::WRL::ComPtr<ID2D1DeviceContext>                          _d2dContext;
		Microsoft::WRL::ComPtr<ID2D1Bitmap1>                                _d2dTargetBitmap;

		D2D1_COLOR_F                                                        _backgroundColor;
		DXGI_ALPHA_MODE                                                     _alphaMode;

		bool                                                                _loadingComplete;

		Concurrency::critical_section                                       _criticalSection;

		float                                                               _renderTargetHeight;
		float                                                               _renderTargetWidth;

		float                                                               _compositionScaleX;
		float                                                               _compositionScaleY;

		float                                                               _height;
		float                                                               _width;

	};

}
