#include "CBoxAlgorithmMatrixDisplay.hpp"

#include <string>
#include <sstream>
#include <iomanip>

#include <cstdlib>
#include <cmath>
#include <visualization-toolkit/ovvizColorGradient.h>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

static void ShowValuesToggleButtonCB(GtkToggleToolButton* button, gpointer data)
{
	auto* display         = reinterpret_cast<CBoxAlgorithmMatrixDisplay*>(data);
	display->m_ShowValues = (gtk_toggle_tool_button_get_active(button) != 0);
}

static void ShowColorsToggleButtonCB(GtkToggleToolButton* button, gpointer data)
{
	auto* display         = reinterpret_cast<CBoxAlgorithmMatrixDisplay*>(data);
	display->m_ShowColors = (gtk_toggle_tool_button_get_active(button) != 0);
	display->ResetColors();
}

bool CBoxAlgorithmMatrixDisplay::ResetColors()

{
	if (m_ShowColors) {
		//we take colors from cache and re-put it in the table
		for (auto it = m_eventBoxCache.begin(); it != m_eventBoxCache.end(); ++it) { gtk_widget_modify_bg((*it).first, GTK_STATE_NORMAL, &(*it).second); }
	}
	else {
		for (auto it = m_eventBoxCache.begin(); it != m_eventBoxCache.end(); ++it) {
			GdkColor white;
			white.red   = 65535;
			white.green = 65535;
			white.blue  = 65535;
			gtk_widget_modify_bg((*it).first, GTK_STATE_NORMAL, &white);
		}
	}

	return true;
}

bool CBoxAlgorithmMatrixDisplay::initialize()

{
	//targets decoder
	m_iMatrix = &this->getAlgorithmManager().getAlgorithm(this->getAlgorithmManager().createAlgorithm(OVP_GD_ClassId_Algorithm_StreamedMatrixDecoder));
	m_iMatrix->initialize();

	//IO for the targets MemoryBuffer -> StreamedMatrix
	ip_buffer.initialize(m_iMatrix->getInputParameter(OVP_GD_Algorithm_StreamedMatrixDecoder_InputParameterId_MemoryBufferToDecode));
	op_matrix.initialize(m_iMatrix->getOutputParameter(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputParameterId_Matrix));

	//widgets
	m_mainWidgetInterface    = gtk_builder_new();
	m_toolbarWidgetInterface = gtk_builder_new();
	// glade_xml_new(Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-MatrixDisplay.ui", "matrix-display-table", nullptr);
	// glade_xml_new(Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-MatrixDisplay.ui", "matrix-display-toolbar", nullptr);
	gtk_builder_add_from_file(m_mainWidgetInterface, Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-MatrixDisplay.ui",
							  nullptr);
	gtk_builder_add_from_file(m_toolbarWidgetInterface,
							  Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-MatrixDisplay.ui", nullptr);

	gtk_builder_connect_signals(m_mainWidgetInterface, nullptr);
	gtk_builder_connect_signals(m_toolbarWidgetInterface, nullptr);

	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "show-values-toggle-button")), "toggled",
					 G_CALLBACK(ShowValuesToggleButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "show-colors-toggle-button")), "toggled",
					 G_CALLBACK(ShowColorsToggleButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_toolbarWidgetInterface, "matrix-display-toolbar")), "delete_event", G_CALLBACK(gtk_widget_hide),
					 nullptr);

	m_mainWidget    = GTK_WIDGET(gtk_builder_get_object(m_mainWidgetInterface, "matrix-display-table"));
	m_toolbarWidget = GTK_WIDGET(gtk_builder_get_object(m_toolbarWidgetInterface, "matrix-display-toolbar"));

	if (!this->canCreatePluginObject(OVP_ClassId_Plugin_VisualizationCtx)) {
		this->getLogManager() << Kernel::LogLevel_Error << "Visualization framework is not loaded" << "\n";
		return false;
	}

	m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
	m_visualizationCtx->setWidget(*this, m_mainWidget);
	m_visualizationCtx->setToolbar(*this, m_toolbarWidget);

	m_ShowValues = (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_toolbarWidgetInterface, "show-values-toggle-button"))) !=
					0);
	m_ShowColors = (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_toolbarWidgetInterface, "show-colors-toggle-button"))) !=
					0);

	CString gradientSetting;
	getBoxAlgorithmContext()->getStaticBoxContext()->getSettingValue(0, gradientSetting);
	VisualizationToolkit::ColorGradient::parse(m_colorGradient, gradientSetting);

	CString gradientStepsSetting;
	getBoxAlgorithmContext()->getStaticBoxContext()->getSettingValue(1, gradientStepsSetting);
	m_gradientSteps = strtol(gradientStepsSetting, nullptr, 10);
	VisualizationToolkit::ColorGradient::interpolate(m_interpolatedColorGardient, m_colorGradient, m_gradientSteps);
	m_max = 0;
	m_min = 0;

	CString symetricMinMaxSetting;
	getBoxAlgorithmContext()->getStaticBoxContext()->getSettingValue(2, symetricMinMaxSetting);
	m_symetricMinMax = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 2);

	CString realTimeMinMaxSetting;
	getBoxAlgorithmContext()->getStaticBoxContext()->getSettingValue(3, realTimeMinMaxSetting);
	m_realTimeMinMax = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 3);

	return true;
}

bool CBoxAlgorithmMatrixDisplay::uninitialize()

{
	op_matrix.uninitialize();
	ip_buffer.uninitialize();

	//decoders
	m_iMatrix->uninitialize();
	this->getAlgorithmManager().releaseAlgorithm(*m_iMatrix);

	//widgets
	g_object_unref(m_toolbarWidgetInterface);
	m_toolbarWidgetInterface = nullptr;

	g_object_unref(m_mainWidgetInterface);
	m_mainWidgetInterface = nullptr;

	this->releasePluginObject(m_visualizationCtx);

	return true;
}

bool CBoxAlgorithmMatrixDisplay::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();

	return true;
}

bool CBoxAlgorithmMatrixDisplay::process()

{
	Kernel::IBoxIO& boxContext = this->getDynamicBoxContext();

	for (size_t i = 0; i < boxContext.getInputChunkCount(0); ++i) {
		ip_buffer = boxContext.getInputChunk(0, i);
		m_iMatrix->process();

		if (m_iMatrix->isOutputTriggerActive(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputTriggerId_ReceivedHeader)) {
			//header received
			//adding the event  to the window
			GtkTable* table = GTK_TABLE(gtk_builder_get_object(m_mainWidgetInterface, "matrix-display-table"));
			guint nRow, nCol;
			if (op_matrix->getDimensionCount() == 1) {
				//getLogManager() << Kernel::LogLevel_Warning<< "The streamed matrix received has 1 dimensions (found "<< op_matrix->getDimensionCount() <<" dimensions)\n";
				nRow = 1;
				nCol = guint(op_matrix->getDimensionSize(0));
				//return false;
			}
			else if (op_matrix->getDimensionCount() != 2) {
				getLogManager() << Kernel::LogLevel_Error << "The streamed matrix received has more than 2 dimensions (found " << op_matrix->getDimensionCount()
						<< " dimensions)\n";
				return false;
			}
			else {
				nRow = guint(op_matrix->getDimensionSize(0));
				nCol = guint(op_matrix->getDimensionSize(1));
			}

			gtk_table_resize(table, nRow + 1, nCol + 1);

			//first line : labels
			for (guint c = 1; c < nCol + 1; ++c) {
				GtkWidget* label = gtk_label_new("");
				gtk_widget_set_visible(label, 1);
				gtk_table_attach(table, label, c, c + 1, 0, 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
				//g_object_unref(l_pGtkBuilderLabel);

				const std::string str = std::to_string(c);
				gtk_label_set_label(GTK_LABEL(label), str.c_str());
				m_columnLabelCache.emplace_back(GTK_LABEL(label), str.c_str());
			}

			//first column : labels
			for (guint r = 1; r < nRow + 1; ++r) {
				GtkWidget* label = gtk_label_new("");
				gtk_widget_set_visible(label, 1);
				gtk_table_attach(table, label, 0, 1, r, r + 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);

				std::stringstream ss;
				ss << char(r - 1 + int('A'));
				gtk_label_set_label(GTK_LABEL(label), ss.str().c_str());
				m_rowLabelCache.emplace_back(GTK_LABEL(label), ss.str().c_str());
			}

			for (guint r = 1; r < nRow + 1; ++r) {
				for (guint c = 1; c < nCol + 1; ++c) {
					GtkWidget* eventBox = gtk_event_box_new();
					gtk_widget_set_visible(eventBox, 1);
					GtkWidget* label = gtk_label_new("");
					gtk_widget_set_visible(label, 1);
					gtk_container_add(GTK_CONTAINER(eventBox), label);
					gtk_table_attach(table, eventBox, c, c + 1, r, r + 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0,
									 0);

					GdkColor white;
					white.red   = 65535;
					white.green = 65535;
					white.blue  = 65535;
					gtk_widget_modify_bg(eventBox, GTK_STATE_NORMAL, &white);
					m_eventBoxCache.emplace_back(eventBox, white);

					gtk_label_set_label(GTK_LABEL(label), "X");
					m_labelCache.emplace_back(GTK_LABEL(label), "X");
				}
			}
		}

		if (m_iMatrix->isOutputTriggerActive(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputTriggerId_ReceivedBuffer)) {
			//buffer received
			//2-dimension-matrix values
			size_t nRow, nCol;
			if (op_matrix->getDimensionCount() == 1) {
				nRow = 1;
				nCol = op_matrix->getDimensionSize(0);
			}
			else {
				nRow = op_matrix->getDimensionSize(0);
				nCol = op_matrix->getDimensionSize(1);
			}

			if (m_realTimeMinMax || // we need recompute the min max at each loop call
				(std::fabs(m_max) <= DBL_EPSILON && std::fabs(m_min) <= DBL_EPSILON)) // we have never computed the min max values.
			{
				if (op_matrix->getBufferElementCount() != 0) // if the matrix is not empty.
				{
					m_max = op_matrix->getBuffer()[0];
					m_min = op_matrix->getBuffer()[0];
				}
			}

			// MIN-MAX computation
			for (size_t r = 0; r < nRow; ++r) {
				for (size_t c = 0; c < nCol; ++c) {
					double value = op_matrix->getBuffer()[r * nCol + c];
					m_max        = (value > m_max ? value : m_max);
					m_min        = (value < m_min ? value : m_min);

					if (m_symetricMinMax) {
						double maxAbsValue = (fabs(m_max) > fabs(m_min) ? fabs(m_max) : fabs(m_min));
						m_max              = maxAbsValue;
						m_min              = -maxAbsValue;
					}
				}
			}

			for (size_t r = 0; r < nRow; ++r) {
				for (size_t c = 0; c < nCol; ++c) {
					double value = op_matrix->getBuffer()[r * nCol + c];
					if (std::fabs(m_max) > DBL_EPSILON || std::fabs(m_min) >
						DBL_EPSILON) // if the first value ever sent is 0, both are 0, and we dont want to divide by 0 :)
					{
						const size_t step = size_t(((value - m_min) / (m_max - m_min)) * double(m_gradientSteps - 1));

						// gtk_widget_modify_bg uses 16bit colors, the interpolated gradients gives 8bits colors.
						GdkColor color;
						color.red   = uint16_t(m_interpolatedColorGardient[step * 4 + 1] * 65535.0 / 100.0);
						color.green = uint16_t(m_interpolatedColorGardient[step * 4 + 2] * 65535.0 / 100.0);
						color.blue  = uint16_t(m_interpolatedColorGardient[step * 4 + 3] * 65535.0 / 100.0);

						if (memcmp(&(m_eventBoxCache[r * nCol + c].second), &color, sizeof(GdkColor)) != 0 && m_ShowColors) {
							gtk_widget_modify_bg(m_eventBoxCache[r * nCol + c].first, GTK_STATE_NORMAL, &color);
						}
						m_eventBoxCache[r * nCol + c].second = color;

						std::stringstream ss;
						ss << std::fixed;
						ss << std::setprecision(2);
						if (m_ShowValues) { ss << value; }

						if (ss.str() != m_labelCache[r * nCol + c].second) { gtk_label_set_label(m_labelCache[r * nCol + c].first, ss.str().c_str()); }
						m_labelCache[r * nCol + c].second = ss.str();
					}
				}
			}

			if (op_matrix->getDimensionCount() != 1) {
				//first line : labels
				for (size_t c = 0; c < nCol; ++c) {
					if (m_columnLabelCache[c].second != op_matrix->getDimensionLabel(1, c) && !std::string(op_matrix->getDimensionLabel(1, c)).empty()) {
						gtk_label_set_label(GTK_LABEL(m_columnLabelCache[c].first), op_matrix->getDimensionLabel(1, c));
						m_columnLabelCache[c].second = op_matrix->getDimensionLabel(1, c);
					}
				}

				//first column : labels
				for (size_t r = 0; r < nRow; ++r) {
					if (m_rowLabelCache[r].second != op_matrix->getDimensionLabel(0, r) && !std::string(op_matrix->getDimensionLabel(0, r)).empty()) {
						gtk_label_set_label(GTK_LABEL(m_rowLabelCache[r].first), op_matrix->getDimensionLabel(0, r));
						m_rowLabelCache[r].second = op_matrix->getDimensionLabel(0, r);
					}
				}
			}
			else {
				//first line : labels
				for (size_t c = 0; c < nCol; ++c) {
					if (m_columnLabelCache[c].second != op_matrix->getDimensionLabel(0, c) && !std::string(op_matrix->getDimensionLabel(0, c)).empty()) {
						gtk_label_set_label(GTK_LABEL(m_columnLabelCache[c].first), op_matrix->getDimensionLabel(0, c));
						m_columnLabelCache[c].second = op_matrix->getDimensionLabel(0, c);
					}
				}
			}
		}

		/*if(iMatrix->isOutputTriggerActive(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputTriggerId_ReceivedEnd)) { }*/

		boxContext.markInputAsDeprecated(0, i);
	}

	return true;
}

}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
