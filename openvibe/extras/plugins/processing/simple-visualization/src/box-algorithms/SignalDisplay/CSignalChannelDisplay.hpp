///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalChannelDisplay.hpp
/// \brief Definition for the class CSignalChannelDisplay.
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

#include "../../defines.hpp"
#include "CSignalDisplayLeftRuler.hpp"

#include <glib.h>
#include <gtk/gtk.h>
#include <openvibe/ov_all.h>

#include <map>
#include <vector>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
class CSignalDisplayView;
class CBufferDatabase;

class CSignalChannelDisplay
{
public:
	/// <summary> Constructor. </summary>
	/// <param name="displayView"> Parent view. </param>
	/// <param name="channelDisplayW"> Width to be requested by widget. </param>
	/// <param name="channelDisplayH"> Height to be requested by widget. </param>
	/// <param name="leftRulerW"> Width to be requested by left ruler. </param>
	/// <param name="leftRulerH"> Height to be requested by left ruler. </param>
	CSignalChannelDisplay(CSignalDisplayView* displayView, int channelDisplayW, int channelDisplayH, int leftRulerW, int leftRulerH);

	/// <summary> Destructor. </summary>
	~CSignalChannelDisplay();

	/// <summary> Get ruler widget. </summary>
	/// <returns> Pointer to ruler widget. </returns>
	GtkWidget* GetRulerWidget(size_t index) const;

	/// <summary> Get signal display widget. </summary>
	/// <returns> Pointer to signal display widget. </returns>
	GtkWidget* GetSignalDisplayWidget() const { return m_DrawingArea; }

	/// <summary> Callback notified upon resize events. </summary>
	/// <param name="width"> New window width. </param>
	/// <param name="height"> New window height. </param>
	void OnResizeEventCB(gint width, gint height);

	/// <summary> Updates scale following a resize event or a time scale change. </summary>
	void UpdateScale();

	// Updates some drawing limits, i.e. to limit drawing to [chn_i,...,chn_j]
	void UpdateLimits();

	/// <summary> Reset list of channels displayed by this object. </summary>
	void ResetChannelList();

	/// <summary> Add a channel to the list of channels to be displayed. </summary>
	/// <param name="channel"> Index of channel to be displayed. </param>
	void AddChannel(size_t channel);

	void AddChannelList(size_t channel);

	/// <summary> Get rectangle to clear and redraw based on latest signal data received. </summary>
	/// <param name="rect"> Rectangle holding part of drawing area to clear and update. </param>
	void GetUpdateRectangle(GdkRectangle& rect) const;

	/// <summary> Flag widget so that its whole window is redrawn at next refresh. </summary>
	void RedrawAllAtNextRefresh(bool redraw);

	/// <summary> Check whether the whole window must be redrawn. </summary>
	/// <returns> True if the whole window must be redrawn, false otherwise. </returns>
	bool MustRedrawAll() const { return m_RedrawAll; }

	/// <summary> Draws the signal on the signal's drawing area. </summary>
	/// <param name="area"> Exposed area that needs to be redrawn. </param>
	void Draw(const GdkRectangle& area);

	/// <summary> Clip signals to drawing area. </summary>
	///
	/// Computes the list of points used to draw the lines (m_ParentDisplayView->m_Points) using the raw points list
	/// (m_ParentDisplayView->m_RawPoints) and by cropping the lines when they go out of the window.
	/// <param name="pointCount"> Number of points to clip. </param>
	/// <returns> The number of points to display. </returns>
	uint64_t CropCurve(uint64_t pointCount) const;

	/// <summary> Computes the parameters necessary for the signal to be zoomed at the selected coordinates. </summary>
	/// <param name="zoomIn"> If true, the operation is a zoom In, if false it's a zoom Out. </param>
	/// <param name="x"> The X-coordinate of the center of the area we want to zoom in. </param>
	/// <param name="y"> The Y-coordinate of the center of the area we want to zoom in. </param>
	void ComputeZoom(bool zoomIn, double x, double y);

	/// <summary> Returns empiric y min and maxes of the currently shown signal chunks for all subchannels. </summary>
	///
	/// Note that the actually used display limits may be different. 
	/// This function can be used to get the data extremal values and then use these to configure the display appropriately.
	void GetDisplayedValueRange(std::vector<double>& min, std::vector<double>& max) const;

	/// <summary> Sets scale for all subchannels. </summary>
	void SetGlobalScaleParameters(double min, double max, double margin);

	/// <summary> Sets scale for a single subchannel. </summary>
	void SetLocalScaleParameters(size_t subChannelIdx, double min, double max, double margin);

	/// <summary> Updates signal scale and translation based on latest global range and margin. </summary>
	void UpdateDisplayParameters();

private:
	/// <summary> Get first buffer to display index and position and first sample to display index. </summary>
	/// <param name="bufferIdx"> Index of first buffer to display. </param>
	/// <param name="sampleIdx"> Index of first sample to display. </param>
	/// <param name="bufferPos"> Position of first buffer to display (0-based, from left edge). </param>
	void getFirstBufferToDisplayInformation(size_t& bufferIdx, size_t& sampleIdx, size_t& bufferPos) const;

	/// <summary> Get start X coord of a buffer. </summary>
	/// <param name="pos"> Position of buffer on screen (0-based, from left edge). </param>
	/// <returns> Floored X coordinate of buffer. </returns>
	int getBufferStartX(size_t pos) const;

	/// <summary> Get X coordinate of a sample. </summary>
	/// <param name="bufferPos"> Position of buffer on screen (0-based, from left edge). </param>
	/// <param name="sampleIdx"> Index of sample in buffer. </param>
	/// <param name="offset"> X offset from which to start drawing. Used in scroll mode only. </param>
	/// <returns> X coordinate of sample. </returns>
	double getSampleXCoordinate(size_t bufferPos, size_t sampleIdx, double offset) const;

	/// <summary> Get Y coordinate of a sample. </summary>
	/// <param name="value"> Sample value and index of channel. </param>
	/// <param name="channelIdx"> Index of channel. </param>
	/// <returns> Y coordinate of sample. </returns>
	double getSampleYCoordinate(double value, size_t channelIdx) const;

	/// <summary> Get Y coordinate of a sample in Multiview mode. </summary>
	/// <param name="value"> Sample value and index of channel. </param>
	/// <returns> Y coordinate of sample. </returns>
	double getSampleYMultiViewCoordinate(double value) const;

	/// <summary> Draw signals (and stimulations, if any) displayed by this channel. </summary>
	/// <param name="firstBuffer"> Index of first buffer to display. </param>
	/// <param name="lastBuffer"> Index of last buffer to display. </param>
	/// <param name="firstSample"> Index of first sample to display in first buffer (subsequent buffers will start at sample 0). </param>
	/// <param name="startX"> Start X Coordinate. </param>
	/// <param name="firstChannel"> Index of first channel to display. </param>
	/// <param name="lastChannel"> Index of last channel to display. </param>
	/// <returns> True if all went ok, false otherwise. </returns>
	bool drawSignals(size_t firstBuffer, size_t lastBuffer, size_t firstSample, double startX, size_t firstChannel, size_t lastChannel) const;

	/// <summary> Draw vertical line highlighting where data was last drawn. </summary>
	void drawProgressLine(size_t firstBufferIdx, size_t firstBufferPos) const;

	/// <summary> Draw Y=0 line. </summary>
	void drawZeroLine() const;

public:
	//! Vector of Left rulers displaying signal scale. Indexed by channel id. @note This is a map as the active number of channels 
	// may change by the toolbar whereas this total set of rulers doesn't...
	std::map<size_t, CSignalDisplayLeftRuler*> m_LeftRuler;
	//! The drawing area where the signal is to be drawn
	GtkWidget* m_DrawingArea = nullptr;
	//! Drawing area dimensions, in pixels
	gint m_Width = 0, m_Height = 0;
	//! Available width per buffer, in pixels
	double m_WidthPerBuffer = 0;
	//! Available width per sample point, in pixels
	double m_PointStep = 0;
	//! The index list of the channels to display
	std::vector<size_t> m_ChannelList;
	//! The "parent" view (which uses this widget)
	CSignalDisplayView* m_ParentDisplayView = nullptr;
	//! The database from which the information are to be read
	CBufferDatabase* m_Database = nullptr;

	/** \ name Extrema of displayed values for all channel in this display */
	//@{
	std::vector<double> m_LocalMaximum, m_LocalMinimum;
	//@}

	/** \name Auto scaling parameters */
	//@{
	//		double m_ScaleX, m_TranslateX;

	std::vector<double> m_ScaleY, m_TranslateY;
	//@}

	/** \name Zooming parameters (user controlled) */
	//@{
	double m_ZoomTranslateX = 0, m_ZoomTranslateY = 0, m_ZoomScaleX = 1, m_ZoomScaleY = 1;
	//! The zoom factor step
	const double m_ZoomFactor = 1.5;
	//@}

	/** \name Scale margin parameters */
	//@{
	std::vector<double> m_OuterTopMargin, m_InnerTopMargin, m_InnerBottomMargin, m_OuterBottomMargin;
	//@}

	gint m_LeftRulerW = 0, m_LeftRulerH = 0;

	//! Current signal display mode
	EDisplayMode m_CurrentSignalMode = GlobalBestFit;
	//! Time of latest displayed data
	uint64_t m_LatestDisplayedTime = 0;

	//! Should the whole window be redrawn at next redraw?
	bool m_RedrawAll = false;

	//! Is it a multiview display ?
	bool m_MultiView = false;

	// These parameters control that we don't unnecessarily draw parts of the signal which are not in view

	// Currently visible y segment in the drawing area
	gint m_StartY = 0, m_StopY = 0;

	// First and last channel to draw
	size_t m_FirstChannelToDisplay = 0, m_LastChannelToDisplay = 0;
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
