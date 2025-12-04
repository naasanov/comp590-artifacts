///-------------------------------------------------------------------------------------------------
/// 
/// \file CAlgorithmLevelMeasure.cpp
/// \brief Classes implementation for the algorithm Level Measure.
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

#include "CAlgorithmLevelMeasure.hpp"
#include <iomanip>
#include <sstream>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

static void ResetScoresButtonCB(GtkToolButton* /*button*/, gpointer data)
{
	auto* levelMeasure = reinterpret_cast<CAlgorithmLevelMeasure*>(data);
	for (auto& i : levelMeasure->m_ProgressBar) { i.score = 0; }
}

static void ThresholdSpinbuttonCB(GtkSpinButton* button, gpointer data)
{
	auto* levelMeasure        = reinterpret_cast<CAlgorithmLevelMeasure*>(data);
	levelMeasure->m_Threshold = .01 * gtk_spin_button_get_value(button);
}

static void ShowPercentagesToggleButtonCB(GtkToggleToolButton* button, gpointer data)
{
	auto* levelMeasure              = reinterpret_cast<CAlgorithmLevelMeasure*>(data);
	levelMeasure->m_ShowPercentages = (gtk_toggle_tool_button_get_active(button) != 0);
}

bool CAlgorithmLevelMeasure::initialize()
{
	m_ipMatrix.initialize(getInputParameter(LevelMeasure_InputParameterId_Matrix));

	m_opMainWidget.initialize(getOutputParameter(LevelMeasure_OutputParameterId_MainWidget));
	m_opToolbarWidget.initialize(getOutputParameter(LevelMeasure_OutputParameterId_ToolbarWidget));

	m_mainWidgetInterface = gtk_builder_new();
	gtk_builder_add_from_file(m_mainWidgetInterface, Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-LevelMeasure.ui",
							  nullptr);

	m_toolbarWidgetInterface = gtk_builder_new();
	gtk_builder_add_from_file(m_toolbarWidgetInterface,
							  Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-LevelMeasure.ui", nullptr);

	gtk_builder_connect_signals(m_mainWidgetInterface, nullptr);
	gtk_builder_connect_signals(m_toolbarWidgetInterface, nullptr);

	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "reset-score-button")), "clicked", G_CALLBACK(ResetScoresButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "show-percentages-toggle-button")), "toggled",
					 G_CALLBACK(ShowPercentagesToggleButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "threshold-spinbutton")), "value-changed", G_CALLBACK(ThresholdSpinbuttonCB),
					 this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "level-measure-toolbar")), "delete_event", G_CALLBACK(gtk_widget_hide), nullptr);

	m_mainWindow    = GTK_WIDGET(gtk_builder_get_object(m_mainWidgetInterface, "level-measure-table"));
	m_toolbarWidget = GTK_WIDGET(gtk_builder_get_object(m_toolbarWidgetInterface, "level-measure-toolbar"));

	m_ShowPercentages = (gtk_toggle_tool_button_get_active(
							 GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_toolbarWidgetInterface, "show-percentages-toggle-button"))) != 0);
	m_Threshold = .01 * gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(m_toolbarWidgetInterface, "threshold-spinbutton")));

	return true;
}

bool CAlgorithmLevelMeasure::uninitialize()
{
	g_object_unref(m_toolbarWidgetInterface);
	m_toolbarWidgetInterface = nullptr;

	g_object_unref(m_mainWidgetInterface);
	m_mainWidgetInterface = nullptr;

	m_opToolbarWidget.uninitialize();
	m_opMainWidget.uninitialize();

	m_ipMatrix.uninitialize();

	return true;
}

bool CAlgorithmLevelMeasure::process()
{
	if (this->isInputTriggerActive(LevelMeasure_InputTriggerId_Reset)) {
		if (m_ipMatrix->getDimensionCount() != 1 && m_ipMatrix->getDimensionCount() != 2) {
			getLogManager() << Kernel::LogLevel_Warning << "Input matrix does not have 1 or 2 dimensions (" << m_ipMatrix->getDimensionCount() << ")\n";
			return false;
		}

		const guint nRow = guint(m_ipMatrix->getDimensionCount() == 2 ? m_ipMatrix->getDimensionSize(0) : 1);
		const guint nCol = guint(m_ipMatrix->getDimensionCount() == 2 ? m_ipMatrix->getDimensionSize(1) : m_ipMatrix->getDimensionSize(0));

		GtkTable* table = GTK_TABLE(gtk_builder_get_object(m_mainWidgetInterface, "level-measure-table"));
		gtk_table_resize(table, nRow, nCol);

		for (guint i = 0; i < nRow; ++i) {
			for (guint j = 0; j < nCol; ++j) {
				GtkBuilder* gtkBuilder = gtk_builder_new();
				gtk_builder_add_from_file(gtkBuilder, Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-LevelMeasure.ui",
										  nullptr);

				GtkWidget* widget = GTK_WIDGET(gtk_builder_get_object(gtkBuilder, "progress-bar-level"));
				gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(widget)), widget);
				gtk_table_attach(table, widget, j, j + 1, i, i + 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
				g_object_unref(gtkBuilder);

				progress_bar_t bar;
				bar.bar                  = GTK_PROGRESS_BAR(widget);
				bar.score                = 0;
				bar.lastWasOverThreshold = false;
				m_ProgressBar.push_back(bar);
			}
		}

		m_opMainWidget    = m_mainWindow;
		m_opToolbarWidget = m_toolbarWidget;
	}

	if (this->isInputTriggerActive(LevelMeasure_InputTriggerId_Refresh)) {
		auto it         = m_ProgressBar.begin();
		double* iBuffer = m_ipMatrix->getBuffer();

		size_t n = m_ipMatrix->getBufferElementCount();
		while (n--) {
			double percent = *iBuffer;
			if (percent > 1) { percent = 1; }
			if (percent < 0) { percent = 0; }

			if (percent > m_Threshold && !it->lastWasOverThreshold) {
				it->score++;
				it->lastWasOverThreshold = true;
			}
			if (percent <= m_Threshold) { it->lastWasOverThreshold = false; }

			std::stringstream ss;
			ss << std::fixed << std::setprecision(2) << "score : " << it->score << "\n";

			if (m_ShowPercentages) { ss << "level : " << percent * 100 << "%\n"; }

			gtk_progress_bar_set_fraction(it->bar, percent);
			gtk_progress_bar_set_text(it->bar, ss.str().c_str());

			iBuffer++;
			++it;
		}

		this->activateOutputTrigger(LevelMeasure_OutputTriggerId_Refreshed, true);
	}

	return true;
}
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
