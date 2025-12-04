///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalDisplayView.cpp
/// \brief Implementation for the class CSignalDisplayView.
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

#include "CSignalDisplayView.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include "../../utils.hpp"

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {


// #define DEBUG 1

static void ScrollModeButtonCB(GtkWidget* widget, gpointer data);
static void UnitsButtonCB(GtkWidget* widget, gpointer data);
static void ScalingModeButtonCB(GtkWidget* widget, gpointer data);
static void ToggleLeftRulerButtonCB(GtkWidget* widget, gpointer data);
static void ToggleBottomRulerButtonCB(GtkWidget* widget, gpointer data);
static void CustomVerticalScaleChangedCB(GtkSpinButton* button, gpointer data);
static void CustomVerticalOffsetChangedCB(GtkSpinButton* button, gpointer data);
static gboolean SpinButtonValueChangedCB(GtkSpinButton* widget, gpointer data); // time scale
static void ChannelSelectButtonCB(GtkButton* button, gpointer data);
static void ChannelSelectDialogApplyButtonCB(GtkButton* button, gpointer data);
static void StimulationColorsButtonCB(GtkButton* button, gpointer data);
static gint CloseStimulationColorsWindow(GtkWidget* widget, GdkEvent* event, gpointer data);
static void InformationButtonCB(GtkButton* button, gpointer data);
static void MultiViewButtonCB(GtkButton* button, gpointer data);
static void MultiViewDialogApplyButtonCB(GtkButton* button, gpointer data);

//const char* CSignalDisplayView::m_ScalingModes[] = { "Per channel", "Global", "None" };
const std::array<std::string, 3> CSignalDisplayView::SCALING_MODES = { "Per channel", "Global", "None" };

CSignalDisplayView::CSignalDisplayView(CBufferDatabase& buffer, const CIdentifier& displayMode, const CIdentifier& scalingMode, const double verticalScale,
									   const double verticalOffset, const double timeScale, const bool horizontalRuler, const bool verticalRuler,
									   const bool multiview)
	: m_ShowLeftRulers(verticalRuler), m_ShowBottomRuler(horizontalRuler), m_CustomVerticalScaleValue(verticalScale), m_CustomVerticalOffset(verticalOffset),
	  m_Buffer(&buffer), m_MultiViewEnabled(multiview), m_ScalingMode(scalingMode)
{
	m_SelectedChannels.clear();
	m_ChannelUnits.clear();
	m_ErrorState.clear();
	Construct(buffer, timeScale, displayMode);
}

void CSignalDisplayView::Construct(CBufferDatabase& /*oBufferDatabase*/, const double timeScale, const CIdentifier& displayMode)
{
	//load the gtk builder interface
	m_Builder =
			gtk_builder_new(); // glade_xml_new(Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-SignalDisplay.ui", nullptr, nullptr);
	gtk_builder_add_from_file(m_Builder, Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-SignalDisplay.ui", nullptr);

	if (!m_Builder) {
		g_warning("Couldn't load the interface!");
		return;
	}

	gtk_builder_connect_signals(m_Builder, nullptr);

	//initialize display mode
	m_Buffer->SetDisplayMode(displayMode);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_Builder, "SignalDisplayScrollModeButton")), displayMode == Scroll);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayScrollModeButton")), true);

	//connect display mode callbacks
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayScrollModeButton")), "toggled", G_CALLBACK(ScrollModeButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayToggleUnitsButton")), "toggled", G_CALLBACK(UnitsButtonCB), this);

	//creates the cursors
	m_Cursor[0] = gdk_cursor_new(GDK_LEFT_PTR);
	m_Cursor[1] = gdk_cursor_new(GDK_SIZING);

	//button callbacks
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectButton")), "clicked", G_CALLBACK(ChannelSelectButtonCB),
					 this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsButton")), "clicked",
					 G_CALLBACK(StimulationColorsButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewButton")), "clicked", G_CALLBACK(MultiViewButtonCB), this);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayInformationButton")), "clicked", G_CALLBACK(InformationButtonCB),
					 this);

	//initialize vertical scale
	// ::gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayVerticalScaleToggleButton")), m_autoVerticalScale);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(m_Builder, "SignalDisplayCustomVerticalScaleSpinButton")), m_CustomVerticalScaleValue);
	// ::gtk_spin_button_set_increments(GTK_SPIN_BUTTON(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayCustomVerticalScaleSpinButton")),0.001,1.0);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayCustomVerticalScaleSpinButton")), m_ScalingMode == None);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayDC")), m_ScalingMode == None);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(m_Builder, "SignalDisplayDC")), m_CustomVerticalOffset);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayDC")), "value-changed", G_CALLBACK(CustomVerticalOffsetChangedCB), this);

	//		::gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayToggleUnitsButton")), false);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayToggleUnitsButton")), false);

	//connect vertical scale callbacks
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayCustomVerticalScaleSpinButton")), "value-changed",
					 G_CALLBACK(CustomVerticalScaleChangedCB), this);

	//time scale
	//----------
	GtkSpinButton* spinButton = GTK_SPIN_BUTTON(gtk_builder_get_object(m_Builder, "SignalDisplayTimeScale"));
	gtk_spin_button_set_value(spinButton, timeScale);
	g_signal_connect(G_OBJECT(spinButton), "value-changed", G_CALLBACK(SpinButtonValueChangedCB), this);
	//notify database of current time scale
	m_Buffer->AdjustNumberOfDisplayedBuffers(gtk_spin_button_get_value(spinButton));

	//channel select dialog's signals
	//-------------------------------
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectApplyButton")), "clicked",
					 G_CALLBACK(ChannelSelectDialogApplyButtonCB), this);

	//connect the cancel button to the dialog's hide command
	g_signal_connect_swapped(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectCancelButton")), "clicked",
							 G_CALLBACK(gtk_widget_hide),
							 G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectDialog")));

	//hides the dialog if the user tries to close it
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectDialog")), "delete_event", G_CALLBACK(gtk_widget_hide), nullptr);

	//stimulation colors dialog's signals
	//-----------------------------------
	//connect the close button to the dialog's hide command
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsCloseButton")), "clicked",
					 G_CALLBACK(StimulationColorsButtonCB), this);

	//hides the dialog if the user tries to close it
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsDialog")), "delete_event",
					 G_CALLBACK(CloseStimulationColorsWindow), this);

	//multiview signals
	//-----------------
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewApplyButton")), "clicked",
					 G_CALLBACK(MultiViewDialogApplyButtonCB), this);

	//connect the cancel button to the dialog's hide command
	g_signal_connect_swapped(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewCancelButton")), "clicked",
							 G_CALLBACK(gtk_widget_hide), G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewDialog")));

	//hides the dialog if the user tries to close it
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewDialog")), "delete_event", G_CALLBACK(gtk_widget_hide),
					 nullptr);

	//bottom box
	//----------
	m_BottomBox = GTK_BOX(gtk_builder_get_object(m_Builder, "SignalDisplayBottomBox"));

	// ::gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayBestFitButton")), false);
	// ::gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayGlobalBestFitButton")), false);

	GtkComboBox* comboBox = GTK_COMBO_BOX(gtk_builder_get_object(m_Builder, "ScalingMode"));

	for (const auto& scalingMode : SCALING_MODES) { gtk_combo_box_append_text(comboBox, scalingMode.c_str()); }

	g_signal_connect(G_OBJECT(comboBox), "changed", G_CALLBACK(ScalingModeButtonCB), this);
	gtk_combo_box_set_active(comboBox, gint(m_ScalingMode.id()));

	gtk_widget_set_sensitive(GTK_WIDGET(comboBox), true);

	//	GtkWidget* mainWindow = GTK_WIDGET(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayMainWindow"));
	//	::gtk_window_set_default_size(GTK_WINDOW(mainWindow), 640, 200);
}

CSignalDisplayView::~CSignalDisplayView()
{
	// @fixme who destroys this beast? It seems to be accessed by visualizationtree later?!  pointer ownership unclear.
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(m_Builder, "Toolbar")));

	gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayInformationDialog")));
	gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectDialog")));
	gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewDialog")));
	gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsDialog")));

	//destroy the window and its children
	gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayMainWindow")));

	//destroy the rest
	for (auto& cursor : m_Cursor) { gdk_cursor_unref(cursor); }

	//unref the xml file as it's not needed anymore
	g_object_unref(G_OBJECT(m_Builder));
	m_Builder = nullptr;

	for (auto& channel : m_ChannelDisplay) { delete channel; }

	delete m_BottomRuler;
	m_BottomRuler = nullptr;
}

void CSignalDisplayView::GetWidgets(GtkWidget*& widget, GtkWidget*& toolbar) const
{
	widget  = GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayScrolledWindow"));
	toolbar = GTK_WIDGET(gtk_builder_get_object(m_Builder, "Toolbar"));
}

void CSignalDisplayView::ChangeMultiView()
{
	CSignalChannelDisplay* multiViewDisplay = m_ChannelDisplay[m_ChannelDisplay.size() - 1];

	//check if there are channels to display in multiview
	m_MultiViewEnabled = false;
	bool noneSelected  = false;
	for (size_t i = 0; i < m_ChannelLabel.size(); ++i) {
		//Check if None is selected
		if (i == m_ChannelLabel.size() - 1) { noneSelected = m_MultiViewSelectedChannels[i]; }

		//Enable Multiview only if None item isn't selected and at list one channel is selected
		if (!noneSelected) { m_MultiViewEnabled |= m_MultiViewSelectedChannels[i]; }
		else { m_MultiViewEnabled = false; }
	}

	//if there are no channels to display in the multiview (None selected only)
	if (!m_MultiViewEnabled) {
		//hides the multiview display (last one in the list)
		multiViewDisplay->ResetChannelList();
		ToggleChannelMultiView(false);
	}
	//there are channels to display in the multiview
	else {
		if (!GTK_WIDGET_VISIBLE(GTK_WIDGET(m_SignalDisplayTable))) {
			//if there were no selected channels before, but now there are, show the table again
			gtk_widget_show(GTK_WIDGET(m_SignalDisplayTable));
		}

		if (!IsChannelDisplayVisible(m_ChannelDisplay.size() - 1)) { ToggleChannelMultiView(true); }

		//updates channels to display list
		multiViewDisplay->ResetChannelList();

		for (size_t i = 0; i < m_MultiViewSelectedChannels.size(); ++i) { if (m_MultiViewSelectedChannels[i]) { multiViewDisplay->AddChannelList(i); } }

		multiViewDisplay->UpdateLimits();

		if (m_ShowLeftRulers) { gtk_widget_show(GTK_WIDGET(m_LeftRulers[m_ChannelDisplay.size() - 1])); }

		multiViewDisplay->m_MultiView = true;

		m_VerticalScaleForceUpdate = true; // need to pass the scale params to multiview, use this to make them refresh...
		m_VerticalScaleRefresh     = true;


		//request a redraw
		/*
		if(display->getSignalDisplayWidget()->window)
		{
			gdk_window_invalidate_rect(display->getSignalDisplayWidget()->window, nullptr, false);
		}
		*/
	}
}

void CSignalDisplayView::Init()
{
	//retrieve channel count
	const size_t nChannel  = m_Buffer->GetChannelCount();
	const size_t tableSize = 2;

	//allocate channel labels and channel displays arrays accordingly
	m_ChannelDisplay.resize(tableSize);
	m_ChannelLabel.resize(nChannel + 1);
	m_ChannelName.resize(nChannel + 1);

	GtkWidget* scrolledWindow = GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayScrolledWindow"));
	gtk_widget_set_size_request(scrolledWindow, 400, 200);

	//retrieve and allocate main table accordingly
	m_SignalDisplayTable = GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayMainTable"));
	//rows : for each channel, [0] channel data, [1] horizontal separator
	//columns : [0] label, [1] vertical separator, [2] left ruler, [3] signal display
	gtk_table_resize(GTK_TABLE(m_SignalDisplayTable), gint(nChannel + 1), 4);

	const int leftRulerW      = 50;
	const int channelDisplayW = 20;
	const int bottomRulerW    = 0;
	const int leftRulerH      = 20;
	const int channelDisplayH = 20;
	const int bottomRulerH    = 20;

	m_NSelectedChannel = nChannel;	// All channels selected by default

	UpdateDisplayTableSize();

	//add a vertical separator
	m_Separator = gtk_vseparator_new();
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), m_Separator, 1, 2, 0, gint(nChannel + 1),	//second column run over the whole table height
					 GTK_SHRINK, GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_widget_show(m_Separator);

	//create a size group for channel labels and the empty bottom left widget
	//(useful to position the bottom ruler correctly)
	//::GtkSizeGroup* sizeGroup = ::gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	//channels selection widget
	GtkWidget* channelSelectList = GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectList"));

	//multiple channels selection widget
	GtkWidget* multiViewSelectList = GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewSelectList"));

	//vector of channel names
	const std::vector<std::string>& channelName = m_Buffer->m_DimLabels[0];

	std::stringstream labelString;

	GtkListStore* channelListStore = gtk_list_store_new(1, G_TYPE_STRING);
	GtkTreeIter channelIter;

	GtkListStore* multiViewChannelListStore = gtk_list_store_new(1, G_TYPE_STRING);
	GtkTreeIter multiViewChannelIter;

	//create channel widgets and add them to display table
	for (size_t i = 0; i < nChannel; ++i) {
		//add channel label
		//-----------------
		// Convention: Channels are numbered as 1,2,... when shown to user
		if (channelName[i].empty()) { labelString << "Channel " << (i + 1); }	// if no name has been set, use channel index
		else { labelString << (i + 1) << " : " << channelName[i]; }				// prepend name with channel index

		// In either mode (eeg or non-eeg) create and attach label widget for each channel
		GtkWidget* label  = gtk_label_new(labelString.str().c_str());
		m_ChannelName[i]  = labelString.str().c_str();
		m_ChannelLabel[i] = label;
		gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), label, 0, 1, //first column
						 gint(i), gint(i + 1), GTK_FILL, GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
		gtk_widget_show(label);

		// Using the labels in a size group causes it to freeze after changing the labels in a callback. Disabled for now.
		//		::gtk_size_group_add_widget(sizeGroup, label);
		if (m_ChannelUnits.size() <= i) { m_ChannelUnits[i] = std::pair<CString, CString>("Unknown", "Unspecified"); }

		//create channel display widget
		//-----------------------------

		//add checkbox in channel select window
		//-------------------------------------
		gtk_list_store_append(channelListStore, &channelIter);
		gtk_list_store_set(channelListStore, &channelIter, 0, channelName[i].c_str(), -1);

		gtk_list_store_append(multiViewChannelListStore, &multiViewChannelIter);
		gtk_list_store_set(multiViewChannelListStore, &multiViewChannelIter, 0, channelName[i].c_str(), -1);

		labelString.str("");

		//a channel is selected by default
		m_SelectedChannels[i] = true;
		if (m_MultiViewEnabled) { m_MultiViewSelectedChannels[i] = true; }
		else { m_MultiViewSelectedChannels[i] = false; }

		//clear label
		labelString.str("");
	}

	// create one display for all channels

	//create and attach display widget
	auto* channelDisplay = new CSignalChannelDisplay(this, channelDisplayW, channelDisplayH, leftRulerW, leftRulerH);
	m_ChannelDisplay[0]  = channelDisplay;
	for (size_t i = 0; i < nChannel; ++i) {
		channelDisplay->AddChannel(i);

		// Still attach left rulers
		gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), channelDisplay->GetRulerWidget(i), 2, 3, //third column
						 gint(i), gint(i + 1), GTK_FILL, GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
		gtk_widget_show(channelDisplay->GetRulerWidget(i));
	}
	channelDisplay->UpdateLimits();

	// attach display
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), channelDisplay->GetSignalDisplayWidget(), 3, 4, 0,
					 gint(nChannel),	// fourth column run over the whole table (last row for multiview)
					 GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_widget_show(m_ChannelDisplay[0]->GetSignalDisplayWidget());

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(channelSelectList)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(channelSelectList),
								gtk_tree_view_column_new_with_attributes("Channel", gtk_cell_renderer_text_new(), "text", 0, nullptr));
	gtk_tree_view_set_model(GTK_TREE_VIEW(channelSelectList), GTK_TREE_MODEL(channelListStore));

	gtk_list_store_append(multiViewChannelListStore, &multiViewChannelIter);
	gtk_list_store_set(multiViewChannelListStore, &multiViewChannelIter, 0, "None", -1);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(multiViewSelectList)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(multiViewSelectList),
								gtk_tree_view_column_new_with_attributes("Channel", gtk_cell_renderer_text_new(), "text", 0, nullptr));
	gtk_tree_view_set_model(GTK_TREE_VIEW(multiViewSelectList), GTK_TREE_MODEL(multiViewChannelListStore));

	//multiview channel
	//-----------------
	//create and attach label
	GtkWidget* label         = gtk_label_new("Multi-View");
	m_ChannelLabel[nChannel] = label;
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), label, 0, 1, gint(nChannel), gint(nChannel) + 1, GTK_FILL, GTK_SHRINK, 0, 0);
	//create and attach display widget
	auto* multiViewDisplay          = new CSignalChannelDisplay(this, channelDisplayW, channelDisplayH, leftRulerW, leftRulerH);
	m_ChannelDisplay[tableSize - 1] = multiViewDisplay;
	multiViewDisplay->AddChannel(0);
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), multiViewDisplay->GetRulerWidget(0), 2, 3, //third column
					 gint(nChannel), gint(nChannel) + 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), multiViewDisplay->GetSignalDisplayWidget(), 3, 4, //fourth column
					 gint(nChannel), gint(nChannel) + 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
	//create bottom ruler
	//-------------------
	m_BottomRuler = new CBottomTimeRuler(*m_Buffer, bottomRulerW, bottomRulerH);
	//::gtk_size_group_add_widget(sizeGroup, GTK_WIDGET(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayEmptyLabel1")));
	gtk_box_pack_start(m_BottomBox, m_BottomRuler->GetWidget(), false, false, 0);
	// tell ruler has to resize when channel displays are resized
	if (!m_ChannelDisplay.empty()) { m_BottomRuler->LinkWidthToWidget(m_ChannelDisplay[0]->GetSignalDisplayWidget()); }
	gtk_widget_show_all(m_BottomRuler->GetWidget());

	//allocate memory to store sample points
	//--------------------------------------
	//reserve the maximum space needed for computing the points to display
	//(when cropping the lines, there can be up to two times the number of original points)
	m_Points.reserve(size_t(m_Buffer->m_DimSizes[1] * m_Buffer->m_NBufferToDisplay * 2));
	//resize the vector of raw points
	m_RawPoints.resize(size_t(m_Buffer->m_DimSizes[1] * m_Buffer->m_NBufferToDisplay));

	for (const auto& channel : m_ChannelDisplay) {
		for (size_t i = 0; i < channel->m_LeftRuler.size(); ++i) {
			GtkWidget* leftRuler = channel->GetRulerWidget(i);
			m_LeftRulers.push_back(leftRuler);
		}
	}

	ToggleLeftRulers(m_ShowLeftRulers);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_Builder, "SignalDisplayToggleLeftRulerButton")),
									  m_ShowLeftRulers);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayToggleLeftRulerButton")), "toggled",
					 G_CALLBACK(ToggleLeftRulerButtonCB), this);

	ToggleBottomRuler(m_ShowBottomRuler);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_Builder, "SignalDisplayToggleBottomRulerButton")),
									  m_ShowBottomRuler);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_Builder, "SignalDisplayToggleBottomRulerButton")), "toggled",
					 G_CALLBACK(ToggleBottomRulerButtonCB), this);

	if (m_MultiViewEnabled) { ChangeMultiView(); }

	ActivateToolbarButtons(true);
}

void CSignalDisplayView::Redraw()
{
	//nothing to redraw if the table isn't visible or no data was received
	if (m_SignalDisplayTable == nullptr || !GTK_WIDGET_VISIBLE(m_SignalDisplayTable) || !m_Buffer->HasFirstBuffer()) { return; }

	if (m_VerticalScaleRefresh || m_VerticalScaleForceUpdate) {
		const double marginMultiplier = 0.2;

		// @note the reason the applying of scale parameters is here and not inside SignalChannelDisplay is that in
		// some situations we wish to estimate and set params across two SignalChannelDisplay objects: 
		// the main view and multiview.
		if (m_ScalingMode == Global) {
			// Auto global

			// Find the global min and max
			std::vector<double> mins;
			std::vector<double> maxs;
			m_ChannelDisplay[0]->GetDisplayedValueRange(mins, maxs);

			double min = *(std::min_element(mins.begin(), mins.end()));
			double max = *(std::max_element(maxs.begin(), maxs.end()));

			if (m_MultiViewEnabled) {
				std::vector<double> multiMins;
				std::vector<double> multiMaxs;
				m_ChannelDisplay[1]->GetDisplayedValueRange(multiMins, multiMaxs);

				min = std::min(min, *(std::min_element(multiMins.begin(), multiMins.end())));
				max = std::max(max, *(std::max_element(multiMaxs.begin(), multiMaxs.end())));
			}

			// @todo some robust & fast estimate of a high quantile instead of max/min...
			const double margin = marginMultiplier * (max - min);

			const double innerTopMargin    = m_ChannelDisplay[0]->m_InnerTopMargin[0];
			const double innerBottomMargin = m_ChannelDisplay[0]->m_InnerBottomMargin[0];

			if (m_VerticalScaleForceUpdate ||
				min < innerBottomMargin - margin || max > innerTopMargin + margin ||
				min > innerBottomMargin + margin || max < innerTopMargin - margin) {
				m_ChannelDisplay[0]->SetGlobalScaleParameters(min, max, margin); // normal chns
				m_ChannelDisplay[1]->SetGlobalScaleParameters(min, max, margin); // multiview
			}
		}
		else if (m_ScalingMode == None) {
			// Manual global, only updated when triggered as necessary
			if (m_VerticalScaleForceUpdate) {
				const double min    = m_CustomVerticalOffset - m_CustomVerticalScaleValue / 2;
				const double max    = m_CustomVerticalOffset + m_CustomVerticalScaleValue / 2;
				const double margin = marginMultiplier * (max - min);

				m_ChannelDisplay[0]->SetGlobalScaleParameters(min, max, margin); // normal chns
				m_ChannelDisplay[1]->SetGlobalScaleParameters(min, max, margin); // multiview
			}
		}
		else if (m_ScalingMode == PerChannel) {
			// Auto local
			std::vector<double> mins;
			std::vector<double> maxs;
			m_ChannelDisplay[0]->GetDisplayedValueRange(mins, maxs);

#ifdef DEBUG
			if (m_VerticalScaleForceUpdate) { std::cout << "All channel params updated, forced\n"; }
#endif

			bool updated = false;
			for (size_t i = 0; i < mins.size(); ++i) {
				const double margin            = marginMultiplier * (maxs[i] - mins[i]);
				const double innerTopMargin    = m_ChannelDisplay[0]->m_InnerTopMargin[i];
				const double innerBottomMargin = m_ChannelDisplay[0]->m_InnerBottomMargin[i];

				if (m_VerticalScaleForceUpdate ||
					mins[i] < innerBottomMargin - margin || maxs[i] > innerTopMargin + margin ||
					mins[i] > innerBottomMargin + margin || maxs[i] < innerTopMargin - margin) {
#ifdef DEBUG
					if (!m_VerticalScaleForceUpdate) {
						std::cout << "Channel " << i + 1 << " params updated: "
							<< mins[i] << " not in [" << innerBottomMargin - margin << "," << innerBottomMargin + margin << "], or "
							<< maxs[i] << " not in [" << innerTopMargin - margin << "," << innerTopMargin + margin << "], "
							<< " margin was " << margin << "\n";
					}
#endif
					m_ChannelDisplay[0]->SetLocalScaleParameters(i, mins[i], maxs[i], margin);
					updated = true;
				}
				else {
#if 0
#ifdef DEBUG
					std::cout << "No need to update channel " << i + 1 << ", "
						<< mins[i] << " in [" << innerBottomMargin - margin << "," << innerBottomMargin + margin << "], or "
						<< maxs[i] << " in [" << innerTopMargin - margin << "," << innerTopMargin + margin << "], "
						<< " margin was " << margin << "\n";
#endif
#endif
				}
			}
			if (updated) { m_ChannelDisplay[0]->UpdateDisplayParameters(); }

			// For multiview, we take the maxes of the involved signals
			if (m_MultiViewEnabled) {
				m_ChannelDisplay[1]->GetDisplayedValueRange(mins, maxs);

				const double min = *(std::min_element(mins.begin(), mins.end()));
				const double max = *(std::max_element(maxs.begin(), maxs.end()));

				// @todo some robust & fast estimate of a high quantile instead of max/min...
				const double margin            = marginMultiplier * (max - min);
				const double innerTopMargin    = m_ChannelDisplay[1]->m_InnerTopMargin[0];
				const double innerBottomMargin = m_ChannelDisplay[1]->m_InnerBottomMargin[0];

				if (m_VerticalScaleForceUpdate || max > innerTopMargin + margin || max < innerTopMargin - margin
					|| min > innerBottomMargin + margin || min < innerBottomMargin - margin) {
					m_ChannelDisplay[1]->SetGlobalScaleParameters(min, max, margin); // multiview
					m_ChannelDisplay[1]->UpdateDisplayParameters();
				}
			}
		}
		else {
			std::stringstream ss;
			ss << "Error: unknown scaling mode " << m_ScalingMode.str() << ". Did you update the box?\n";
			m_ErrorState.emplace_back(ss.str().c_str());
			return;
		}
		m_VerticalScaleRefresh     = false;
		m_VerticalScaleForceUpdate = false;
	}

	// todo don't reset every frame

	/*


	std::cout << "Range is " << largestDisplayedValueRange << " at " << maxIdxI << "," << maxIdxJ
		<< " with lim [" << m_LargestDisplayedValueRange - m_ValueRangeMargin << ","
						 << m_LargestDisplayedValueRange + m_ValueRangeMargin << ","
		<< " largest " << largestDisplayedValue
		<< " vs " << m_LargestDisplayedValue
		<< " smallest " << smallestDisplayedValue << " vs " << m_SmallestDisplayedValue
		<< "\n";
	*/

	//if in scan mode, check whether time scale needs to be updated
	if (m_Buffer->GetDisplayMode() == Scan && m_LeftmostDisplayedTime < m_Buffer->m_StartTime[0]) {
		//std::cout << "Time basis needs to be updated\n";
		if (m_Buffer->m_SampleBuffers.size() < m_Buffer->m_NBufferToDisplay) { m_LeftmostDisplayedTime = m_Buffer->m_StartTime[0]; }
		else //catch up with current time interval
		{
			if (m_Buffer->m_TotalStep == 0) {
				// Error
				//
				// @note This can happen at least during changing of time scale, however on the next attempt it seems
				// to be already fixed in the bufferdatabase and things seem to work, so don't bother returning error.
				// @fixme should get proper understanding of this part to properly handle it, i.e. should we 
				// really raise an error state in some situations or not.
				//						m_ErrorState.push_back(CString("Error: Buffer database m_TotalStep is 0\n"));
			}
			else {
				m_LeftmostDisplayedTime += m_Buffer->m_TotalStep;

				uint64_t upperLimit = 0;
				if (m_Buffer->m_BufferStep <= m_Buffer->m_StartTime[0]) // This bubblegum-patch test is here for uint, should be an assert
				{
					upperLimit = m_Buffer->m_StartTime[0] - m_Buffer->m_BufferStep;
				}
				else { m_ErrorState.emplace_back("Error: Buffer step is larger than the start time\n"); }

				//while there is time to catch up
				while (m_LeftmostDisplayedTime < upperLimit) { m_LeftmostDisplayedTime += m_Buffer->m_TotalStep; }

				//round leftmost displayed time to start of closest data buffer
				for (size_t i = 0; i < m_Buffer->m_StartTime.size(); ++i) {
					if (m_Buffer->m_EndTime[i] > m_LeftmostDisplayedTime) { m_LeftmostDisplayedTime = m_Buffer->m_StartTime[i]; }
				}

#if 0
				//if drawing is not up to date, force a full redraw
				// We're not currently doing this as it seems to cause even worse lag
				if (m_ChannelDisplay[0]->m_latestDisplayedTime != m_leftmostDisplayedTime)
				{
					for (size_t i = 0; i < m_ChannelDisplay.size(); ++i)
					{
#ifdef DEBUG
						std::cout << "Requesting full redraw for " << i << ", case D (drawing late)\n";
#endif
						m_ChannelDisplay[i]->redrawAllAtNextRefresh(true);
					}
				}
#endif
			}
		}
	}

	//redraw channels
	for (const auto& channel : m_ChannelDisplay) {
		if (GTK_WIDGET_VISIBLE(channel->GetSignalDisplayWidget())) {
			/*
			//if in scroll mode, or if time basis changed, redraw all
			if(m_BufferDatabase->getDisplayMode() == TypeId_SignalDisplayMode_Scroll || channelDisplay->mustRedrawAll() == true)
			{
				std::cout << "full redraw\n";*/
			GdkRectangle updateRect;
			channel->GetUpdateRectangle(updateRect);
			if (channel->GetSignalDisplayWidget()->window) { gdk_window_invalidate_rect(channel->GetSignalDisplayWidget()->window, &updateRect, false); }
			/*}
			else
			{
				GdkRectangle updateRect;
				m_ChannelDisplay[i]->getUpdateRectangle(updateRect);
				//printf("partial redraw : x=%d, w=%d\n", updateRect.x, updateRect.width);
				gdk_window_clear_area_e(m_ChannelDisplay[i]->getSignalDisplayWidget()->window, updateRect.x, updateRect.y, updateRect.width, updateRect.height);
			}*/
		}
	}

	//redraw ruler
	m_BottomRuler->SetLeftmostDisplayedTime(m_LeftmostDisplayedTime);
	if (GTK_WIDGET(m_BottomRuler->GetWidget())->window) { gdk_window_invalidate_rect(GTK_WIDGET(m_BottomRuler->GetWidget())->window, nullptr, true); }
}

void CSignalDisplayView::ToggleLeftRulers(const bool active)
{
	m_ShowLeftRulers = active;

	for (size_t j = 0; j < m_ChannelDisplay[0]->m_LeftRuler.size(); ++j) {
		if (active && IsChannelDisplayVisible(0) && m_SelectedChannels[j]) { gtk_widget_show(m_ChannelDisplay[0]->GetRulerWidget(j)); }
		else { gtk_widget_hide(m_ChannelDisplay[0]->GetRulerWidget(j)); }
	}

	// Multiview
	if (m_MultiViewEnabled) {
		if (active) { gtk_widget_show(m_ChannelDisplay[1]->GetRulerWidget(0)); }
		else { gtk_widget_hide(m_ChannelDisplay[1]->GetRulerWidget(0)); }
	}
}

void CSignalDisplayView::ToggleBottomRuler(const bool active)
{
	m_ShowBottomRuler = active;

	if (active) { gtk_widget_show_all(GTK_WIDGET(m_BottomBox)); }
	else { gtk_widget_hide_all(GTK_WIDGET(m_BottomBox)); }
}

void CSignalDisplayView::ToggleChannel(const size_t index, const bool active)
{
	const CSignalChannelDisplay* display = GetChannelDisplay(index);

	if (active) {
		gtk_widget_show(m_ChannelLabel[index]);
		if (m_ShowLeftRulers) { gtk_widget_show(display->GetRulerWidget(display->m_ChannelList.size() - 1)); }
		gtk_widget_show(display->GetSignalDisplayWidget());
		gtk_widget_show(m_Separators[index]);
	}
	else {
		gtk_widget_hide(m_ChannelLabel[index]);
		gtk_widget_hide(display->GetRulerWidget(display->m_ChannelList.size() - 1));
		gtk_widget_hide(display->GetSignalDisplayWidget());
		gtk_widget_hide(m_Separators[index]);
	}
}

// If we swap multiview on/off, it seems we need to do another size request to 
// get the labels and signals properly aligned. The problem appears if there are many channels and this is not done.
void CSignalDisplayView::UpdateDisplayTableSize() const
{
	const int leftRulerW = 50, channelDisplayW = 20, channelDisplayH = 20;

	//sets a minimum size for the table (needed to scroll)
	gtk_widget_set_size_request(m_SignalDisplayTable, leftRulerW + channelDisplayW, gint(m_NSelectedChannel + (m_MultiViewEnabled ? 1 : 0)) * channelDisplayH);
}

void CSignalDisplayView::ToggleChannelMultiView(const bool active) const
{
	UpdateDisplayTableSize();

	const CSignalChannelDisplay* display = GetChannelDisplay(m_ChannelDisplay.size() - 1);
	if (active) {
		gtk_widget_show(m_ChannelLabel[m_ChannelLabel.size() - 1]);
		if (m_ShowLeftRulers) { gtk_widget_show(display->GetRulerWidget(0)); }
		gtk_widget_show(display->GetSignalDisplayWidget());
	}
	else {
		gtk_widget_hide(m_ChannelLabel[m_ChannelLabel.size() - 1]);
		gtk_widget_hide(display->GetRulerWidget(0));
		gtk_widget_hide(display->GetSignalDisplayWidget());
	}
}

// This removes all the per-channel rulers and widgets. It adds ref count to the removed
// widgets so we can later add them back.
void CSignalDisplayView::RemoveOldWidgets()
{
	// Remove labels and rulers
	for (size_t i = 0; i < m_SelectedChannels.size(); ++i) {
		// Only remove those which we know are displayed
		if (m_SelectedChannels[i]) {
			g_object_ref(m_ChannelLabel[i]);
			g_object_ref(m_ChannelDisplay[0]->GetRulerWidget(i));
			gtk_container_remove(GTK_CONTAINER(m_SignalDisplayTable), m_ChannelLabel[i]);
			gtk_container_remove(GTK_CONTAINER(m_SignalDisplayTable), m_ChannelDisplay[0]->GetRulerWidget(i));
		}
	}

	// Remove the separator
	g_object_ref(m_Separator);
	gtk_container_remove(GTK_CONTAINER(m_SignalDisplayTable), m_Separator);

	// Remove the drawing area
	g_object_ref(m_ChannelDisplay[0]->GetSignalDisplayWidget());
	gtk_container_remove(GTK_CONTAINER(m_SignalDisplayTable), m_ChannelDisplay[0]->GetSignalDisplayWidget());
}

// When channels are added or removed, this function removes and recreates the table holding the
// rulers. The reason to do this is that the size of the drawing canvas is dependent on the size
// of the table, and we want to use the window space to draw the selected signals, likely much
// smaller than the size of canvas for all the channes.
// @note refcounts of the added widgets are decreased. Its expected removeOldWidgets() has been called before.
// @fixme this code could really use some refactoring, for example
// make a struct to hold label and ruler and keep them in a vector. Also, similar attach code is
// already in init(). Turn to functions.
void CSignalDisplayView::RecreateWidgets(const size_t nChannel)
{
	// Resize the table to fit only the selected amount of channels (+multiview)
	gtk_table_resize(GTK_TABLE(m_SignalDisplayTable), gint(nChannel) + 1, 4);

	// Add selected channel widgets back
	for (gint i = 0, cnt = 0; i < gint(m_SelectedChannels.size()); ++i) {
		if (m_SelectedChannels[i]) {
			gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), m_ChannelDisplay[0]->GetRulerWidget(i), 2, 3, //third column
							 cnt, cnt + 1, GTK_FILL, GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
			gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), m_ChannelLabel[i], 0, 1, //first column
							 cnt, cnt + 1, GTK_FILL, GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
			cnt++;
			g_object_unref(m_ChannelLabel[i]);
			g_object_unref(m_ChannelDisplay[0]->GetRulerWidget(i));
		}
	}

	// Add separator back
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), m_Separator, 1, 2, 0, gint(nChannel + 1), //second column run over the whole table height
					 GTK_SHRINK, GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
	g_object_unref(m_Separator);

	// Add drawing canvas back
	gtk_table_attach(GTK_TABLE(m_SignalDisplayTable), m_ChannelDisplay[0]->GetSignalDisplayWidget(), 3, 4, //fourth column
					 0, gint(nChannel),// run over the whole table (last row for multiview)
					 GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
	g_object_unref(m_ChannelDisplay[0]->GetSignalDisplayWidget());

	UpdateDisplayTableSize();
}


void CSignalDisplayView::UpdateMainTableStatus()
{
	// Do we have multiview channels selected?
	bool multiView = false;
	for (size_t i = 0; i < m_MultiViewSelectedChannels.size(); ++i) { multiView |= m_MultiViewSelectedChannels[i]; }

	// See if any normal channels have been selected
	bool channels = false;
	for (size_t i = 0; i < m_SelectedChannels.size(); ++i) { channels |= m_SelectedChannels[i]; }

	//if nothing has been selected, hide & bail out
	if (!channels && !multiView) {
		//hide the whole table
		gtk_widget_hide(GTK_WIDGET(m_SignalDisplayTable));
		return;
	}

	// If a multiview channel has been selected, we link the bottom ruler to the multiview 
	if (!GTK_WIDGET_VISIBLE(GTK_WIDGET(m_SignalDisplayTable))) {
		//if there were no selected channels before, but now there are, show the table again
		gtk_widget_show(GTK_WIDGET(m_SignalDisplayTable));
	}

	if (!multiView) { m_BottomRuler->LinkWidthToWidget(m_ChannelDisplay[0]->GetSignalDisplayWidget()); }
	else { m_BottomRuler->LinkWidthToWidget(m_ChannelDisplay[1]->GetSignalDisplayWidget()); }
}

void CSignalDisplayView::ActivateToolbarButtons(const bool active) const
{
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayScrollModeButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayToggleLeftRulerButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayToggleBottomRulerButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayToggleUnitsButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayChannelSelectButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayMultiViewButton")), active);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayInformationButton")), active);
}

bool CSignalDisplayView::OnUnitsToggledCB(const bool active)
{
	// dont update for multiview
	for (size_t i = 0; i < m_ChannelLabel.size() - 1; ++i) {
		if (active) {
			std::stringstream label("");
			label << m_ChannelName[i].toASCIIString();
			label << "\n(" << m_ChannelUnits[i].first.toASCIIString();
			label << ", " << m_ChannelUnits[i].second.toASCIIString() << ")";

			gtk_label_set_text(GTK_LABEL(m_ChannelLabel[i]), label.str().c_str());
		}
		else {
			std::stringstream label("");
			label << m_ChannelName[i];
			gtk_label_set_text(GTK_LABEL(m_ChannelLabel[i]), label.str().c_str());
		}
	}

	return true;
}

bool CSignalDisplayView::OnDisplayModeToggledCB(const CIdentifier& mode)
{
	m_Buffer->SetDisplayMode(mode);

	//force full redraw of all channels when display mode changes
	for (const auto& channel : m_ChannelDisplay) { channel->RedrawAllAtNextRefresh(true); }

	//redraw channels
	Redraw();

	return true;
}

bool CSignalDisplayView::OnVerticalScaleModeToggledCB(GtkToggleButton* /*pToggleButton*/)
{
	m_VerticalScaleForceUpdate = true;
	m_VerticalScaleRefresh     = true;

	//			::gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayCustomVerticalScaleSpinButton")), !m_autoVerticalScale);
	//			::gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(m_BuilderInterface, "SignalDisplayCustomVerticalScaleSpinButton")), m_LargestDisplayedValueRange);

	return true;
}

bool CSignalDisplayView::OnCustomVerticalScaleChangedCB(GtkSpinButton* button)
{
	m_VerticalScaleForceUpdate = true;
	m_VerticalScaleRefresh     = true;
	m_CustomVerticalScaleValue = gtk_spin_button_get_value(button);
	return true;
}

bool CSignalDisplayView::OnCustomVerticalOffsetChangedCB(GtkSpinButton* button)
{
	m_VerticalScaleForceUpdate = true;
	m_VerticalScaleRefresh     = true;
	m_CustomVerticalOffset     = gtk_spin_button_get_value(button);
	return true;
}


CSignalChannelDisplay* CSignalDisplayView::GetChannelDisplay(const size_t index) const
{
	return (index < m_ChannelDisplay.size()) ? m_ChannelDisplay[index] : nullptr;
}

bool CSignalDisplayView::IsChannelDisplayVisible(const size_t index) const { return GTK_WIDGET_VISIBLE(GetChannelDisplay(index)->GetSignalDisplayWidget()); }

void CSignalDisplayView::OnStimulationReceivedCB(const uint64_t id, const CString& name)
{
	if (m_Stimulations.find(id) == m_Stimulations.end()) {
		//only the lower 32 bits of the stimulation code are currently used to compute the color
		const auto code = uint32_t(id);
		GdkColor color  = InitGDKColor(0, 0, 0, 0);

		//go through the lower 32 bits to compute RGB components. Bit positions are
		//inverted so that close code values result in very different colors.
		for (uint32_t i = 0; i < 11; ++i) {
			color.red |= ((code >> (3 * i)) & 0x1) << (10 - i);
			color.green |= ((code >> (3 * i + 1)) & 0x1) << (10 - i);
			if (i < 10) { color.blue |= ((code >> (3 * i + 2)) & 0x1) << (9 - i); }	//only 10 bits for blue component
		}

		//convert to 16 bits per channel
		color.red   = (color.red * 65535) / 0x7FF; //red coded on 11 bits
		color.green = (color.green * 65535) / 0x7FF; //green coded on 11 bits
		color.blue  = (color.blue * 65535) / 0x3FF; //blue coded on 10 bits

		//store stimulation color in map
		m_Stimulations[id].first  = name;
		m_Stimulations[id].second = color;

		//update stimulations dialog
		updateStimulationColorsDialog(name, color);
	}

	// @note We should not redraw after the stimuli, as the stim timestamp may be in the future compared
	// to the signal database. If that is the case, we get an expensive redraw from the code. 
	// The redraw will be carried out in the normal course of events when plotting the signal.
}

bool CSignalDisplayView::SetChannelUnits(const std::vector<std::pair<CString, CString>>& channelUnits)
{
	for (size_t i = 0; i < channelUnits.size(); ++i) { m_ChannelUnits[i] = channelUnits[i]; }

	return true;
}

void CSignalDisplayView::GetStimulationColor(const uint64_t id, GdkColor& color)
{
	if (m_Stimulations.find(id) != m_Stimulations.end()) { color = m_Stimulations[id].second; }
}

void CSignalDisplayView::GetMultiViewColor(const size_t index, GdkColor& color)
{
	if (m_Signals.find(index) != m_Signals.end()) { color = m_Signals[index].second; }
	else {
		const auto code = uint32_t(index);
		color.red       = 0;
		color.green     = 0;
		color.blue      = 0;

		//go through the lower 32 bits to compute RGB components. Bit positions are
		//inverted so that close code values result in very different colors.
		for (uint32_t i = 0; i < 11; ++i) {
			color.red |= ((code >> (3 * i)) & 0x1) << (10 - i);
			color.green |= ((code >> (3 * i + 1)) & 0x1) << (10 - i);
			if (i < 10) //only 10 bits for blue component
			{
				color.blue |= ((code >> (3 * i + 2)) & 0x1) << (9 - i);
			}
		}

		//convert to 16 bits per channel
		color.red   = (color.red * 65535) / 0x7FF; //red coded on 11 bits
		color.green = (color.green * 65535) / 0x7FF; //green coded on 11 bits
		color.blue  = (color.blue * 65535) / 0x3FF; //blue coded on 10 bits

		//store signal color in map
		m_Signals[index].first  = "";
		m_Signals[index].second = color;
	}
}

void CSignalDisplayView::updateStimulationColorsDialog(const CString& stimulation, const GdkColor& color) const
{
	//retrieve table
	GtkTable* colors = GTK_TABLE(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsTable"));

	//resize table and store new colors
	gtk_table_resize(colors, colors->nrows + 1, 2);

	//set a minimum size request (needed to scroll)
	const int labelW = -1, colorW = 50, rowH = 20;

	gtk_widget_set_size_request(GTK_WIDGET(colors), -1, (colors->nrows + 1) * rowH);

	GtkLabel* stimLabel = GTK_LABEL(gtk_label_new("Stimulations"));
	gtk_widget_set_size_request(GTK_WIDGET(stimLabel), -1, 20);
	gtk_table_attach(colors, GTK_WIDGET(stimLabel), 0, 1, 0, 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0, 0);

	GtkLabel* colorLabel = GTK_LABEL(gtk_label_new("Colors"));
	gtk_widget_set_size_request(GTK_WIDGET(colorLabel), -1, 20);
	gtk_table_attach(colors, GTK_WIDGET(colorLabel), 1, 2, 0, 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0, 0);

	GtkLabel* label = GTK_LABEL(gtk_label_new(stimulation.toASCIIString()));
	gtk_widget_set_size_request(GTK_WIDGET(label), labelW, rowH);
	gtk_table_attach(colors, GTK_WIDGET(label), 0, 1,							//first column
					 colors->nrows - 1, colors->nrows - 1 + 1,	//last row
					 GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
#if 1
	GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, colorW, rowH);
	//fill with RGBA value (255 / 65535 = 1 / 257)
	const guint32 c = (guint32(color.red / 257) << 24) + (guint32(color.green / 257) << 16) + (guint32(color.blue / 257) << 8);
	gdk_pixbuf_fill(pixbuf, c);
	GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
	gtk_table_attach(colors, GTK_WIDGET(image), 1, 2, colors->nrows - 1, colors->nrows - 1 + 1,	//2nd column last row
					 GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
#else
	::GtkColorButton* button = GTK_COLOR_BUTTON(gtk_color_button_new_with_color(&rStimulationColor));
	::gtk_widget_set_size_request(GTK_WIDGET(button), colorWidthRequest, rowHeightRequest);
	//g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(dummyButtonCallback), nullptr);
	::gtk_table_attach(stimulationColorsTable, GTK_WIDGET(button), 1, 2, //2nd column
		stimulationColorsTable->nrows - 1, stimulationColorsTable->nrows - 1 + 1, //last row
		static_cast <::GtkAttachOptions>(GTK_EXPAND | GTK_FILL), static_cast <::GtkAttachOptions>(GTK_EXPAND | GTK_FILL), 0, 0);
#endif
	GtkWidget* stimulationColorsDialog = GTK_WIDGET(gtk_builder_get_object(m_Builder, "SignalDisplayStimulationColorsDialog"));
	if (m_StimulationColorsShown) {
		// Forces a redraw of it all
		gtk_widget_show_all(stimulationColorsDialog);
		gtk_widget_queue_draw(stimulationColorsDialog);
	}
}

void CSignalDisplayView::RefreshScale() { m_VerticalScaleRefresh = true; }	// But do not force an update, its just a recommendation to check...

//
//CALLBACKS
//

void ScrollModeButtonCB(GtkWidget* widget, gpointer data)
{
	reinterpret_cast<CSignalDisplayView*>(data)->OnDisplayModeToggledCB(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) != 0 ? Scroll : Scan);
}

void UnitsButtonCB(GtkWidget* widget, gpointer data)
{
	reinterpret_cast<CSignalDisplayView*>(data)->OnUnitsToggledCB(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) != 0);
}

void ScalingModeButtonCB(GtkWidget* widget, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	const int selection = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

	if (view->m_ScalingMode != selection) {
		view->m_ScalingMode              = selection;
		view->m_VerticalScaleForceUpdate = true;
		view->m_VerticalScaleRefresh     = true;

		view->Redraw(); // immediate redraw

		const bool controlsActive = (view->m_ScalingMode == None);

		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayCustomVerticalScaleSpinButton")), controlsActive);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayDC")), controlsActive);
	}
}

void ToggleLeftRulerButtonCB(GtkWidget* widget, gpointer data)
{
	reinterpret_cast<CSignalDisplayView*>(data)->ToggleLeftRulers(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) != 0);
}

void ToggleBottomRulerButtonCB(GtkWidget* widget, gpointer data)
{
	reinterpret_cast<CSignalDisplayView*>(data)->ToggleBottomRuler(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) != 0);
}

void CustomVerticalScaleChangedCB(GtkSpinButton* button, gpointer data) { reinterpret_cast<CSignalDisplayView*>(data)->OnCustomVerticalScaleChangedCB(button); }

void CustomVerticalOffsetChangedCB(GtkSpinButton* button, gpointer data)
{
	reinterpret_cast<CSignalDisplayView*>(data)->OnCustomVerticalOffsetChangedCB(button);
}

gboolean SpinButtonValueChangedCB(GtkSpinButton* widget, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	const double newValue = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));

	//Compute and save the new number of buffers to display
	const bool numberOfDisplayedBufferChanged = view->m_Buffer->AdjustNumberOfDisplayedBuffers(newValue);

	if (numberOfDisplayedBufferChanged) {
		//reserve the maximum space needed for computing the points to display
		//(when cropping the lines, there can be up to two times the number of original points)
		view->m_Points.reserve(size_t(view->m_Buffer->m_DimSizes[1] * view->m_Buffer->m_NBufferToDisplay * 2));

		//resize the vector of raw points (before cropping)
		view->m_RawPoints.resize(size_t(view->m_Buffer->m_DimSizes[1] * view->m_Buffer->m_NBufferToDisplay));

		//force full redraw of all channels when time scale changes
		for (size_t i = 0; i < view->m_ChannelDisplay.size(); ++i) { view->GetChannelDisplay(i)->UpdateScale(); }
		//redraw channels

		view->m_VerticalScaleForceUpdate = true;
		view->m_VerticalScaleRefresh     = true;

		view->Redraw();
	}

	return FALSE;
}

//called when the channel select button is pressed (opens the channel selection dialog)
void ChannelSelectButtonCB(GtkButton* /*button*/, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	GtkWidget* channelSelectDialog               = GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayChannelSelectDialog"));
	GtkTreeView* channelSelectTreeView           = GTK_TREE_VIEW(gtk_builder_get_object(view->m_Builder, "SignalDisplayChannelSelectList"));
	GtkTreeSelection* channelSelectTreeSelection = gtk_tree_view_get_selection(channelSelectTreeView);
	GtkTreeModel* channelSelectTreeModel         = gtk_tree_view_get_model(channelSelectTreeView);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_first(channelSelectTreeModel, &iter)) {
		for (size_t i = 0; i < view->m_SelectedChannels.size(); ++i) {
			if (view->m_SelectedChannels[i]) { gtk_tree_selection_select_iter(channelSelectTreeSelection, &iter); }
			else { gtk_tree_selection_unselect_iter(channelSelectTreeSelection, &iter); }
			if (!gtk_tree_model_iter_next(channelSelectTreeModel, &iter)) { break; }
		}
	}

	//finally, show the dialog
	gtk_widget_show_all(channelSelectDialog);
}

//Called when the user presses the apply button of the channel selection dialog
void ChannelSelectDialogApplyButtonCB(GtkButton* /*button*/, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	GtkTreeView* channelSelectTreeView           = GTK_TREE_VIEW(gtk_builder_get_object(view->m_Builder, "SignalDisplayChannelSelectList"));
	GtkTreeSelection* channelSelectTreeSelection = gtk_tree_view_get_selection(channelSelectTreeView);
	GtkTreeModel* channelSelectTreeModel         = gtk_tree_view_get_model(channelSelectTreeView);
	GtkTreeIter iter;
	size_t selectedCount = 0;

	view->m_ChannelDisplay[0]->ResetChannelList();

	// We first remove the widgets while we still know from m_SelectedChannels which are displayed
	view->RemoveOldWidgets();

	if (gtk_tree_model_get_iter_first(channelSelectTreeModel, &iter)) {
		for (size_t i = 0; i < view->m_SelectedChannels.size(); ++i) {
			view->m_SelectedChannels[i] = (gtk_tree_selection_iter_is_selected(channelSelectTreeSelection, &iter) != 0);

			if (gtk_tree_selection_iter_is_selected(channelSelectTreeSelection, &iter) != 0) {
				view->m_ChannelDisplay[0]->AddChannelList(i);
				gtk_widget_show(view->m_ChannelLabel[i]);
				if (view->m_ShowLeftRulers) { gtk_widget_show(view->m_LeftRulers[i]); }
				selectedCount++;
			}
			else {
				gtk_widget_hide(view->m_ChannelLabel[i]);
				gtk_widget_hide(view->m_LeftRulers[i]);
			}

			if (!gtk_tree_model_iter_next(channelSelectTreeModel, &iter)) { break; }
		}
	}

	view->m_NSelectedChannel = selectedCount;

	// Add the widgets back with the new list of channels
	view->RecreateWidgets(selectedCount);

	view->UpdateMainTableStatus();

	view->m_VerticalScaleForceUpdate = true;
	view->m_VerticalScaleRefresh     = true;

	//redraw channels
	// view->redraw();

	//hides the channel selection dialog
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayChannelSelectDialog")));
}

gint CloseStimulationColorsWindow(GtkWidget* /*widget*/, GdkEvent* /*event*/, gpointer data)
{
	StimulationColorsButtonCB(nullptr, data);
	return TRUE;
}

void StimulationColorsButtonCB(GtkButton* /*button*/, gpointer data)
{
	auto* view        = reinterpret_cast<CSignalDisplayView*>(data);
	GtkWidget* dialog = GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayStimulationColorsDialog"));

	if (view->m_StimulationColorsShown) {
		gtk_widget_hide(dialog);
		view->m_StimulationColorsShown = false;
	}
	else {
		gtk_widget_show_all(dialog);
		view->m_StimulationColorsShown = true;
	}
}

//Called when the user presses the Information button (opens the information dialog)
void InformationButtonCB(GtkButton* /*button*/, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	//gets the different values from the database and updates the corresponding label's text field
	std::stringstream value;
	value << view->m_Buffer->m_DimSizes[0];
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(view->m_Builder, "SignalDisplayNumberOfChannels")), value.str().c_str());

	value.str("");
	value << view->m_Buffer->m_Sampling;
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(view->m_Builder, "SignalDisplaySamplingFrequency")), value.str().c_str());

	value.str("");
	value << view->m_Buffer->m_DimSizes[1];
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(view->m_Builder, "SignalDisplaySamplesPerBuffer")), value.str().c_str());

	value.str("");
	value << view->m_Buffer->m_MinValue;
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(view->m_Builder, "SignalDisplayMinimumValue")), value.str().c_str());

	value.str("");
	value << view->m_Buffer->m_MaxValue;
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(view->m_Builder, "SignalDisplayMaximumValue")), value.str().c_str());

	GtkWidget* informationDialog = GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayInformationDialog"));

	//connect the close button to the dialog's hide command
	g_signal_connect_swapped(G_OBJECT(gtk_builder_get_object(view->m_Builder, "SignalDisplayInformationClose")),
							 "clicked", G_CALLBACK(gtk_widget_hide), G_OBJECT(informationDialog));

	g_signal_connect(G_OBJECT(informationDialog), "delete_event", G_CALLBACK(gtk_widget_hide), nullptr);

	//finally, show the information dialog
	gtk_widget_show_all(informationDialog);
}

//called when the channel select button is pressed (opens the channel selection dialog)
void MultiViewButtonCB(GtkButton* /*button*/, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	GtkWidget* dialog               = GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayMultiViewDialog"));
	GtkTreeView* treeView           = GTK_TREE_VIEW(gtk_builder_get_object(view->m_Builder, "SignalDisplayMultiViewSelectList"));
	GtkTreeSelection* treeSelection = gtk_tree_view_get_selection(treeView);
	GtkTreeModel* treeModel         = gtk_tree_view_get_model(treeView);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_first(treeModel, &iter)) {
		for (size_t i = 0; !view->m_MultiViewSelectedChannels.empty(); ++i) {
			if (view->m_MultiViewSelectedChannels[i]) { gtk_tree_selection_select_iter(treeSelection, &iter); }
			else { gtk_tree_selection_unselect_iter(treeSelection, &iter); }
			if (!gtk_tree_model_iter_next(treeModel, &iter)) { break; }
		}
	}

	//finally, show the information dialog
	gtk_widget_show_all(dialog);
}

//Called when the user presses the apply button of the channel selection dialog
void MultiViewDialogApplyButtonCB(GtkButton* /*button*/, gpointer data)
{
	auto* view = reinterpret_cast<CSignalDisplayView*>(data);

	GtkTreeView* treeView           = GTK_TREE_VIEW(gtk_builder_get_object(view->m_Builder, "SignalDisplayMultiViewSelectList"));
	GtkTreeSelection* treeSelection = gtk_tree_view_get_selection(treeView);
	GtkTreeModel* treeModel         = gtk_tree_view_get_model(treeView);
	GtkTreeIter it;

	if (gtk_tree_model_get_iter_first(treeModel, &it)) {
		for (size_t i = 0; i < view->m_MultiViewSelectedChannels.size(); ++i) {
			view->m_MultiViewSelectedChannels[i] = (gtk_tree_selection_iter_is_selected(treeSelection, &it) != 0);
			if (!gtk_tree_model_iter_next(treeModel, &it)) { break; }
		}
	}

	view->ChangeMultiView();
	view->UpdateMainTableStatus();
	view->Redraw(); // immediate redraw

	//hides the channel selection dialog
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(view->m_Builder, "SignalDisplayMultiViewDialog")));
}
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
