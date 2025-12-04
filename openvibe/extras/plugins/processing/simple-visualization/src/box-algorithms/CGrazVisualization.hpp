///-------------------------------------------------------------------------------------------------
/// 
/// \file CGrazVisualization.hpp
/// \brief Classes for the Box Graz visualization.
/// \author Bruno Renier, Jussi T. Lindgren (INRIA/IRISA).
/// \version 0.2.
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

#include <gtk/gtk.h>
#include <deque>
#include <vector>
#include "../utils.hpp"

namespace TCPTagging {
class IStimulusSender; // fwd declare
}  // namespace TCPTagging

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
enum class EArrowDirections { None = 0, Left, Right, Up, Down };

enum class EStates { Idle, Reference, Cue, ContinousFeedback };

class CGrazVisualization final : public Toolkit::TBoxAlgorithm<IBoxAlgorithm>
{
public:
	void release() override { delete this; }

	bool initialize() override;
	bool uninitialize() override;
	bool processInput(const size_t index) override;
	bool process() override;

	void Redraw();
	void Resize(size_t width, size_t height);

	void FlushQueue();					// Sends all accumulated stimuli to the TCP Tagging

protected:
	void setStimulation(size_t index, uint64_t identifier, uint64_t date);
	void setMatrixBuffer(const double* buffer);
	void processState() const;

	void drawReferenceCross() const;
	void drawArrow(EArrowDirections direction);
	void drawBar() const;
	void drawAccuracy() const;
	void updateConfusionMatrix(double prediction);
	double aggregatePredictions(bool includeAll);

	_IsDerivedFromClass_Final_(Toolkit::TBoxAlgorithm<IBoxAlgorithm>, Box_GrazVisualization)

public:
	//! The Builder handler used to create the interface
	GtkBuilder* m_Builder    = nullptr;
	GtkWidget* m_MainWindow  = nullptr;
	GtkWidget* m_DrawingArea = nullptr;

	//ebml
	Toolkit::TStimulationDecoder<CGrazVisualization> m_StimulationDecoder;
	Toolkit::TStreamedMatrixDecoder<CGrazVisualization> m_MatrixDecoder;

	EStates m_CurrentState              = EStates::Idle;
	EArrowDirections m_CurrentDirection = EArrowDirections::None;

	double m_MaxAmplitude = -DBL_MAX;
	double m_BarScale     = 0.0;

	//Start and end time of the last buffer
	uint64_t m_StartTime = 0;
	uint64_t m_EndTime   = 0;

	bool m_TwoValueInput = false;

	GdkPixbuf* m_OriginalBar = nullptr;
	GdkPixbuf* m_LeftBar     = nullptr;
	GdkPixbuf* m_RightBar    = nullptr;

	GdkPixbuf* m_OriginalLeftArrow  = nullptr;
	GdkPixbuf* m_OriginalRightArrow = nullptr;
	GdkPixbuf* m_OriginalUpArrow    = nullptr;
	GdkPixbuf* m_OriginalDownArrow  = nullptr;

	GdkPixbuf* m_LeftArrow  = nullptr;
	GdkPixbuf* m_RightArrow = nullptr;
	GdkPixbuf* m_UpArrow    = nullptr;
	GdkPixbuf* m_DownArrow  = nullptr;

	GdkColor m_BackgroundColor = InitGDKColor(0, 0, 0, 0);
	GdkColor m_ForegroundColor = InitGDKColor(0, 0, 32768, 0);

	std::deque<double> m_Amplitudes; // predictions for the current trial

	bool m_ShowInstruction      = true;
	bool m_ShowFeedback         = false;
	bool m_DelayFeedback        = false;
	bool m_ShowAccuracy         = false;
	bool m_PositiveFeedbackOnly = false;

	uint64_t m_PredictionsToIntegrate = 5;

	// For queuing stimulations to the TCP Tagging
	std::vector<uint64_t> m_StimuliQueue;
	guint m_IdleFuncTag                           = 0;
	TCPTagging::IStimulusSender* m_StimulusSender = nullptr;

	uint64_t m_LastStimulation = 0;

private:
	VisualizationToolkit::IVisualizationContext* m_visualizationCtx = nullptr;

	Toolkit::TStreamedMatrixEncoder<CGrazVisualization> m_confusionEncoder;
	CMatrix* m_oConfusion = nullptr;	// Outpout confusion Matrix pointer
};

class CGrazVisualizationDesc final : public IBoxAlgorithmDesc
{
public:
	CString getName() const override { return "Graz visualization"; }
	CString getAuthorName() const override { return "Bruno Renier, Jussi T. Lindgren"; }
	CString getAuthorCompanyName() const override { return "INRIA/IRISA"; }
	CString getShortDescription() const override { return "Visualization plugin for the Graz experiment"; }
	CString getDetailedDescription() const override { return "Visualization/Feedback plugin for the Graz experiment"; }
	CString getCategory() const override { return "Visualization/Presentation"; }
	CString getVersion() const override { return "0.2"; }
	void release() override { }

	CIdentifier getCreatedClass() const override { return Box_GrazVisualization; }
	CString getStockItemName() const override { return "gtk-fullscreen"; }
	IPluginObject* create() override { return new CGrazVisualization(); }

	bool hasFunctionality(const EPluginFunctionality functionality) const override { return functionality == EPluginFunctionality::Visualization; }

	bool getBoxPrototype(Kernel::IBoxProto& prototype) const override
	{
		prototype.addInput("Stimulations", OV_TypeId_Stimulations);
		prototype.addInput("Amplitude", OV_TypeId_StreamedMatrix);

		prototype.addSetting("Show instruction", OV_TypeId_Boolean, "true");
		prototype.addSetting("Show feedback", OV_TypeId_Boolean, "false");
		prototype.addSetting("Delay feedback", OV_TypeId_Boolean, "false");
		prototype.addSetting("Show accuracy", OV_TypeId_Boolean, "false");
		prototype.addSetting("Predictions to integrate", OV_TypeId_Integer, "5");
		prototype.addSetting("Positive feedback only", OV_TypeId_Boolean, "false");

		prototype.addOutput("Confusion Matrix", OV_TypeId_StreamedMatrix);

		return true;
	}

	_IsDerivedFromClass_Final_(IBoxAlgorithmDesc, Box_GrazVisualizationDesc)
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
