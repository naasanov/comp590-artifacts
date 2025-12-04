///-------------------------------------------------------------------------------------------------
/// 
/// \file CBoxAlgorithmLevelMeasure.hpp
/// \brief Classes for the Box Level measure.
/// \author Yann Renard (INRIA/IRISA).
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
#include <visualization-toolkit/ovviz_all.h>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
class CBoxAlgorithmLevelMeasure final : public Toolkit::TBoxAlgorithm<IBoxAlgorithm>
{
public:
	void release() override { delete this; }

	bool initialize() override;
	bool uninitialize() override;
	bool processInput(const size_t index) override;
	bool process() override;

	_IsDerivedFromClass_Final_(Toolkit::TBoxAlgorithm<IBoxAlgorithm>, Box_LevelMeasure)

protected:
	Kernel::IAlgorithmProxy* m_matrixDecoder = nullptr;
	Kernel::IAlgorithmProxy* m_levelMeasure  = nullptr;

	Kernel::TParameterHandler<const CMemoryBuffer*> m_matrixBuffer;
	Kernel::TParameterHandler<CMatrix*> m_matrixHandler;

	Kernel::TParameterHandler<CMatrix*> m_levelMeasureMatrix;
	Kernel::TParameterHandler<GtkWidget*> m_levelMeasureMainWidget;
	Kernel::TParameterHandler<GtkWidget*> m_levelMeasureToolbarWidget;

	CMatrix* m_matrix = nullptr;

private:
	VisualizationToolkit::IVisualizationContext* m_visualizationCtx = nullptr;
};

class CBoxAlgorithmLevelMeasureDesc final : public IBoxAlgorithmDesc
{
public:
	void release() override { }

	CString getName() const override { return "Level measure"; }
	CString getAuthorName() const override { return "Yann Renard"; }
	CString getAuthorCompanyName() const override { return "INRIA/IRISA"; }
	CString getShortDescription() const override { return ""; }
	CString getDetailedDescription() const override { return ""; }
	CString getCategory() const override { return "Visualization/Basic"; }
	CString getVersion() const override { return "1.0"; }
	CString getStockItemName() const override { return "gtk-go-up"; }

	CIdentifier getCreatedClass() const override { return Box_LevelMeasure; }
	IPluginObject* create() override { return new CBoxAlgorithmLevelMeasure; }

	bool hasFunctionality(const EPluginFunctionality functionality) const override { return functionality == EPluginFunctionality::Visualization; }

	bool getBoxPrototype(Kernel::IBoxProto& prototype) const override
	{
		prototype.addInput("Input matrix to display", OV_TypeId_StreamedMatrix);
		return true;
	}

	_IsDerivedFromClass_Final_(IBoxAlgorithmDesc, Box_LevelMeasureDesc)
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
