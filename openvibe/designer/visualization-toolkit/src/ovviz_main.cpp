#include "visualization-toolkit/ovviz_all.h"
#include "ovvizCVisualizationContext.hpp"

namespace OpenViBE {

namespace {
VisualizationToolkit::CVisualizationContextDesc visualizationContextDesc;
}  // namespace

bool VisualizationToolkit::initialize(const Kernel::IKernelContext& ctx)
{
	Kernel::ITypeManager& typeManager     = ctx.getTypeManager();
	Kernel::IPluginManager& pluginManager = ctx.getPluginManager();

	typeManager.registerType(OV_TypeId_Color, "Color");
	typeManager.registerType(OV_TypeId_ColorGradient, "Color Gradient");

	pluginManager.registerPluginDesc(visualizationContextDesc);

	return true;
}

bool VisualizationToolkit::uninitialize(const Kernel::IKernelContext& /*ctx*/) { return true; }

}  // namespace OpenViBE
