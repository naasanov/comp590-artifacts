///-------------------------------------------------------------------------------------------------
/// 
/// \file CBoxAlgorithmClassifierAccuracyMeasure.cpp
/// \author Laurent Bonnet (INRIA/IRISA)
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

#if defined(TARGET_HAS_ThirdPartyGTK)

#include "CBoxAlgorithmClassifierAccuracyMeasure.hpp"

#include <sstream>
#include <iomanip>

namespace OpenViBE {
namespace Plugins {
namespace Evaluation {

static void ResetScoresButtonCB(GtkToolButton* /*button*/, gpointer data)
{
	for (auto& progress : static_cast<CBoxAlgorithmClassifierAccuracyMeasure*>(data)->m_ProgressBar) {
		progress.score        = 0;
		progress.nStimulation = 0;
	}
}

static void show_percentages_toggle_button_cb(GtkToggleToolButton* button, gpointer data) { static_cast<CBoxAlgorithmClassifierAccuracyMeasure*>(data)->m_ShowPercentages = (gtk_toggle_tool_button_get_active(button) ? true : false); }
static void show_scores_toggle_button_cb(GtkToggleToolButton* button, gpointer data) { static_cast<CBoxAlgorithmClassifierAccuracyMeasure*>(data)->m_ShowScores = (gtk_toggle_tool_button_get_active(button) ? true : false); }

bool CBoxAlgorithmClassifierAccuracyMeasure::initialize()
{
	const size_t nInput = this->getStaticBoxContext().getInputCount();

	m_ProgressBar.resize(getStaticBoxContext().getInputCount() - 1); //-1 because the first input is the target

	//classifier decoders
	for (size_t i = 1; i < nInput; ++i) {
		m_classifierStimDecoders.push_back(new Toolkit::TStimulationDecoder<CBoxAlgorithmClassifierAccuracyMeasure>());
		m_classifierStimDecoders.back()->initialize(*this, i);
	}

	m_targetStimDecoder.initialize(*this, 0);

	//widgets
	m_mainWidgetInterface = gtk_builder_new();
	gtk_builder_add_from_file(m_mainWidgetInterface,
							  Directories::getDataDir() + "/plugins/evaluation/openvibe-simple-visualization-ClassifierAccuracyMeasure.ui", nullptr);

	m_toolbarWidgetInterface = gtk_builder_new();
	gtk_builder_add_from_file(m_toolbarWidgetInterface,
							  Directories::getDataDir() + "/plugins/evaluation/openvibe-simple-visualization-ClassifierAccuracyMeasure.ui", nullptr);

	gtk_builder_connect_signals(m_mainWidgetInterface, nullptr);
	gtk_builder_connect_signals(m_toolbarWidgetInterface, nullptr);

	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "reset-score-button")), "clicked", G_CALLBACK(ResetScoresButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "show-percentages-toggle-button")), "toggled",
					 G_CALLBACK(show_percentages_toggle_button_cb), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "show-scores-toggle-button")), "toggled",
					 G_CALLBACK(show_scores_toggle_button_cb), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "classifier-accuracy-measure-toolbar")), "delete_event",
					 G_CALLBACK(gtk_widget_hide), nullptr);

	m_mainWidget    = GTK_WIDGET(gtk_builder_get_object(m_mainWidgetInterface, "classifier-accuracy-measure-table"));
	m_toolbarWidget = GTK_WIDGET(gtk_builder_get_object(m_toolbarWidgetInterface, "classifier-accuracy-measure-toolbar"));

	m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
	m_visualizationCtx->setWidget(*this, m_mainWidget);
	m_visualizationCtx->setToolbar(*this, m_toolbarWidget);

	m_ShowPercentages = (gtk_toggle_tool_button_get_active(
							 GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_toolbarWidgetInterface, "show-percentages-toggle-button"))) ? true : false);
	m_ShowScores = (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_toolbarWidgetInterface, "show-scores-toggle-button")))
						? true : false);

	return true;
}

bool CBoxAlgorithmClassifierAccuracyMeasure::uninitialize()
{
	const size_t nInput = this->getStaticBoxContext().getInputCount();
	//decoders
	for (size_t i = 0; i < nInput - 1; ++i) {
		m_classifierStimDecoders[i]->uninitialize();
		delete m_classifierStimDecoders[i];
	}
	m_classifierStimDecoders.clear();

	m_targetStimDecoder.uninitialize();

	//widgets
	g_object_unref(m_toolbarWidgetInterface);
	m_toolbarWidgetInterface = nullptr;

	g_object_unref(m_mainWidgetInterface);
	m_mainWidgetInterface = nullptr;

	if (m_visualizationCtx) {
		this->releasePluginObject(m_visualizationCtx);
		m_visualizationCtx = nullptr;
	}

	return true;
}

bool CBoxAlgorithmClassifierAccuracyMeasure::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();

	return true;
}

bool CBoxAlgorithmClassifierAccuracyMeasure::process()
{
	Kernel::IBoxIO& boxContext           = this->getDynamicBoxContext();
	const Kernel::IBox& staticBoxContext = this->getStaticBoxContext();
	const size_t nInput                  = staticBoxContext.getInputCount();

	//input chunk 0 = targets
	// we iterate over the "target" chunks and update the timeline
	for (size_t i = 0; i < boxContext.getInputChunkCount(0); ++i) {
		m_targetStimDecoder.decode(i);

		if (m_targetStimDecoder.isHeaderReceived()) {
			//header received
			//adding the progress bars to the window
			GtkTable* table = GTK_TABLE(gtk_builder_get_object(m_mainWidgetInterface, "classifier-accuracy-measure-table"));
			gtk_table_resize(table, 1, guint(nInput - 1));

			//@TODO i variable redefine replace alll i in the loop ( it's logical but must be verified
			for (guint j = 0; j < nInput - 1; ++j) {
				GtkBuilder* builderBar = gtk_builder_new();
				gtk_builder_add_from_file(
					builderBar, Directories::getDataDir() + "/plugins/evaluation/openvibe-simple-visualization-ClassifierAccuracyMeasure.ui", nullptr);

				GtkBuilder* builderLabel = gtk_builder_new();
				gtk_builder_add_from_file(
					builderLabel, Directories::getDataDir() + "/plugins/evaluation/openvibe-simple-visualization-ClassifierAccuracyMeasure.ui", nullptr);

				GtkWidget* bar   = GTK_WIDGET(gtk_builder_get_object(builderBar, "progress-bar-classifier-accuracy"));
				GtkWidget* label = GTK_WIDGET(gtk_builder_get_object(builderLabel, "label-classifier-name"));

				gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(bar)), bar);
				gtk_table_attach(table, bar, j, j + 1, 0, 6, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
				gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(label)), label);
				gtk_table_attach(table, label, j, j + 1, 6, 7, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);

				g_object_unref(builderBar);
				g_object_unref(builderLabel);

				progress_bar_t progressBar;
				progressBar.progressBar     = GTK_PROGRESS_BAR(bar);
				progressBar.score           = 0;
				progressBar.nStimulation    = 0;
				progressBar.labelClassifier = GTK_LABEL(label);

				gtk_progress_bar_set_fraction(progressBar.progressBar, 0);
				CString inputName;
				staticBoxContext.getInputName(j + 1, inputName);
				gtk_progress_bar_set_text(progressBar.progressBar, inputName.toASCIIString());
				gtk_label_set_text(progressBar.labelClassifier, inputName.toASCIIString());
				m_ProgressBar[j] = (progressBar);
			}

			m_currentProcessingTimeLimit = 0;
		}

		if (m_targetStimDecoder.isBufferReceived()) {
			//buffer received
			//A new target comes, let's update the timeline with it
			const CStimulationSet* dstStimSet = m_targetStimDecoder.getOutputStimulationSet();
			for (size_t s = 0; s < dstStimSet->size(); ++s) {
				const uint64_t id   = dstStimSet->getId(s);
				const uint64_t date = dstStimSet->getDate(s);
				m_targetsTimeLines.insert(std::pair<uint64_t, uint64_t>(date, id));
				getLogManager() << Kernel::LogLevel_Trace << "New target inserted (" << id << "," << CTime(date) << ")\n";
			}

			//we updtae the time limit for processing classifier stim
			const uint64_t chunkEndTime  = boxContext.getInputChunkEndTime(0, i);
			m_currentProcessingTimeLimit = MAX(chunkEndTime, m_currentProcessingTimeLimit);
		}

		if (m_targetStimDecoder.isEndReceived()) { }

		boxContext.markInputAsDeprecated(0, i);
	}

	//input index 1-n = n classifier results
	for (size_t ip = 1; ip < staticBoxContext.getInputCount(); ++ip) {
		for (size_t i = 0; i < boxContext.getInputChunkCount(ip); ++i) {
			// lets get the chunck end time
			const uint64_t chunkEndTime = boxContext.getInputChunkEndTime(ip, i);
			// if the incoming chunk is in the timeline
			if (chunkEndTime <= m_currentProcessingTimeLimit) {
				if (!m_targetsTimeLines.empty()) {
					// we can process it
					m_classifierStimDecoders[ip - 1]->decode(i);

					if (m_classifierStimDecoders[ip - 1]->isHeaderReceived()) { } //header received
					if (m_classifierStimDecoders[ip - 1]->isBufferReceived()) {
						//buffer received
						const CStimulationSet* stimSet = m_classifierStimDecoders[ip - 1]->getOutputStimulationSet();
						for (size_t s = 0; s < stimSet->size(); ++s) {
							//We need to locate the stimulation on the timeline
							uint64_t id         = stimSet->getId(s);
							const uint64_t date = stimSet->getDate(s);

							getLogManager() << Kernel::LogLevel_Trace << "New Classifier state received (" << id << "," << CTime(date) << ") from Classifier " << ip << "\n";

							auto it   = m_targetsTimeLines.begin();
							bool cont = true;
							while (it != m_targetsTimeLines.end() && cont) {
								auto nextTarget = it;
								++nextTarget;
								if ((nextTarget == m_targetsTimeLines.end() || date < nextTarget->first) && date > it->first) {
									if (id == it->second) {
										//+1 for this classifier !
										m_ProgressBar[ip - 1].score++;
									}
									m_ProgressBar[ip - 1].nStimulation++;
									cont = false;
								}
								++it;
							}

							//auto it = m_targetsTimeLines.lower_bound(l_stimulationFromClassifierDate);
						}

						std::stringstream ss;
						ss << std::fixed;
						ss << std::setprecision(2);
						if (m_ShowScores) { ss << "score : " << m_ProgressBar[ip - 1].score << "/" << m_ProgressBar[ip - 1].nStimulation << "\n"; }
						double percent = 0.0;
						if (m_ProgressBar[ip - 1].nStimulation != 0) { percent = m_ProgressBar[ip - 1].score * 1. / m_ProgressBar[ip - 1].nStimulation; }
						if (m_ShowPercentages) { ss << percent * 100 << "%\n"; }

						gtk_progress_bar_set_fraction(m_ProgressBar[ip - 1].progressBar, percent);
						gtk_progress_bar_set_text(m_ProgressBar[ip - 1].progressBar, ss.str().c_str());
					}

					if (m_targetStimDecoder.isEndReceived()) { }
				}

				boxContext.markInputAsDeprecated(ip, i);
			}
		}
	}
	return true;
}

}  // namespace Evaluation
}  // namespace Plugins
}  // namespace OpenViBE
#endif
