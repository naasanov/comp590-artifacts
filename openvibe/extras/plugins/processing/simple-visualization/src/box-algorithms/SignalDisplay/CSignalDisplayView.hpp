///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalDisplayView.hpp
/// \brief Definition for the class CSignalDisplayView.
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

#include <openvibe/ov_all.h>
#include "CSignalChannelDisplay.hpp"
#include "../../CBufferDatabase.hpp"
#include "../../CBottomTimeRuler.hpp"
#include <gtk/gtk.h>
#include <map>
#include <string>
#include <array>
#include <vector>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
/**
* This class contains everything necessary to setup a GTK window and display
* a signal thanks to a CBufferDatabase's information.
*/
class CSignalDisplayView final : public CSignalDisplayDrawable
{
public:
	/// <summary> Constructor. </summary>
	/// <param name="buffer"> Signal database. </param>
	/// <param name="displayMode"> Initial signal display mode. </param>
	/// <param name="scalingMode"> Initial signal scaling mode. </param>
	/// <param name="verticalScale"> Initial vertical scale value. </param>
	/// <param name="verticalOffset"> Initial vertical offset value. </param>
	/// <param name="timeScale"> Initial time scale value. </param>
	/// <param name="horizontalRuler"> Initial horizontal ruler activation. </param>
	/// <param name="verticalRuler"> Initial vertical ruler activation. </param>
	/// <param name="multiview"> Initial multiview ruler activation. </param>
	CSignalDisplayView(CBufferDatabase& buffer, const CIdentifier& displayMode, const CIdentifier& scalingMode,
					   double verticalScale, double verticalOffset, double timeScale, bool horizontalRuler, bool verticalRuler, bool multiview);

	/// <summary> Base constructor. </summary>
	/// <param name="buffer"> Signal database. </param>
	/// <param name="timeScale"> Initial time scale value. </param>
	/// <param name="displayMode"> Initial signal display mode. </param>
	void Construct(CBufferDatabase& buffer, double timeScale, const CIdentifier& displayMode);

	/// <summary> Destructor. </summary>
	~CSignalDisplayView() override;
	/// <summary> Get pointers to plugin main widget and (optional) toolbar widget. </summary>
	/// <param name="widget"> Pointer to main widget. </param>
	/// <param name="toolbar"> Pointer to (optional) toolbar widget. </param>
	void GetWidgets(GtkWidget*& widget, GtkWidget*& toolbar) const;

	/// <summary> Initializes the window. </summary>
	void Init() override;
	/// <summary> Invalidates the window's content and tells it to redraw itself. </summary>
	void Redraw() override;
	/// <summary> Toggle left rulers on/off. </summary>
	/// <param name="active"> Show the ruler if true. </param>
	void ToggleLeftRulers(bool active);
	/// <summary> Toggle time ruler on/off. </summary>
	/// Toggle time ruler on/off
	/// <param name="active"> Show the ruler if true. </param>
	void ToggleBottomRuler(bool active);
	/// <summary> Toggle a channel on/off. </summary>
	/// <param name="index"> The index of the channel to toggle. </param>
	/// <param name="active"> Show the channel if true. </param>
	void ToggleChannel(size_t index, bool active);

	void ToggleChannelMultiView(bool active) const;

	void ChangeMultiView();

	void RemoveOldWidgets();
	void RecreateWidgets(size_t nChannel);
	void UpdateMainTableStatus();

	void UpdateDisplayTableSize() const;

	void ActivateToolbarButtons(bool active) const;

	/// <summary> Callback called when display mode changes. </summary>
	/// <param name="mode"> New display mode. </param>
	/// <returns> True. </returns>
	bool OnDisplayModeToggledCB(const CIdentifier& mode);
	bool OnUnitsToggledCB(bool active);

	/// <summary> Callback called when vertical scale mode changes. </summary>
	/// Callback called when vertical scale mode changes
	/// <param name="button"> Radio button toggled. </param>
	/// <returns> True. </returns>
	bool OnVerticalScaleModeToggledCB(GtkToggleButton* button);

	/// <summary> Callback called when custom vertical scale is changed. </summary>
	/// <param name="button"> Custom vertical scale widget. </param>
	/// <returns> True if custom vertical scale value could be retrieved, false otherwise. </returns>
	bool OnCustomVerticalScaleChangedCB(GtkSpinButton* button);
	bool OnCustomVerticalOffsetChangedCB(GtkSpinButton* button);

	/// <summary> Get a channel display object. </summary>
	/// <param name="index"> Index of channel display. </param>
	/// <returns> Channel display object. </returns>
	CSignalChannelDisplay* GetChannelDisplay(size_t index) const;

	bool IsChannelDisplayVisible(size_t index) const;

	void OnStimulationReceivedCB(uint64_t id, const CString& name);

	bool SetChannelUnits(const std::vector<std::pair<CString, CString>>& channelUnits);

	/// <summary> Get a color from a stimulation code. </summary>
	/// <param name="id"> Stimulation code. </param>
	/// <param name="color"> Color computed from stimulation code. </param>
	/// <remarks> Only the lower 32 bits of the stimulation code are currently used to compute the color. </remarks>
	void GetStimulationColor(uint64_t id, GdkColor& color);

	/// <summary> Get a color from a signal. </summary>
	/// <param name="index"> channel index. </param>
	/// <param name="color"> Color computed from stimulation code. </param>
	void GetMultiViewColor(size_t index, GdkColor& color);

	void RefreshScale();

private:
	/// <summary> Update stimulations color dialog with a new (stimulation, color) pair. </summary>
	/// <param name="stimulation"> Stimulation label. </param>
	/// <param name="color"> Stimulation color. </param>
	void updateStimulationColorsDialog(const CString& stimulation, const GdkColor& color) const;

public:
	//! The Builder handler used to create the interface
	GtkBuilder* m_Builder = nullptr;

	//! The table containing the CSignalChannelDisplays
	GtkWidget* m_SignalDisplayTable = nullptr;
	GtkWidget* m_Separator          = nullptr;

	//! Array of the channel's labels
	std::vector<GtkWidget*> m_ChannelLabel;

	//! Array of CSignalChannelDisplays (one per channel, displays the corresponding channel)
	std::vector<CSignalChannelDisplay*> m_ChannelDisplay;
	std::map<size_t, GtkWidget*> m_Separators;

	//! Show left rulers when true
	bool m_ShowLeftRulers = false;

	//!Show bottom time ruler when true
	bool m_ShowBottomRuler = false;

	//! Time of displayed signals at the left of channel displays
	uint64_t m_LeftmostDisplayedTime = 0;

	//! Largest displayed value range, to be matched by all channels in global best fit mode
	double m_LargestDisplayedValueRange = 0;
	double m_LargestDisplayedValue      = 0;
	double m_SmallestDisplayedValue     = 0;

	//! Current value range margin, used to avoid redrawing signals every time the largest value range changes
	double m_ValueRangeMargin = 0;
	// double m_ValueMaxMargin;
	/*! Margins added to largest and subtracted from smallest displayed values are computed as :
	m_MarginFactor * m_LargestDisplayedValueRange. If m_MarginFactor = 0, there's no margin at all.
	If factor is 0.1, largest displayed value range is extended by 10% above and below its extremums at the time
	when margins are computed. */
	double m_MarginFactor = 0.4;

	//! Normal/zooming cursors
	std::array<GdkCursor*, 2> m_Cursor;

	/** \name Vertical scale */
	//@{
	//! Flag set to true when you'd like the display to *check* if the scale needs to change and possibly update
	bool m_VerticalScaleRefresh = true;
	//! Flag set to true when you'd like the display to update in any case
	bool m_VerticalScaleForceUpdate = true;
	//! Value of custom vertical scale
	double m_CustomVerticalScaleValue = 0;
	//! Value of custom vertical offset
	double m_CustomVerticalOffset = 0;
	//@}

	//! The database that contains the information to use to draw the signals
	CBufferDatabase* m_Buffer = nullptr;

	//! Vector of gdk points. Used to draw the signals.
	std::vector<GdkPoint> m_Points;

	//! Vector of raw points. Stores the points' coordinates before cropping.
	std::vector<std::pair<double, double>> m_RawPoints;

	std::vector<CString> m_ChannelName;

	//! Vector of indexes of the channels to display
	std::map<size_t, bool> m_SelectedChannels;

	size_t m_NSelectedChannel = 0;

	std::map<size_t, std::pair<CString, CString>> m_ChannelUnits;

	//! Flag set to true once multi view configuration dialog is initialized
	bool m_MultiViewEnabled = false;

	//! Vector of indices of selected channels
	std::map<size_t, bool> m_MultiViewSelectedChannels;

	//Map of stimulation codes received so far, and their corresponding name and color
	std::map<uint64_t, std::pair<CString, GdkColor>> m_Stimulations;

	//Map of signal indices received so far, and their corresponding name and color
	std::map<uint64_t, std::pair<CString, GdkColor>> m_Signals;

	//! Bottom box containing bottom ruler
	GtkBox* m_BottomBox = nullptr;

	//! Bottom time ruler
	CBottomTimeRuler* m_BottomRuler = nullptr;

	//! Widgets for left rulers
	std::vector<GtkWidget*> m_LeftRulers;

	CIdentifier m_ScalingMode = CIdentifier::undefined();

	static const std::array<std::string, 3> SCALING_MODES;

	std::vector<CString> m_ErrorState;

	bool m_StimulationColorsShown = false;
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
