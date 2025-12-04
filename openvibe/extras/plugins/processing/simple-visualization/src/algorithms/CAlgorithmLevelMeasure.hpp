///-------------------------------------------------------------------------------------------------
/// 
/// \file CAlgorithmLevelMeasure.hpp
/// \brief Classes for the algorithm Level Measure.
/// \author Yann Renard (Inria).
/// \version 1.0.
/// \copyright Copyright (C) 2022 Inria
///
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as published
/// by the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU Affero General Public License for more details.
///
/// You should have received a copy of the GNU Affero General Public License
/// along with this program.  If not, see <https://www.gnu.org/licenses/>.
/// 
///-------------------------------------------------------------------------------------------------

#pragma once

#include "../defines.hpp"
#include <openvibe/ov_all.h>
#include <toolkit/ovtk_all.h>

#include <gtk/gtk.h>
#include <map>
#include <vector>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
class CAlgorithmLevelMeasure final : public Toolkit::TAlgorithm<IAlgorithm>
{
public:
	void release() override { delete this; }

	bool initialize() override;
	bool uninitialize() override;
	bool process() override;

	_IsDerivedFromClass_Final_(Toolkit::TAlgorithm<IAlgorithm>, Algorithm_LevelMeasure)

protected:
	Kernel::TParameterHandler<CMatrix*> m_ipMatrix;
	Kernel::TParameterHandler<GtkWidget*> m_opMainWidget;
	Kernel::TParameterHandler<GtkWidget*> m_opToolbarWidget;

	GtkBuilder* m_mainWidgetInterface    = nullptr;
	GtkBuilder* m_toolbarWidgetInterface = nullptr;
	GtkWidget* m_mainWindow              = nullptr;
	GtkWidget* m_toolbarWidget           = nullptr;

public:
	using progress_bar_t = struct
	{
		GtkProgressBar* bar;
		size_t score;
		bool lastWasOverThreshold;
	};

	std::vector<progress_bar_t> m_ProgressBar;
	bool m_ShowPercentages = false;
	double m_Threshold     = 0.1;
};

class CAlgorithmLevelMeasureDesc final : public IAlgorithmDesc
{
public:
	void release() override { }

	CString getName() const override { return "Level measure"; }
	CString getAuthorName() const override { return "Yann Renard"; }
	CString getAuthorCompanyName() const override { return "INRIA/IRISA"; }
	CString getShortDescription() const override { return "Displays sample chunk of each channel as a row of progress bars"; }
	CString getDetailedDescription() const override { return "Another way to look at it: Each displayed row is a histogram normalized to sum to 1"; }
	CString getCategory() const override { return "Simple visualization"; }
	CString getVersion() const override { return "1.0"; }

	CIdentifier getCreatedClass() const override { return Algorithm_LevelMeasure; }
	IPluginObject* create() override { return new CAlgorithmLevelMeasure; }

	bool getAlgorithmPrototype(Kernel::IAlgorithmProto& prototype) const override
	{
		prototype.addInputParameter(LevelMeasure_InputParameterId_Matrix, "Matrix", Kernel::ParameterType_Matrix);
		prototype.addOutputParameter(LevelMeasure_OutputParameterId_MainWidget, "Main widget", Kernel::ParameterType_Pointer);
		prototype.addOutputParameter(LevelMeasure_OutputParameterId_ToolbarWidget, "Toolbar widget", Kernel::ParameterType_Pointer);
		prototype.addInputTrigger(LevelMeasure_InputTriggerId_Reset, "Reset");
		prototype.addInputTrigger(LevelMeasure_InputTriggerId_Refresh, "Refresh");
		prototype.addOutputTrigger(LevelMeasure_OutputTriggerId_Refreshed, "Refreshed");

		return true;
	}

	_IsDerivedFromClass_Final_(IAlgorithmDesc, Algorithm_LevelMeasureDesc)
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
