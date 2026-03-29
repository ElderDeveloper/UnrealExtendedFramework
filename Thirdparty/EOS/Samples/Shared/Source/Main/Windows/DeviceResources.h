// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef DXTK

// Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
interface IDeviceNotify
{
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;
};

// Controls all the DirectX device resources.
class DeviceResources
{
public:
    DeviceResources(DXGI_FORMAT InBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                    DXGI_FORMAT InDepthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
                    UINT InBackBufferCount = 2,
                    D3D_FEATURE_LEVEL InMinFeatureLevel = D3D_FEATURE_LEVEL_9_1) noexcept;

    void CreateDeviceResources();
    void CreateWindowSizeDependentResources();
	HWND GetWindow() const { return Window; }
    void SetWindow(HWND window, int Width, int Height);
    bool WindowSizeChanged(int Width, int Height);
    void HandleDeviceLost();
    void RegisterDeviceNotify(IDeviceNotify* InDeviceNotify) { DeviceNotify = InDeviceNotify; }
    void Present();

    // Device Accessors.
    RECT GetOutputSize() const { return OutputSize; }

    // Direct3D Accessors.
    ID3D11Device*           GetD3DDevice() const                    { return D3DDevice.Get(); }
    ID3D11Device1*          GetD3DDevice1() const                   { return D3DDevice1.Get(); }
    ID3D11DeviceContext*    GetD3DDeviceContext() const             { return D3DContext.Get(); }
    ID3D11DeviceContext1*   GetD3DDeviceContext1() const            { return D3DContext1.Get(); }
    IDXGISwapChain*         GetSwapChain() const                    { return SwapChain.Get(); }
    IDXGISwapChain1*        GetSwapChain1() const                   { return SwapChain1.Get(); }
    D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const           { return D3DFeatureLevel; }
    ID3D11Texture2D*        GetRenderTarget() const                 { return RenderTarget.Get(); }
    ID3D11Texture2D*        GetDepthStencil() const                 { return DepthStencil.Get(); }
    ID3D11RenderTargetView*	GetRenderTargetView() const             { return D3DRenderTargetView.Get(); }
    ID3D11DepthStencilView* GetDepthStencilView() const             { return D3DDepthStencilView.Get(); }
    DXGI_FORMAT             GetBackBufferFormat() const             { return BackBufferFormat; }
    DXGI_FORMAT             GetDepthBufferFormat() const            { return DepthBufferFormat; }
    D3D11_VIEWPORT          GetScreenViewport() const               { return ScreenViewport; }
    UINT                    GetBackBufferCount() const              { return BackBufferCount; }

    // Performance events
    void PIXBeginEvent(_In_z_ const wchar_t* Name)
    {
        if (D3DAnnotation)
        {
            D3DAnnotation->BeginEvent(Name);
        }
    }

    void PIXEndEvent()
    {
        if (D3DAnnotation)
        {
            D3DAnnotation->EndEvent();
        }
    }

    void PIXSetMarker(_In_z_ const wchar_t* Name)
    {
        if (D3DAnnotation)
        {
            D3DAnnotation->SetMarker(Name);
        }
    }

private:
    void GetHardwareAdapter(IDXGIAdapter1** PPAdapter);

    // Direct3D objects.
    Microsoft::WRL::ComPtr<ID3D11Device>            D3DDevice;
    Microsoft::WRL::ComPtr<ID3D11Device1>           D3DDevice1;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     D3DContext;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1>    D3DContext1;
    Microsoft::WRL::ComPtr<IDXGISwapChain>          SwapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>         SwapChain1;
    Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> D3DAnnotation;

    // Direct3D rendering objects. Required for 3D.
    Microsoft::WRL::ComPtr<ID3D11Texture2D>         RenderTarget;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>         DepthStencil;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  D3DRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  D3DDepthStencilView;
    D3D11_VIEWPORT                                  ScreenViewport;

    // Direct3D properties.
    DXGI_FORMAT                                     BackBufferFormat;
    DXGI_FORMAT                                     DepthBufferFormat;
    UINT                                            BackBufferCount;
    D3D_FEATURE_LEVEL                               D3DMinFeatureLevel;

    // Cached device properties.
    HWND                                            Window;
    D3D_FEATURE_LEVEL                               D3DFeatureLevel;
    RECT                                            OutputSize;

    // The IDeviceNotify can be held directly as it owns the DeviceResources.
    IDeviceNotify*                                  DeviceNotify;
};

#endif //DXTK