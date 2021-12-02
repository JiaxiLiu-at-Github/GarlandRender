#pragma once
#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>


class DxManager;


class GarlandRenderOverride : public MHWRender::MRenderOverride
{
public:
	GarlandRenderOverride( const MString & name );
	~GarlandRenderOverride();

	MHWRender::DrawAPI supportedDrawAPIs() const override { return MHWRender::kDirectX11; }
	MStatus setup( const MString & destination ) override;
	MStatus cleanup() override;
	MString uiName() const override { return _UIName; }
	const MString& panelName() const { return _PanelName; }

	// Custom Render Func
	void InitRTs();
	void CleanRTs();
	void UpdateRTs();
	MHWRender::MRenderTarget* const* grTargetOverrideList(unsigned int& listSize);
	MHWRender::MRenderTarget* grColorRT() { return _RTs[0]; }
	MHWRender::MRenderTarget* grDepthRT() { return _RTs[1]; }
	bool grRTsValid() { return _RTs[0] && _RTs[1]; }
	DxManager* Dx() { return dx; }

protected:
	MString _UIName;
	MString _PanelName;
	MHWRender::MRenderTargetDescription* _Desc[2];
	MHWRender::MRenderTarget* _RTs[2];

	DxManager* dx = nullptr;
};


class GarlandUserOperation
{
public:
	GarlandUserOperation(GarlandRenderOverride* gr) { _gr = gr; }
	~GarlandUserOperation() { _gr = nullptr; }

protected:
	GarlandRenderOverride* _gr = nullptr;

};


template<class _Ty>
class SimpleOverrideClass : public _Ty, public GarlandUserOperation
{
public:
	SimpleOverrideClass(const MString& name, GarlandRenderOverride* gr) : _Ty(name), GarlandUserOperation(gr) {}
	SimpleOverrideClass(GarlandRenderOverride* gr) : GarlandUserOperation(gr) {}

	MHWRender::MRenderTarget* const* targetOverrideList(unsigned int& listSize) override
	{
		return _gr ? _gr->grTargetOverrideList(listSize) : nullptr;
	}
};


class CustomSceneRender : public SimpleOverrideClass<MHWRender::MUserRenderOperation>
{
public:
	CustomSceneRender(const MString& name, GarlandRenderOverride* gr);
	MStatus execute(const MHWRender::MDrawContext& drawContext) override;
};
