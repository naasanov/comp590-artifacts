#pragma once

#include "CPluginObjectDescEnum.hpp"

#include <map>
#include <vector>
#include <string>

#include <gtk/gtk.h>

namespace OpenViBE {
namespace PluginInspector {
// ------------------------------------------------------------------------------------------------------------------------------------
class CAlgorithmSnapshotGen final : public CPluginObjectDescEnum
{
public:
	CAlgorithmSnapshotGen(const Kernel::IKernelContext& ctx, const std::string& snapshotDir, const std::string& docTemplateDir);
	~CAlgorithmSnapshotGen() override;
	bool Callback(const Plugins::IPluginObjectDesc& pod) override;

protected:
	std::string m_snapshotDir;
	std::string m_docTemplateDir;
	std::vector<std::pair<std::string, std::string>> m_categories;
	std::map<EColors, GdkColor> m_colors;
	GtkWidget* m_window = nullptr;
	GtkWidget* m_widget = nullptr;
};
}  // namespace PluginInspector
}  // namespace OpenViBE
