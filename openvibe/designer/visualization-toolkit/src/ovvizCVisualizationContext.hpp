#pragma once

#include <map>
#include <memory>

#include <openvibe/plugins/ovIPluginObjectDesc.h>
#include <visualization-toolkit/ovviz_all.h>
#include <gtk/gtk.h>
#include "ovvizIVisualizationManager.h"
#include "ovvizIVisualizationContext.h"

#define OVP_ClassId_Plugin_VisualizationCtx			OpenViBE::CIdentifier(0x05A7171D, 0x78E4FE3C)
#define OVP_ClassId_Plugin_VisualizationCtxDesc		OpenViBE::CIdentifier(0x35A11438, 0x764F72E8)

namespace OpenViBE {
namespace VisualizationToolkit {
/**
 * @brief The CVisualizationContext class is a singleton used for passing visualization related information between the application
 * and visualization plugins.
 */
class CVisualizationContext final : public IVisualizationContext
{
public:
	/**
	 * The release function is neutralized. The object is only allocated once in the descriptor as a unique_ptr
	 * and will be released at its destruction.
	 */
	void release() override { }

	bool setManager(IVisualizationManager* visualizationManager) override
	{
		m_VisualizationManager = visualizationManager;
		return true;
	}

	bool setWidget(Toolkit::TBoxAlgorithm<Plugins::IBoxAlgorithm>& box, GtkWidget* widget) override;

	bool setToolbar(Toolkit::TBoxAlgorithm<Plugins::IBoxAlgorithm>& box, GtkWidget* toolbarWidget) override;

	bool isDerivedFromClass(const CIdentifier& classIdentifier) const override
	{
		return ((classIdentifier == OVP_ClassId_Plugin_VisualizationCtx) || IVisualizationContext::isDerivedFromClass(classIdentifier));
	}

	CIdentifier getClassIdentifier() const override { return OVP_ClassId_Plugin_VisualizationCtx; }

	CVisualizationContext() = default;

private:
	IVisualizationManager* m_VisualizationManager = nullptr;
};

class CVisualizationContextDesc final : public Plugins::IPluginObjectDesc
{
public:
	CVisualizationContextDesc() : m_visualizationCtx(new CVisualizationContext()) {}

	void release() override { }

	CString getName() const override { return "Visualization Context"; }
	CString getAuthorName() const override { return "Jozef Legény"; }
	CString getAuthorCompanyName() const override { return "Mensia Technologies"; }
	CString getShortDescription() const override { return ""; }
	CString getDetailedDescription() const override { return ""; }
	CString getCategory() const override { return ""; }
	CString getVersion() const override { return "1.0"; }

	CIdentifier getCreatedClass() const override { return OVP_ClassId_Plugin_VisualizationCtx; }

	/**
	 * The create function usage is different from standard plugins. As we need to be able to pass data between
	 * the application and the plugins, we need a permanent object that can be accessed by both. We achieve this
	 * by saving the object within the plugin descriptor and returning the pointer to the same object to all
	 * plugins.
	 *
	 * @return The singleton visualizationContext object
	 */
	Plugins::IPluginObject* create() override { return m_visualizationCtx.get(); }

	bool isDerivedFromClass(const CIdentifier& classIdentifier) const override
	{
		return ((classIdentifier == OVP_ClassId_Plugin_VisualizationCtxDesc) || IPluginObjectDesc::isDerivedFromClass(classIdentifier));
	}

	CIdentifier getClassIdentifier() const override { return OVP_ClassId_Plugin_VisualizationCtxDesc; }

private:
	std::unique_ptr<CVisualizationContext> m_visualizationCtx;
};
}  // namespace VisualizationToolkit
}  // namespace OpenViBE
