#pragma once
#include "pch.h"
#include "DirectXPanelBase.h"
#include "StepTimer.h"
#include "ShaderStructures.h"
#include <robuffer.h>

namespace EquirectangularImageViewer
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class EquirectangularViewer sealed : public DirectXPanelBase
	{
	private protected:

		virtual void Render() override;
		virtual float CalculateFov();
		virtual void CreateDeviceResources() override;
		virtual void CreateSizeDependentResources() override;
		virtual void OnDeviceLost() override;

		Microsoft::WRL::ComPtr<IDXGIOutput>                 _dxgiOutput;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>      _renderTargetView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView>      _depthStencilView;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>          _vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>           _pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>           _inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>                _vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>                _indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>                _constantBuffer;
		DX::ModelViewProjectionConstantBuffer               _constantBufferData;

		uint32                                              _indexCount;
		Windows::Foundation::IAsyncAction^ _renderLoopWorker;
		// Rendering loop timer.
		DX::StepTimer                                       _timer;

		std::unique_ptr<DirectX::GeometricPrimitive>		_shape;
		DirectX::SimpleMath::Matrix							_world;
		DirectX::SimpleMath::Matrix							_view;
		DirectX::SimpleMath::Matrix							_proj;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	_texture;
		std::unique_ptr<DirectX::BasicEffect>				_effect;
		std::unique_ptr<DirectX::CommonStates>				_states;

		float												_yaw;
		float												_pitch;
		float												_roll;
		float												_fov;
		Platform::String^ _texturePath;

	public:
		EquirectangularViewer();

		void StartRenderLoop();
		void StopRenderLoop();
		void LoadTexture(Platform::String^ path);

		property float Yaw
		{
			float get() { return _yaw; }
			void set(float value) { _yaw = value; }
		}

		property float Pitch
		{
			float get() { return _pitch; }
			void set(float value) { _pitch = value; }
		}

		property float Roll
		{
			float get() { return _roll; }
			void set(float value) { _roll = value; }
		}

		property float FieldOfView
		{
			float get() { return _fov; }
			void set(float value) { _fov = value; }
		}



	private:
		~EquirectangularViewer();
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		byte* GetPointerToPixelData(Windows::Storage::Streams::IBuffer^ pixelBuffer, unsigned int* length);
	};

}