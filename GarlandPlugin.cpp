#include <stdio.h>
#include <maya/MString.h>
#include <maya/MFnPlugin.h>
#include <maya/MViewport2Renderer.h>
#include "GarlandRender.h"


MStatus initializePlugin(MObject obj)
{
	MStatus status;
	MFnPlugin plugin(obj, "JIAXI_LIU", "0.2", "Any");

	MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
	if (renderer)
	{
		GarlandRenderOverride*overridePtr = new GarlandRenderOverride("GarlandViewport");
		if (overridePtr)
		{
			renderer->registerOverride(overridePtr);
		}
	}

	return status;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus status;
	MFnPlugin plugin(obj);

	MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
	if (renderer)
	{
		// Find override with the given name and deregister
		const MHWRender::MRenderOverride* overridePtr = renderer->findRenderOverride("GarlandViewport");
		if (overridePtr)
		{
			renderer->deregisterOverride(overridePtr);
			delete overridePtr;
		}
	}

	return status;
}
