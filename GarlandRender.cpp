#include "GarlandRender.h"
#include <maya/MRenderTargetManager.h>
#include <maya/MGlobal.h>
#include "DxManager.h"


GarlandRenderOverride::GarlandRenderOverride(const MString & name)
	: MRenderOverride(name) , _UIName("GarlandRender")
{
	InitRTs();
	_PanelName.clear();
	dx = new DxManager(this);
}

GarlandRenderOverride::~GarlandRenderOverride()
{
	CleanRTs();

	if (dx)
	{
		delete dx;
	}
}

MStatus GarlandRenderOverride::setup( const MString & destination )
{
	UpdateRTs();
	_PanelName.set(destination.asChar());
	return MRenderOverride::setup(destination);
}

MStatus GarlandRenderOverride::cleanup()
{
	_PanelName.clear();
	return MRenderOverride::cleanup();
}

void GarlandRenderOverride::InitRTs()
{
	_RTs[0] = nullptr;
	_RTs[1] = nullptr;
	_Desc[0] = nullptr;
	_Desc[1] = nullptr;

	MHWRender::MRenderer* theRenderer = MHWRender::MRenderer::theRenderer();
	if (!theRenderer)
		return;

	mOperations.clear();
	//Stanard Operations get from theRender: Background-Scene-HUD-Presesnt
	//We need to 
	//00.  Render Standard Background to RT
	//01.  Render Standard Scene to RT
	//02.  Render Custom Scene overlay to RT by UserOperation
	//03.  Render Standard HUD RT
	//04.  Standard Present

	auto grClear = new SimpleOverrideClass<MHWRender::MClearOperation>("GarlandClear", this);
	auto grScene = new SimpleOverrideClass<MHWRender::MSceneRender>("GarlandScene", this);
	CustomSceneRender* customScene = new CustomSceneRender("CustomScene", this);
	auto grHUD = new SimpleOverrideClass<MHWRender::MHUDRender>(this);
	auto grPresent = new SimpleOverrideClass<MHWRender::MPresentTarget>("GarlandPresent", this);

	mOperations.append(grClear);
	mOperations.append(grScene);
	mOperations.append(customScene);
	mOperations.append(grHUD);
	mOperations.append(grPresent);

	const MHWRender::MRenderTargetManager* targetManager = theRenderer ? theRenderer->getRenderTargetManager() : NULL;
	if (targetManager)
	{
		unsigned int targetWidth = 0;
		unsigned int targetHeight = 0;

		if (theRenderer)
		{
			theRenderer->outputTargetSize(targetWidth, targetHeight);
		}

		_Desc[0] = new MHWRender::MRenderTargetDescription(
			MString("__CustomColorTarget__"),
			targetWidth,
			targetHeight,
			1,
			MHWRender::kR8G8B8A8_UNORM,
			0,
			false);

		_RTs[0] = targetManager->acquireRenderTarget(*_Desc[0]);

		_Desc[1] = new MHWRender::MRenderTargetDescription(
			MString("__CustomDepthTarget__"),
			targetWidth,
			targetHeight,
			1,
			MHWRender::kD24S8,
			0,
			false);

		_RTs[1] = targetManager->acquireRenderTarget(*_Desc[1]);
	}

	if (!grRTsValid())
	{
		MGlobal::displayError("Get RenderTragets Fail !");
	}
}

void GarlandRenderOverride::CleanRTs()
{
	MHWRender::MRenderer* theRenderer = MHWRender::MRenderer::theRenderer();
	const MHWRender::MRenderTargetManager* targetManager = theRenderer ? theRenderer->getRenderTargetManager() : NULL;

	if (targetManager)
	{
		if (_RTs[0])
		{
			targetManager->releaseRenderTarget(_RTs[0]);
			_RTs[0] = nullptr;
		}

		if (_RTs[1])
		{
			targetManager->releaseRenderTarget(_RTs[1]);
			_RTs[1] = nullptr;
		}
	}
}

void GarlandRenderOverride::UpdateRTs()
{
	MHWRender::MRenderer* theRenderer = MHWRender::MRenderer::theRenderer();

	unsigned int targetWidth = 0;
	unsigned int targetHeight = 0;

	if (theRenderer)
	{
		theRenderer->outputTargetSize(targetWidth, targetHeight);

		if (_Desc[0]->width() != targetWidth || _Desc[1]->height() != targetHeight)
		{
			_Desc[0]->setWidth(targetWidth);
			_Desc[0]->setHeight(targetHeight);
			_Desc[1]->setWidth(targetWidth);
			_Desc[1]->setHeight(targetHeight);

			const MHWRender::MRenderTargetManager* targetManager = theRenderer ? theRenderer->getRenderTargetManager() : NULL;
			if (targetManager)
			{
				if (!_RTs[0])
				{
					_RTs[0] = targetManager->acquireRenderTarget(*(_Desc[0]));
				}
				else
				{
					_RTs[0]->updateDescription(*(_Desc[0]));
				}
				if (!_RTs[1])
				{
					_RTs[1] = targetManager->acquireRenderTarget(*(_Desc[1]));
				}
				else
				{
					_RTs[1]->updateDescription(*(_Desc[1]));
				}
			}
		}
	}
}

MHWRender::MRenderTarget* const* GarlandRenderOverride::grTargetOverrideList(unsigned int& listSize)
{
	if (_RTs[0] && _RTs[1])
	{
		listSize = 2;
		return _RTs;
	}
	return nullptr;
}


CustomSceneRender::CustomSceneRender(const MString& name, GarlandRenderOverride* gr)
	: SimpleOverrideClass<MHWRender::MUserRenderOperation>(name, gr)
{

}

MStatus CustomSceneRender::execute(const MHWRender::MDrawContext& drawContext)
{	
	_gr->Dx()->debug(drawContext);
	return MStatus::kSuccess;
}
