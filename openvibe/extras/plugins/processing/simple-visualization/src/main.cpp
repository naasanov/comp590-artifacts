///-------------------------------------------------------------------------------------------------
/// 
/// \file main.cpp
/// \brief Main file for simple visualization plugin, registering the boxes to OpenViBE
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

#include "defines.hpp"
#include "algorithms/CAlgorithmLevelMeasure.hpp"

//Presentation
#include "box-algorithms/CBoxAlgorithmP300IdentifierCardVisualization.hpp"
#include "box-algorithms/CBoxAlgorithmP300MagicCardVisualization.hpp"
#include "box-algorithms/CBoxAlgorithmP300SpellerVisualization.hpp"
#include "box-algorithms/CDisplayCueImage.hpp"
#include "box-algorithms/CGrazMultiVisualization.hpp"
#include "box-algorithms/CGrazVisualization.hpp"

//2D plugins
#include "box-algorithms/CBoxAlgorithmErpPlot.hpp"
#include "box-algorithms/CBoxAlgorithmLevelMeasure.hpp"
#include "box-algorithms/CSignalDisplay.hpp"

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

OVP_Declare_Begin()
	context.getTypeManager().registerEnumerationEntry(OV_TypeId_BoxAlgorithmFlag, OV_AttributeId_Box_FlagIsUnstable.toString(),
													  OV_AttributeId_Box_FlagIsUnstable.id());

	context.getTypeManager().registerEnumerationType(TypeId_SphericalLinearInterpolationType, "Spherical linear interpolation type");
	context.getTypeManager().registerEnumerationEntry(TypeId_SphericalLinearInterpolationType, "Spline (potentials)", Spline);
	context.getTypeManager().registerEnumerationEntry(TypeId_SphericalLinearInterpolationType, "Spline laplacian (currents)", Laplacian);

	context.getTypeManager().registerEnumerationType(TypeId_SignalDisplayMode, "Signal display mode");
	context.getTypeManager().registerEnumerationEntry(TypeId_SignalDisplayMode, "Scroll", Scroll);
	context.getTypeManager().registerEnumerationEntry(TypeId_SignalDisplayMode, "Scan", Scan);

	context.getTypeManager().registerEnumerationType(TypeId_SignalDisplayScaling, "Signal display scaling");
	context.getTypeManager().registerEnumerationEntry(TypeId_SignalDisplayScaling, CSignalDisplayView::SCALING_MODES[PerChannel].c_str(), PerChannel);
	context.getTypeManager().registerEnumerationEntry(TypeId_SignalDisplayScaling, CSignalDisplayView::SCALING_MODES[Global].c_str(), Global);
	context.getTypeManager().registerEnumerationEntry(TypeId_SignalDisplayScaling, CSignalDisplayView::SCALING_MODES[None].c_str(), None);

	context.getTypeManager().registerEnumerationType(TypeId_FeedbackMode, "Feedback display mode");
	context.getTypeManager().registerEnumerationEntry(TypeId_FeedbackMode, "Positive Only", Positive);
	context.getTypeManager().registerEnumerationEntry(TypeId_FeedbackMode, "Best Only", Best);
	context.getTypeManager().registerEnumerationEntry(TypeId_FeedbackMode, "All", All);
	context.getTypeManager().registerEnumerationEntry(TypeId_FeedbackMode, "None", No);


	OVP_Declare_New(CDisplayCueImageDesc)
	OVP_Declare_New(CSignalDisplayDesc)
	OVP_Declare_New(CAlgorithmLevelMeasureDesc)

	OVP_Declare_New(CGrazVisualizationDesc)
	OVP_Declare_New(CGrazMultiVisualizationDesc)

	OVP_Declare_New(CBoxAlgorithmP300SpellerVisualizationDesc)
	OVP_Declare_New(CBoxAlgorithmP300IdentifierCardVisualizationDesc)
	OVP_Declare_New(CBoxAlgorithmP300MagicCardVisualizationDesc)

	OVP_Declare_New(CBoxAlgorithmLevelMeasureDesc)
	OVP_Declare_New(CBoxAlgorithmErpPlotDesc)

OVP_Declare_End()

}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
