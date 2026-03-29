// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef DXTK

#include "DeviceResources.h"
#include "../Utils/DebugLog.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
#if defined(_DEBUG)
    // Check for SDK Layer support.
    inline bool SdkLayersAvailable()
    {
        HRESULT HR = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
            0,
            D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
            nullptr,                    // Any feature level will do.
            0,
            D3D11_SDK_VERSION,
            nullptr,                    // No need to keep the D3D device reference.
            nullptr,                    // No need to know the feature level.
            nullptr                     // No need to keep the D3D device context reference.
            );

        return SUCCEEDED(HR);
    }
#endif
};

// Constructor for DeviceResources.
DeviceResources::DeviceResources(
	DXGI_FORMAT InBackBufferFormat,
	DXGI_FORMAT InDepthBufferFormat,
	UINT InBackBufferCount,
	D3D_FEATURE_LEVEL InMinFeatureLevel) noexcept :
	ScreenViewport{},
	BackBufferFormat(InBackBufferFormat),
	DepthBufferFormat(InDepthBufferFormat),
	BackBufferCount(InBackBufferCount),
	D3DMinFeatureLevel(InMinFeatureLevel),
	Window(nullptr),
	D3DFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	OutputSize{ 0, 0, 1, 1 },
	DeviceNotify(nullptr)
{
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DeviceResources::CreateDeviceResources()
{
	UINT CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (SdkLayersAvailable())
	{
		// If the project is in a debug build, enable debugging via SDK Layers with this flag.
		CreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
	else
	{
		OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
	}
#endif

	// Determine DirectX hardware feature levels this app will support.
	static const D3D_FEATURE_LEVEL s_FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};

	UINT FeatLevelCount = 0;
	for (; FeatLevelCount < _countof(s_FeatureLevels); ++FeatLevelCount)
	{
		if (s_FeatureLevels[FeatLevelCount] < D3DMinFeatureLevel)
			break;
	}

	if (!FeatLevelCount)
	{
		throw std::out_of_range("minFeatureLevel too high");
	}

	ComPtr<IDXGIAdapter1> Adapter;
	GetHardwareAdapter(Adapter.GetAddressOf());

	// Create the Direct3D 11 API device object and a corresponding context.
	HRESULT HR = E_FAIL;
	if (Adapter)
	{
		HR = D3D11CreateDevice(
			Adapter.Get(),
			D3D_DRIVER_TYPE_UNKNOWN,
			0,
			CreationFlags,
			s_FeatureLevels,
			FeatLevelCount,
			D3D11_SDK_VERSION,
			D3DDevice.ReleaseAndGetAddressOf(),   // Returns the Direct3D device created.
			&D3DFeatureLevel,                     // Returns feature level of device created.
			D3DContext.ReleaseAndGetAddressOf()   // Returns the device immediate context.
		);

		if (HR == E_INVALIDARG && FeatLevelCount > 1)
		{
			assert(s_FeatureLevels[0] == D3D_FEATURE_LEVEL_11_1);

			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			HR = D3D11CreateDevice(
				Adapter.Get(),
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				CreationFlags,
				&s_FeatureLevels[1],
				FeatLevelCount - 1,
				D3D11_SDK_VERSION,
				D3DDevice.ReleaseAndGetAddressOf(),
				&D3DFeatureLevel,
				D3DContext.ReleaseAndGetAddressOf()
			);
		}
	}
#if defined(NDEBUG)
	else
	{
		throw std::exception("No Direct3D hardware device found");
	}
#else
	if (FAILED(HR))
	{
		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see: 
		// http://go.microsoft.com/fwlink/?LinkId=286690
		HR = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
			0,
			CreationFlags,
			s_FeatureLevels,
			FeatLevelCount,
			D3D11_SDK_VERSION,
			D3DDevice.ReleaseAndGetAddressOf(),
			&D3DFeatureLevel,
			D3DContext.ReleaseAndGetAddressOf()
		);

		if (HR == E_INVALIDARG && FeatLevelCount > 1)
		{
			assert(s_FeatureLevels[0] == D3D_FEATURE_LEVEL_11_1);

			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			HR = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				CreationFlags,
				&s_FeatureLevels[1],
				FeatLevelCount - 1,
				D3D11_SDK_VERSION,
				D3DDevice.ReleaseAndGetAddressOf(),
				&D3DFeatureLevel,
				D3DContext.ReleaseAndGetAddressOf()
			);
		}

		if (SUCCEEDED(HR))
		{
			OutputDebugStringA("Direct3D Adapter - WARP\n");
		}
	}
#endif

	ThrowIfFailed(HR);

#ifndef NDEBUG
	ComPtr<ID3D11Debug> D3DDebug;
	if (SUCCEEDED(D3DDevice.As(&D3DDebug)))
	{
		ComPtr<ID3D11InfoQueue> D3DInfoQueue;
		if (SUCCEEDED(D3DDebug.As(&D3DInfoQueue)))
		{
#ifdef _DEBUG
			D3DInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
			D3DInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
			D3D11_MESSAGE_ID Hide[] =
			{
				D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
			};
			D3D11_INFO_QUEUE_FILTER Filter = {};
			Filter.DenyList.NumIDs = _countof(Hide);
			Filter.DenyList.pIDList = Hide;
			D3DInfoQueue->AddStorageFilterEntries(&Filter);
		}
	}
#endif

	// Obtain Direct3D 11.1 interfaces (if available)
	if (SUCCEEDED(D3DDevice.As(&D3DDevice1)))
	{
		(void)D3DContext.As(&D3DContext1);
		(void)D3DContext.As(&D3DAnnotation);
	}
}

// These resources need to be recreated every time the window size is changed.
void DeviceResources::CreateWindowSizeDependentResources()
{
	if (!Window)
	{
		throw std::exception("Call SetWindow with a valid Win32 window handle");
	}

	// Clear the previous window size specific context.
	ID3D11RenderTargetView* NullViews[] = { nullptr };
	D3DContext->OMSetRenderTargets(_countof(NullViews), NullViews, nullptr);
	D3DRenderTargetView.Reset();
	D3DDepthStencilView.Reset();
	RenderTarget.Reset();
	DepthStencil.Reset();
	D3DContext->Flush();

	// Determine the render target size in pixels.
	UINT BackBufferWidth = std::max<UINT>(OutputSize.right - OutputSize.left, 1);
	UINT BackBufferHeight = std::max<UINT>(OutputSize.bottom - OutputSize.top, 1);

	if (SwapChain)
	{
		// If the swap chain already exists, resize it.
		HRESULT HR = SwapChain->ResizeBuffers(
			BackBufferCount,
			BackBufferWidth,
			BackBufferHeight,
			BackBufferFormat,
			0
		);

		if (HR == DXGI_ERROR_DEVICE_REMOVED || HR == DXGI_ERROR_DEVICE_RESET)
		{
#ifdef _DEBUG
			char Buff[64] = {};
			sprintf_s(Buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n", (HR == DXGI_ERROR_DEVICE_REMOVED) ? D3DDevice->GetDeviceRemovedReason() : HR);
			OutputDebugStringA(Buff);
#endif
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			HandleDeviceLost();

			// Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
			// and correctly set up the new device.
			return;
		}
		else
		{
			ThrowIfFailed(HR);
		}
	}
	else
	{
		// Otherwise, create a new one using the same adapter as the existing Direct3D device.

		// This sequence obtains the DXGI factory that was used to create the Direct3D device above.
		ComPtr<IDXGIDevice1> DXGIDevice;
		ThrowIfFailed(D3DDevice.As(&DXGIDevice));

		ComPtr<IDXGIAdapter> DXGIAdapter;
		ThrowIfFailed(DXGIDevice->GetAdapter(DXGIAdapter.GetAddressOf()));

		ComPtr<IDXGIFactory1> DXGIFactory;
		ThrowIfFailed(DXGIAdapter->GetParent(IID_PPV_ARGS(DXGIFactory.GetAddressOf())));

		ComPtr<IDXGIFactory2> DXGIFactory2;
		if (SUCCEEDED(DXGIFactory.As(&DXGIFactory2)))
		{
			// DirectX 11.1 or later
			DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
			SwapChainDesc.Width = BackBufferWidth;
			SwapChainDesc.Height = BackBufferHeight;
			SwapChainDesc.Format = BackBufferFormat;
			SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			SwapChainDesc.BufferCount = BackBufferCount;
			SwapChainDesc.SampleDesc.Count = 1;
			SwapChainDesc.SampleDesc.Quality = 0;
			SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
			SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

			DXGI_SWAP_CHAIN_FULLSCREEN_DESC FSSwapChainDesc = {};
			FSSwapChainDesc.Windowed = TRUE;

			// Create a SwapChain from a Win32 window.
			ThrowIfFailed(DXGIFactory2->CreateSwapChainForHwnd(
				D3DDevice.Get(),
				Window,
				&SwapChainDesc,
				&FSSwapChainDesc,
				nullptr, SwapChain1.ReleaseAndGetAddressOf()
			));

			ThrowIfFailed(SwapChain1.As(&SwapChain));
		}
		else
		{
			// DirectX 11.0
			DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
			SwapChainDesc.BufferDesc.Width = BackBufferWidth;
			SwapChainDesc.BufferDesc.Height = BackBufferHeight;
			SwapChainDesc.BufferDesc.Format = BackBufferFormat;
			SwapChainDesc.SampleDesc.Count = 1;
			SwapChainDesc.SampleDesc.Quality = 0;
			SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			SwapChainDesc.BufferCount = BackBufferCount;
			SwapChainDesc.OutputWindow = Window;
			SwapChainDesc.Windowed = TRUE;
			SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			ThrowIfFailed(DXGIFactory->CreateSwapChain(
				D3DDevice.Get(),
				&SwapChainDesc,
				SwapChain.ReleaseAndGetAddressOf()
			));
		}

		// This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
		ThrowIfFailed(DXGIFactory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER));
	}

	// Create a render target view of the swap chain back buffer.
	ThrowIfFailed(SwapChain->GetBuffer(0, IID_PPV_ARGS(RenderTarget.ReleaseAndGetAddressOf())));

	CD3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, BackBufferFormat);
	ThrowIfFailed(D3DDevice->CreateRenderTargetView(
		RenderTarget.Get(),
		&RenderTargetViewDesc,
		D3DRenderTargetView.ReleaseAndGetAddressOf()
	));

	if (DepthBufferFormat != DXGI_FORMAT_UNKNOWN)
	{
		// Create a depth stencil view for use with 3D rendering if needed.
		CD3D11_TEXTURE2D_DESC DepthStencilDesc(
			DepthBufferFormat,
			BackBufferWidth,
			BackBufferHeight,
			1, // This depth stencil view has only one texture.
			1, // Use a single mipmap level.
			D3D11_BIND_DEPTH_STENCIL
		);

		ThrowIfFailed(D3DDevice->CreateTexture2D(
			&DepthStencilDesc,
			nullptr,
			DepthStencil.ReleaseAndGetAddressOf()
		));

		CD3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
		ThrowIfFailed(D3DDevice->CreateDepthStencilView(
			DepthStencil.Get(),
			&DepthStencilViewDesc,
			D3DDepthStencilView.ReleaseAndGetAddressOf()
		));
	}

	// Set the 3D rendering viewport to target the entire window.
	ScreenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		static_cast<float>(BackBufferWidth),
		static_cast<float>(BackBufferHeight)
	);
}

// This method is called when the Win32 window is created (or re-created).
void DeviceResources::SetWindow(HWND InWindow, int Width, int Height)
{
	Window = InWindow;

	OutputSize.left = OutputSize.top = 0;
	OutputSize.right = Width;
	OutputSize.bottom = Height;
}

// This method is called when the Win32 window changes size
bool DeviceResources::WindowSizeChanged(int Width, int Height)
{
	RECT NewRC;
	NewRC.left = NewRC.top = 0;
	NewRC.right = Width;
	NewRC.bottom = Height;
	if (NewRC == OutputSize)
	{
		return false;
	}

	OutputSize = NewRC;
	CreateWindowSizeDependentResources();
	return true;
}

// Recreate all device resources and set them back to the current state.
void DeviceResources::HandleDeviceLost()
{
	if (DeviceNotify)
	{
		DeviceNotify->OnDeviceLost();
	}

	D3DDepthStencilView.Reset();
	D3DRenderTargetView.Reset();
	SwapChain.Reset();
	SwapChain1.Reset();
	D3DContext.Reset();
	D3DContext1.Reset();
	D3DAnnotation.Reset();
	D3DDevice1.Reset();

#ifdef _DEBUG
	{
		ComPtr<ID3D11Debug> D3DDebug;
		if (SUCCEEDED(D3DDevice.As(&D3DDebug)))
		{
			D3DDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
		}
	}
#endif

	D3DDevice.Reset();

	CreateDeviceResources();
	CreateWindowSizeDependentResources();

	if (DeviceNotify)
	{
		DeviceNotify->OnDeviceRestored();
	}
}

// Present the contents of the swap chain to the screen.
void DeviceResources::Present()
{
	// The first argument instructs DXGI to block until VSync, putting the application
	// to sleep until the next VSync. This ensures we don't waste any cycles rendering
	// frames that will never be displayed to the screen.
	HRESULT HR = SwapChain->Present(1, 0);

	if (D3DContext1)
	{
		// Discard the contents of the render target.
		// This is a valid operation only when the existing contents will be entirely
		// overwritten. If dirty or scroll rects are used, this call should be removed.
		D3DContext1->DiscardView(D3DRenderTargetView.Get());

		if (D3DDepthStencilView)
		{
			// Discard the contents of the depth stencil.
			D3DContext1->DiscardView(D3DDepthStencilView.Get());
		}
	}

	// If the device was removed either by a disconnection or a driver upgrade, we 
	// must recreate all device resources.
	if (HR == DXGI_ERROR_DEVICE_REMOVED || HR == DXGI_ERROR_DEVICE_RESET)
	{
#ifdef _DEBUG
		char Buff[64] = {};
		sprintf_s(Buff, "Device Lost on Present: Reason code 0x%08X\n", (HR == DXGI_ERROR_DEVICE_REMOVED) ? D3DDevice->GetDeviceRemovedReason() : HR);
		OutputDebugStringA(Buff);
#endif
		HandleDeviceLost();
	}
	else
	{
		ThrowIfFailed(HR);
	}
}

// This method acquires the first available hardware adapter.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
void DeviceResources::GetHardwareAdapter(IDXGIAdapter1** PPAdapter)
{
	*PPAdapter = nullptr;

	ComPtr<IDXGIFactory1> DXGIFactory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(DXGIFactory.GetAddressOf())));

	ComPtr<IDXGIAdapter1> Adapter;
	for (UINT AdapterIndex = 0;
		DXGI_ERROR_NOT_FOUND != DXGIFactory->EnumAdapters1(
			AdapterIndex,
			Adapter.ReleaseAndGetAddressOf());
		AdapterIndex++)
	{
		DXGI_ADAPTER_DESC1 Desc;
		Adapter->GetDesc1(&Desc);

		if (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		FDebugLog::Log(L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", AdapterIndex, Desc.VendorId, Desc.DeviceId, Desc.Description);
		break;
	}

	*PPAdapter = Adapter.Detach();
}

#endif //DXTK
