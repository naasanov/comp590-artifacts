///-------------------------------------------------------------------------------------------------
/// 
/// \file CBoxAlgorithmROCCurve.hpp
/// \author Serrière Guillaume (Inria)
/// \version 1.0.
/// \date 28/05/2015.
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

#if defined(TARGET_HAS_ThirdPartyGTK)

#include "../defines.hpp"

#include <openvibe/ov_all.h>
#include <toolkit/ovtk_all.h>

#include "CROCCurveDraw.hpp"

#include <gtk/gtk.h>

#include <set>
#include <iostream>
#include <sstream>

#include <visualization-toolkit/ovviz_all.h>

namespace OpenViBE {
namespace Plugins {
namespace Evaluation {
typedef std::pair<uint64_t, uint64_t> CTimestampLabelPair;
typedef std::pair<uint64_t, double*> CTimestampValuesPair;
typedef std::pair<uint64_t, double*> CLabelValuesPair;

typedef std::pair<bool, double> CRocPairValue;

/// <summary> The class CBoxAlgorithmKappaCoef describes the box Kappa coefficient. </summary>
///
/// The roc curve is a graphical plot that represents the performance of a classifier.
/// This curve is created by plotting the true positive rate against the false positive rate at various threshold settings.
class CBoxAlgorithmROCCurve final : virtual public Toolkit::TBoxAlgorithm<IBoxAlgorithm>
{
public:
	void release() override { delete this; }

	bool initialize() override;
	bool uninitialize() override;
	bool processInput(const size_t index) override;

	bool process() override;


	_IsDerivedFromClass_Final_(Toolkit::TBoxAlgorithm<IBoxAlgorithm>, Box_ROCCurve)

private:
	bool computeROCCurves();
	void computeOneROCCurve(const CIdentifier& classID, const size_t classIdx) const;

	// Input decoder:
	Toolkit::TStimulationDecoder<CBoxAlgorithmROCCurve> m_expectedDecoder;
	Toolkit::TStreamedMatrixDecoder<CBoxAlgorithmROCCurve> m_classificationDecoder;

	std::set<CIdentifier> m_classStimSet;
	CIdentifier m_computationTrigger = CIdentifier::undefined();

	std::vector<CTimestampLabelPair> m_stimTimeline;
	std::vector<CTimestampValuesPair> m_valueTimeline;

	std::vector<CLabelValuesPair> m_labelValueList;

	//Display section
	GtkWidget* m_widget = nullptr;
	std::vector<CROCCurveDraw*> m_drawerList;

	VisualizationToolkit::IVisualizationContext* m_visualizationCtx = nullptr;
};

// The box listener can be used to call specific callbacks whenever the box structure changes : input added, name changed, etc.
// Please uncomment below the callbacks you want to use.
class CBoxAlgorithmROCCurveListener final : public Toolkit::TBoxListener<IBoxListener>
{
public:
	bool onSettingValueChanged(Kernel::IBox& box, const size_t index) override
	{
		if (index == 1) {
			CString nClass;
			box.getSettingValue(index, nClass);
			//Could happen if we rewritte a number
			if (nClass.length() == 0) { return true; }

			size_t nSetting;
			std::stringstream ss(nClass.toASCIIString());
			ss >> nSetting;

			//First of all we prevent for the value to goes under 1.
			if (nSetting < 1) {
				box.setSettingValue(index, "1");
				nSetting = 1;
			}

			size_t nCurrent = box.getSettingCount() - 2;
			//We have two choice 1/We need to add class, 2/We need to remove some
			if (nCurrent < nSetting) {
				while (nCurrent < nSetting) {
					box.addSetting(("Class " + std::to_string(nCurrent + 1) + " identifier").c_str(), OVTK_TypeId_Stimulation, "");
					++nCurrent;
				}
			}
			else {
				while (nCurrent > nSetting) {
					box.removeSetting(box.getSettingCount() - 1);
					--nCurrent;
				}
			}
		}
		return true;
	}

	_IsDerivedFromClass_Final_(Toolkit::TBoxListener<IBoxListener>, CIdentifier::undefined())
};

/// <summary> Descriptor of the box ROC curve. </summary>
class CBoxAlgorithmROCCurveDesc final : virtual public IBoxAlgorithmDesc
{
public:
	void release() override { }

	CString getName() const override { return "ROC curve"; }
	CString getAuthorName() const override { return "Serrière Guillaume"; }
	CString getAuthorCompanyName() const override { return "Inria"; }
	CString getShortDescription() const override { return "Compute the ROC curve for each class."; }
	CString getDetailedDescription() const override { return "The box computes the ROC curve for each class."; }
	CString getCategory() const override { return "Evaluation/Classification"; }
	CString getVersion() const override { return "0.1"; }
	CString getStockItemName() const override { return "gtk-yes"; }

	CIdentifier getCreatedClass() const override { return Box_ROCCurve; }
	IPluginObject* create() override { return new CBoxAlgorithmROCCurve; }

	IBoxListener* createBoxListener() const override { return new CBoxAlgorithmROCCurveListener; }
	void releaseBoxListener(IBoxListener* listener) const override { delete listener; }

	bool hasFunctionality(const EPluginFunctionality functionality) const override { return functionality == EPluginFunctionality::Visualization; }

	bool getBoxPrototype(Kernel::IBoxProto& prototype) const override
	{
		prototype.addInput("Expected labels", OV_TypeId_Stimulations);
		prototype.addInput("Probability values", OV_TypeId_StreamedMatrix);

		prototype.addSetting("Computation trigger", OV_TypeId_Stimulation, "OVTK_StimulationId_ExperimentStop");
		prototype.addSetting("Number of classes", OV_TypeId_Integer, "2");
		prototype.addSetting("Class 1 identifier", OV_TypeId_Stimulation, "OVTK_StimulationId_Label_01");
		prototype.addSetting("Class 2 identifier", OV_TypeId_Stimulation, "OVTK_StimulationId_Label_02");

		prototype.addFlag(Kernel::BoxFlag_CanModifySetting);

		prototype.addFlag(OV_AttributeId_Box_FlagIsUnstable);

		return true;
	}

	_IsDerivedFromClass_Final_(IBoxAlgorithmDesc, Box_ROCCurveDesc)
};
}  // namespace Evaluation
}  // namespace Plugins
}  // namespace OpenViBE

#endif // TARGET_HAS_ThirdPartyGTK
