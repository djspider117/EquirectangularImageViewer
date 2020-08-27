#pragma once

#pragma region Includes

#include "pch.h"
#include "EquirectangularViewer.h"
#include "DirectXHelper.h"

#include <DirectXMath.h>
#include <DirectXColors.h>
#include <math.h>
#include <ppltasks.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <robuffer.h>

#pragma endregion

#pragma region Usings



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
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Interop;
using namespace Concurrency;
using namespace D2D1;
using namespace EquirectangularImageViewer;
using namespace DX;
using namespace DirectX;
using namespace DirectX::SimpleMath;

#pragma endregion

#pragma region Init

EquirectangularViewer::EquirectangularViewer() :
	_indexCount(0),
	_fov(65)
{
	critical_section::scoped_lock lock(_criticalSection);
	CreateDeviceIndependentResources();
	CreateDeviceResources();
	CreateSizeDependentResources();

	Loaded += ref new RoutedEventHandler(this, &EquirectangularViewer::OnLoaded);
	Unloaded += ref new RoutedEventHandler(this, &EquirectangularViewer::OnUnloaded);

}

void EquirectangularViewer::OnLoaded(Object^ sender, RoutedEventArgs^ e)
{
	this->StartRenderLoop();
}

#pragma endregion

#pragma region API

void EquirectangularViewer::StartRenderLoop()
{
	// If the animation render loop is already running then do not start another thread.
	if (_renderLoopWorker != nullptr && _renderLoopWorker->Status == AsyncStatus::Started)
	{
		return;
	}

	// Create a task that will be run on a background thread.
	auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction^ action)
		{
			// Calculate the updated frame and render once per vertical blanking interval.
			while (action->Status == AsyncStatus::Started)
			{
				_timer.Tick([&]()
					{
						critical_section::scoped_lock lock(_criticalSection);
						Render();
					});

				// Halt the thread until the next vblank is reached.
				// This ensures the app isn't updating and rendering faster than the display can refresh,
				// which would unnecessarily spend extra CPU and GPU resources.  This helps improve battery life.
				_dxgiOutput->WaitForVBlank();
			}
		});

	// Run task on a dedicated high priority background thread.
	_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void EquirectangularViewer::StopRenderLoop()
{
	// Cancel the asynchronous task and let the render thread exit.
	_renderLoopWorker->Cancel();
}

void EquirectangularViewer::LoadTexture(Platform::String^ path)
{
	auto getFileTask = create_task(Windows::Storage::StorageFile::GetFileFromPathAsync(path));
	getFileTask.then([&](IStorageFile^ sfile)
		{
			auto readTask = create_task(FileIO::ReadBufferAsync(sfile));
			readTask.then([&](IBuffer^ buff)
				{
					UINT len;
					PBYTE pData = GetPointerToPixelData(buff, &len);

					ThrowIfFailed(CreateWICTextureFromMemory(_d3dDevice.Get(), pData, len, nullptr, _texture.ReleaseAndGetAddressOf()));
					_effect->SetTexture(_texture.Get());
				});
		});
}

byte* EquirectangularViewer::GetPointerToPixelData(IBuffer^ pixelBuffer, unsigned int* length)
{
	if (length != nullptr)
		*length = pixelBuffer->Length;

	ComPtr<IBufferByteAccess> bufferByteAccess;
	reinterpret_cast<IInspectable*>(pixelBuffer)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));

	byte* pixels = nullptr;
	bufferByteAccess->Buffer(&pixels);
	return pixels;
}

#pragma endregion

#pragma region Render

void EquirectangularViewer::Render()
{
	if (!_loadingComplete || _timer.GetFrameCount() == 0)
	{
		return;
	}

	float radYaw = XMConvertToRadians(_yaw);
	float radPitch = XMConvertToRadians(_pitch);
	float radRoll = XMConvertToRadians(_roll);


	// Set render targets to the screen.
	ID3D11RenderTargetView* const targets[1] = { _renderTargetView.Get() };
	_d3dContext->OMSetRenderTargets(1, targets, _depthStencilView.Get());

	// Clear the back buffer and depth stencil view.
	_d3dContext->ClearRenderTargetView(_renderTargetView.Get(), DirectX::Colors::Black);
	_d3dContext->ClearDepthStencilView(_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	_world = DirectX::SimpleMath::Matrix::CreateRotationY(radYaw)
		* DirectX::SimpleMath::Matrix::CreateRotationX(radPitch)
		* DirectX::SimpleMath::Matrix::CreateRotationZ(radRoll);


	_effect->SetWorld(_world);
	_shape->Draw(_effect.get(), _inputLayout.Get());

	Present();
}

#pragma endregion

#pragma region Resource Loading


void EquirectangularViewer::CreateDeviceResources()
{
	DirectXPanelBase::CreateDeviceResources();

	ComPtr<IDXGIFactory1> dxgiFactory;
	ComPtr<IDXGIAdapter> dxgiAdapter;

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));
	ThrowIfFailed(dxgiFactory->EnumAdapters(0, &dxgiAdapter));
	ThrowIfFailed(dxgiAdapter->EnumOutputs(0, &_dxgiOutput));

	_states = std::make_unique<CommonStates>(_d3dDevice.Get());

	_effect = std::make_unique<BasicEffect>(_d3dDevice.Get());
	_effect->SetTextureEnabled(true);
	_effect->SetPerPixelLighting(false);
	_effect->SetLightingEnabled(false);
	_effect->SetAmbientLightColor(FXMVECTOR{ 1.0f, 1.0f, 1.0f, 0.0f });

	_shape = GeometricPrimitive::CreateSphere(_d3dContext.Get(), 30.0f, 64, false);
	_shape->CreateInputLayout(_effect.get(), _inputLayout.ReleaseAndGetAddressOf());

	//ThrowIfFailed(CreateWICTextureFromFile(_d3dDevice.Get(), L".\\Content\\Pano2.jpg", nullptr, _texture.ReleaseAndGetAddressOf()));
	_effect->SetTexture(_texture.Get());

	_world = DirectX::SimpleMath::Matrix::Identity;

	_loadingComplete = true;
}

void EquirectangularViewer::CreateSizeDependentResources()
{
	_renderTargetView = nullptr;
	_depthStencilView = nullptr;

	ComPtr<ID3D11Texture2D> backBuffer;
	ComPtr<ID3D11Texture2D> depthStencil;


	DirectXPanelBase::CreateSizeDependentResources();


	ThrowIfFailed(_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));// Create a render target view of the swap chain back buffer.
	ThrowIfFailed(_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &_renderTargetView));// Create render target view.

	D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.0f, 0.0f, _renderTargetWidth, _renderTargetHeight);// Create and set viewport.

	_d3dContext->RSSetViewports(1, &viewport);

	CD3D11_TEXTURE2D_DESC depthStencilDesc(DXGI_FORMAT_D24_UNORM_S8_UINT, static_cast<UINT>(_renderTargetWidth), static_cast<UINT>(_renderTargetHeight), 1, 1, D3D11_BIND_DEPTH_STENCIL);

	ThrowIfFailed(_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencil));

	const CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D);
	ThrowIfFailed(_d3dDevice->CreateDepthStencilView(depthStencil.Get(), &depthStencilViewDesc, &_depthStencilView));

	float aspectRatio = _renderTargetWidth / _renderTargetHeight;

	float fovAngleY = 70.0f * XM_PI / 180.0f;
	if (aspectRatio < 1.0f)
	{
		fovAngleY /= aspectRatio;
	}

	_view = DirectX::SimpleMath::Matrix::CreateLookAt(Vector3(0.f, 0.f, 0.1f), Vector3::Zero, Vector3::UnitY);
	_proj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.3f, aspectRatio, 0.1f, 1000.f);

	_effect->SetView(_view);
	_effect->SetProjection(_proj);
}

#pragma endregion

#pragma region Cleanup

void EquirectangularViewer::OnDeviceLost()
{
	_shape.reset();
	_texture.Reset();
	_effect.reset();
	_inputLayout.Reset();
	_states.reset();
}

void EquirectangularViewer::OnUnloaded(Object^ sender, RoutedEventArgs^ e)
{
	this->StopRenderLoop();
}

EquirectangularViewer::~EquirectangularViewer()
{
	_renderLoopWorker->Cancel();
}


#pragma endregion

