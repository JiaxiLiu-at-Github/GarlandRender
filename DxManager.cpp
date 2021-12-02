#include "DxManager.h"

#include <maya/MViewport2Renderer.h>
#include <maya/MGlobal.h>
#include <maya/MRenderTargetManager.h>
#include <maya/MDrawContext.h>
#include <maya/MDrawTraversal.h>
#include <maya/M3dView.h>
#include <maya/MFnDagNode.h>

#include "GarlandRender.h"
#include "util.h"
#include "build/shaders/unlit_vs.h"
#include "build/shaders/unlit_ps.h"
#include <DirectXMath.h>


using vec3 = DirectX::XMFLOAT3;
using vec4 = DirectX::XMFLOAT4;
using mat4 = DirectX::XMFLOAT4X4;

struct VSInputData
{
	vec3 position;
};

struct VSConstantBuffer
{
	mat4 WVP;
};

struct PSConstantBuffer
{
	vec4 diffuseMaterial;
};


class MSurfaceDrawTraversal : public MDrawTraversal
{
	virtual bool filterNode(const MDagPath& traversalItem)
	{
		bool prune = false;

		// Check to only prune shapes, not transforms.
		if (traversalItem.childCount() == 0)
		{
			if (!traversalItem.hasFn(MFn::kMesh) &&
				!traversalItem.hasFn(MFn::kNurbsSurface) &&
				!traversalItem.hasFn(MFn::kSubdiv)
				)
			{
				prune = true;
			}
		}
		return prune;
	}
};


DxManager::DxManager(GarlandRenderOverride* gr)
{
	_gr = gr;

	MHWRender::MRenderer* theRenderer = MHWRender::MRenderer::theRenderer();

	_device = (ID3D11Device*)theRenderer->GPUDeviceHandle();
	_device->GetImmediateContext(&_deviceContext);

	_mainRenderTargetView = (ID3D11RenderTargetView*)_gr->grColorRT()->resourceHandle();
	_mainDepthStencilView = (ID3D11DepthStencilView*)_gr->grDepthRT()->resourceHandle();

	size_t vsByteSize = sizeof(unlit_vs) / sizeof(unlit_vs[0]);
	size_t psByteSize = sizeof(unlit_ps) / sizeof(unlit_ps[0]);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	int numLayoutElements = sizeof layout / sizeof layout[0];

	bool result = InitializeShadersFromByteData(unlit_vs, vsByteSize, unlit_ps, psByteSize, layout, numLayoutElements);
	if (!result)
	{
		return;
	}

	result = CreateBuffers();
	if (!result)
	{
		return;
	}
}

DxManager::~DxManager()
{
	_gr = nullptr;
	_device = nullptr;
	_deviceContext = nullptr;
	_mainRenderTargetView = nullptr;
	_mainDepthStencilView = nullptr;
}

void DxManager::debug(const MHWRender::MDrawContext& drawContext)
{

	MHWRender::MRenderer* theRenderer = MHWRender::MRenderer::theRenderer();

	//  Draw some addition things after the scene draw
	bool drawingInteractive = false;

	MString mPanelName = _gr->panelName();
	M3dView mView;

	if (mPanelName.length() &&
		(M3dView::getM3dViewFromModelPanel(mPanelName, mView) == MStatus::kSuccess))
	{
		drawingInteractive = true;
	}

	// Get the current viewport and scale it relative to that
	int targetW, targetH;
	drawContext.getRenderTargetSize(targetW, targetH);

	// Some user drawing
	MDagPath cameraPath;
	if (drawingInteractive)
	{
		mView.getCamera(cameraPath);
	}

	// Set view and projection
	MMatrix view =
		drawContext.getMatrix(MHWRender::MFrameContext::kWorldViewMtx);
	MMatrix projection =
		drawContext.getMatrix(MHWRender::MFrameContext::kProjectionMtx);

	mat4 currentViewMatrix = mat4(
		(float)view.matrix[0][0], (float)view.matrix[0][1], (float)view.matrix[0][2], (float)view.matrix[0][3],
		(float)view.matrix[1][0], (float)view.matrix[1][1], (float)view.matrix[1][2], (float)view.matrix[1][3],
		(float)view.matrix[2][0], (float)view.matrix[2][1], (float)view.matrix[2][2], (float)view.matrix[2][3],
		(float)view.matrix[3][0], (float)view.matrix[3][1], (float)view.matrix[3][2], (float)view.matrix[3][3]
	);

	mat4 currentProjectionMatrix = mat4(
		(float)projection.matrix[0][0], (float)projection.matrix[0][1], (float)projection.matrix[0][2], (float)projection.matrix[0][3],
		(float)projection.matrix[1][0], (float)projection.matrix[1][1], (float)projection.matrix[1][2], (float)projection.matrix[1][3],
		(float)projection.matrix[2][0], (float)projection.matrix[2][1], (float)projection.matrix[2][2], (float)projection.matrix[2][3],
		(float)projection.matrix[3][0], (float)projection.matrix[3][1], (float)projection.matrix[3][2], (float)projection.matrix[3][3]
	);

	// Update state objects
	UpdateStates(drawContext);

	// Clear the background
	if (_deviceContext)
	{
		if (_mainRenderTargetView)
		{
			float clearColor[4] = { 0.0f, 0.125f, 0.6f, 0.0f };
			_deviceContext->ClearRenderTargetView(_mainRenderTargetView, clearColor);
		}
		if (_mainDepthStencilView)
		{
			_deviceContext->ClearDepthStencilView(_mainDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
		}
	}

	if (!cameraPath.isValid())
		return;

	MDrawTraversal* trav = NULL;
	trav = new MSurfaceDrawTraversal;

	if (!trav)
		return;

	int width, height;
	drawContext.getRenderTargetSize(width, height);

	trav->enableFiltering(true);
	trav->setFrustum(cameraPath, width, height);

	if (!trav->frustumValid())
	{
		delete trav;
		trav = NULL;
		return;
	}
	trav->traverse();

	unsigned int numItems = trav->numberOfItems();
	for (unsigned int i = 0; i < numItems; i++)
	{
		MDagPath path;
		trav->itemPath(i, path);

		if (path.isValid())
		{
			bool performBoundsDraw = false;

			// Draw surfaces (polys, nurbs, subdivs)
			bool active = false;
			bool templated = false;
			float boundsColor[3] = { 1.0f, 1.0f, 1.0f };

			if (path.hasFn(MFn::kMesh) ||
				path.hasFn(MFn::kNurbsSurface) ||
				path.hasFn(MFn::kSubdiv))
			{
				performBoundsDraw = true;
				if (trav->itemHasStatus(i, MDrawTraversal::kActiveItem))
				{
					active = true;
				}
				else if (trav->itemHasStatus(i, MDrawTraversal::kTemplateItem))
				{
					boundsColor[0] = 0.2f;
					boundsColor[1] = 0.2f;
					boundsColor[2] = 0.2f;
					templated = true;
				}
				else
				{
					if (path.hasFn(MFn::kMesh))
					{
						boundsColor[0] = 0.286f;
						boundsColor[1] = 0.706f;
						boundsColor[2] = 1.0f;
					}
					else if (path.hasFn(MFn::kNurbsSurface))
					{
						boundsColor[0] = 0.486f;
						boundsColor[1] = 0.306f;
						boundsColor[2] = 1.0f;
					}
					else
					{
						boundsColor[0] = 0.886f;
						boundsColor[1] = 0.206f;
						boundsColor[2] = 1.0f;
					}
				}
			}

			// Draw bounds
			if (performBoundsDraw)
			{
				MFnDagNode dagNode(path);
				MBoundingBox box = dagNode.boundingBox();
				
				if (!_vertexBuffer || !_indexBuffer || !_vertexConstantBuffer || !_pixelConstantBuffer)
				{
					return;
				}

				bool useDrawContextToSetState = true;
				MStatus status = MStatus::kFailure;

				if (useDrawContextToSetState)
				{
					MHWRender::MStateManager* stateManager = drawContext.getStateManager();
					if (stateManager)
					{
						status = stateManager->setRasterizerState(_mRasterState);
					}
				}
				if (status != MStatus::kSuccess)
				{
					_deviceContext->RSSetState(_dxRasterState);
				}

				MMatrix  matrix = path.inclusiveMatrix();

				// Transform from object to world space
				mat4 mat = mat4
				(
					(float)matrix.matrix[0][0], (float)matrix.matrix[0][1], (float)matrix.matrix[0][2], (float)matrix.matrix[0][3],
					(float)matrix.matrix[1][0], (float)matrix.matrix[1][1], (float)matrix.matrix[1][2], (float)matrix.matrix[1][3],
					(float)matrix.matrix[2][0], (float)matrix.matrix[2][1], (float)matrix.matrix[2][2], (float)matrix.matrix[2][3],
					(float)matrix.matrix[3][0], (float)matrix.matrix[3][1], (float)matrix.matrix[3][2], (float)matrix.matrix[3][3]
				);

				// Adjust the unit cube to the bounds
				MPoint	minPt = box.min();
				MPoint	maxPt = box.max();

				float minVal[3] = { (float)minPt.x, (float)minPt.y, (float)minPt.z };
				float maxVal[3] = { (float)maxPt.x, (float)maxPt.y, (float)maxPt.z };

				mat4 bounds(
					0.5f * (maxVal[0] - minVal[0]), 0.0f, 0.0f, 0.0f,
					0.0f, 0.5f * (maxVal[1] - minVal[1]), 0.0f, 0.0f,
					0.0f, 0.0f, 0.5f * (maxVal[2] - minVal[2]), 0.0f,
					0.5f * (maxVal[0] + minVal[0]), 0.5f * (maxVal[1] + minVal[1]), 0.5f * (maxVal[2] + minVal[2]), 1.0f
				);

				// Set vertex buffer
				UINT stride = sizeof(VSInputData);
				UINT offset = 0;
				_deviceContext->IASetVertexBuffers(0, 1, &_vertexBuffer, &stride, &offset);

				// Set index buffer
				_deviceContext->IASetIndexBuffer(_indexBuffer, DXGI_FORMAT_R16_UINT, 0);

				// Set constant buffer
				VSConstantBuffer vb;
				DirectX::XMMATRIX x0 = XMLoadFloat4x4(&bounds);
				DirectX::XMMATRIX x1 = XMLoadFloat4x4(&mat);
				DirectX::XMMATRIX x2 = XMLoadFloat4x4(&currentViewMatrix);
				DirectX::XMMATRIX x3 = XMLoadFloat4x4(&currentProjectionMatrix);
				
				DirectX::XMMATRIX result = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(x0, x1), x2);
				DirectX::XMStoreFloat4x4(&(vb.WVP), XMMatrixTranspose(DirectX::XMMatrixMultiply(result, x3)));
				_deviceContext->UpdateSubresource(_vertexConstantBuffer, 0, NULL, &vb, 0, 0);

				PSConstantBuffer pb;
				pb.diffuseMaterial = vec4(boundsColor[0], boundsColor[1], boundsColor[2], 0.f);
				_deviceContext->UpdateSubresource(_pixelConstantBuffer, 0, NULL, &pb, 0, 0);

				// Set primitive topology
				_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

				// bind shaders
				_deviceContext->VSSetShader(unlitShader->vertexShader, NULL, 0);
				_deviceContext->VSSetConstantBuffers(0, 1, &_vertexConstantBuffer);
				_deviceContext->IASetInputLayout(unlitShader->inputLayout);
				_deviceContext->PSSetShader(unlitShader->pixelShader, NULL, 0);
				_deviceContext->PSSetConstantBuffers(0, 1, &_pixelConstantBuffer);

				// draw
				_deviceContext->DrawIndexed(24, 0, 0);

			}
		}
	}

	if (trav)
	{
		delete trav;
		trav = NULL;
	}
}

bool DxManager::InitializeShadersFromByteData(const BYTE* vsByteData, size_t vsByteSize,
	const BYTE* psByteData, size_t psByteSize, const D3D11_INPUT_ELEMENT_DESC* layout, int numLayoutElements)
{
	HRESULT hres;

	ID3D11VertexShader* pVertexShader = NULL;
	hres = _device->CreateVertexShader(vsByteData, vsByteSize, NULL, &pVertexShader);
	if (FAILED(hres))
	{
		MGlobal::displayError("Failed to create vertex shader");
		return false;
	}

	ID3D11InputLayout* pVertexLayout = NULL;
	hres = _device->CreateInputLayout(layout, numLayoutElements, vsByteData, vsByteSize, &pVertexLayout);
	if (FAILED(hres))
	{
		MGlobal::displayError("Failed to create input layout");
		return false;
	}

	// Set up pixel shader
	ID3D11PixelShader* pPixelShader = NULL;
	hres = _device->CreatePixelShader(psByteData, psByteSize, NULL, &pPixelShader);

	if (FAILED(hres))
	{
		MGlobal::displayError("Failed to create pixel shader");
		return false;
	}

	ShaderAndLayout* sr = new ShaderAndLayout;
	sr->vertexShader = pVertexShader;
	sr->pixelShader = pPixelShader;
	sr->inputLayout = pVertexLayout;
	unlitShader = sr;

	return true;
}

bool DxManager::CreateBuffers()
{
	HRESULT hr;

	// Create vertex buffer
	VSInputData vertices[] =
	{
		{ vec3(-1.0f, -1.0f, -1.0f) },
		{ vec3(-1.0f, -1.0f,  1.0f) },
		{ vec3(-1.0f,  1.0f, -1.0f) },
		{ vec3(-1.0f,  1.0f,  1.0f) },
		{ vec3(1.0f, -1.0f, -1.0f) },
		{ vec3(1.0f, -1.0f,  1.0f) },
		{ vec3(1.0f,  1.0f, -1.0f) },
		{ vec3(1.0f,  1.0f,  1.0f) },
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(vertices);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;
	hr = _device->CreateBuffer(&bd, &InitData, &_vertexBuffer);

	if (FAILED(hr))
	{
		MGlobal::displayError("Failed to create vertex buffer");
		return false;
	}

	// Create index buffer
	WORD indices[] =
	{
		0, 1,
		1, 3,
		3, 2,
		2, 0,
		4, 5,
		5, 7,
		7, 6,
		6, 4,
		0, 4,
		1, 5,
		2, 6,
		3, 7,
	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(indices);
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = indices;
	hr = _device->CreateBuffer(&bd, &InitData, &_indexBuffer);

	if (FAILED(hr))
	{
		MGlobal::displayError("Failed to create index buffer");
		return false;
	}

	// Create vertex constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VSConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = _device->CreateBuffer(&bd, NULL, &_vertexConstantBuffer);

	if (FAILED(hr))
	{
		MGlobal::displayError("Failed to create vertex const buffer");
		return false;
	}

	// Create pixel constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(PSConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = _device->CreateBuffer(&bd, NULL, &_pixelConstantBuffer);

	if (FAILED(hr))
	{
		MGlobal::displayError("Failed to create pixel const buffer");
		return false;
	}

	return true;
}

bool DxManager::UpdateStates(const MHWRender::MDrawContext& drawContext)
{
	HRESULT hr;

	MHWRender::MStateManager* stateManager = drawContext.getStateManager();
	bool createStatesViaMayaAPI = (stateManager != NULL);

	if (createStatesViaMayaAPI)
	{
		MHWRender::MRasterizerStateDesc rasterizerStateDesc;
		rasterizerStateDesc.fillMode = MHWRender::MRasterizerState::kFillSolid;
		rasterizerStateDesc.cullMode = MHWRender::MRasterizerState::kCullNone;
		rasterizerStateDesc.frontCounterClockwise = true;
		rasterizerStateDesc.depthBiasIsFloat = true;
		rasterizerStateDesc.depthBias = 0.0f;
		rasterizerStateDesc.depthBiasClamp = 0.0f;
		rasterizerStateDesc.slopeScaledDepthBias = 0.0f;
		rasterizerStateDesc.depthClipEnable = true;
		rasterizerStateDesc.scissorEnable = false;
		rasterizerStateDesc.multiSampleEnable = false;
		rasterizerStateDesc.antialiasedLineEnable = false;

		_mRasterState = stateManager->acquireRasterizerState(rasterizerStateDesc);
		if (_mRasterState)
		{
			_dxRasterState = (ID3D11RasterizerState*)_mRasterState->resourceHandle();
		}
		else
		{
			MHWRender::MStateManager::releaseRasterizerState(_mRasterState);
			_mRasterState = NULL;
			createStatesViaMayaAPI = false;
		}

		MHWRender::MSamplerStateDesc sd;
		_mSamplerState = stateManager->acquireSamplerState(sd);
		if (_mSamplerState)
		{
			_dxSamplerState = (ID3D11SamplerState*)_mSamplerState->resourceHandle();
		}
	}

	if (!createStatesViaMayaAPI)
	{
		D3D11_RASTERIZER_DESC rd;
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_BACK;
		rd.FrontCounterClockwise = TRUE;
		rd.DepthBias = 0;
		rd.SlopeScaledDepthBias = 0.0f;
		rd.DepthBiasClamp = 0.0f;
		rd.DepthClipEnable = TRUE;
		rd.ScissorEnable = FALSE;
		rd.MultisampleEnable = FALSE;
		rd.AntialiasedLineEnable = FALSE;

		hr = _device->CreateRasterizerState(&rd, &_dxRasterState);
		if (FAILED(hr))
		{
			MGlobal::displayError("Failed to create raster state");
			return false;
		}
	}

	return true;
}
