#pragma once
#pragma warning(disable: 4005)

#include <maya/MStateManager.h>

// Includes for DX
#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>


class GarlandRenderOverride;
struct ShaderAndLayout
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* inputLayout;
};


class DxManager
{
public:
	DxManager(GarlandRenderOverride* gr);
	~DxManager();

	void Setup();
	void debug(const MHWRender::MDrawContext& drawContext);

protected:
	bool InitializeShadersFromByteData(const BYTE* vsByteData, size_t vsBtyeSize,
		const BYTE* psByteData, size_t sBtyeSize, const D3D11_INPUT_ELEMENT_DESC* layout, int numLayoutElements);
	bool CreateBuffers();
	bool UpdateStates(const MHWRender::MDrawContext& drawContext);

	GarlandRenderOverride* _gr;

	// DirectX device members
	ID3D11Device* _device = nullptr;
	ID3D11DeviceContext* _deviceContext = nullptr;
	ID3D11RenderTargetView* _mainRenderTargetView = nullptr;
	ID3D11DepthStencilView* _mainDepthStencilView = nullptr;

	// DirectX Rasterizer states.
	ID3D11RasterizerState* _dxRasterState = nullptr;
	const MHWRender::MRasterizerState* _mRasterState = nullptr;

	// DirectX Sampler states
	ID3D11SamplerState* _dxSamplerState = nullptr;
	const MHWRender::MSamplerState* _mSamplerState = nullptr;

	// DirectX Buffers
	ID3D11Buffer* _vertexBuffer = nullptr;
	ID3D11Buffer* _indexBuffer = nullptr;
	ID3D11Buffer* _vertexConstantBuffer = nullptr;
	ID3D11Buffer* _pixelConstantBuffer = nullptr;

	// DirectX Shaders
	ShaderAndLayout* unlitShader = nullptr;
};
