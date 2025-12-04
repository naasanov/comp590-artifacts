///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalChannelDisplay.cpp
/// \brief Implementation for the class CSignalChannelDisplay.
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

/*
 * Notes: 
 *
 * - currently this component actually never redraws by design. Calling the corresp. gtk functions for the whole
 *   set of buffers in memory causes the display to lag/freeze on Windows with big signals. If you 'fix' the
 *   redraw to actually work, make sure that the display runs smoothly in real-time on Win when Signal Display is in 
 *   full screen (maximized), 1000hz and 256 channels, including using scrollbar, alt-tab, resize, occluding windows, etc.
 *
 * - explanation of y margins: 
 *
 *   outerTop      ---
 *   innerTop      ---
 *   [a single signal channel here]
 *   innerBottom   ---
 *   outerBottom   ---
 *
 *   A channel is imagined to reside between the 'outer' top and bottom margins with a little headroom. 
 *   The automatic rescalers should react if the signal passes outside either of the 'inner' margins.
 */

#include "CSignalChannelDisplay.hpp"
#include "CSignalDisplayView.hpp"
#include <system/ovCTime.h>
#include <cmath>				// For unix system
#include <iostream>
#include "../../utils.hpp"

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

// #define DEBUG 1

// Redraw can be enabled by setting the following, however some bugs remain in some cases, esp.
// if the system is not able to render fast enough for the signal.
#define SUPPORT_REDRAW 0

static gboolean DrawingAreaExposeEventCB(GtkWidget* widget, GdkEventExpose* event, gpointer data);
static gboolean DrawingAreaConfigureCB(GtkWidget* widget, GdkEventExpose* event, gpointer data);
static gboolean DrawingAreaResizeEventCB(GtkWidget* widget, GtkAllocation* allocation, gpointer data);
static void DrawingAreaClickedEventCB(GtkWidget* widget, GdkEventButton* event, gpointer data);
static void DrawingAreaEnterEventCB(GtkWidget* widget, GdkEventCrossing* event, gpointer data);
static void DrawingAreaLeaveEventCB(GtkWidget* widget, GdkEventCrossing* event, gpointer data);
static gboolean VisibleRegionChangedCB(GtkWidget* widget, gpointer data);

CSignalChannelDisplay::CSignalChannelDisplay(CSignalDisplayView* displayView, const int channelDisplayW, const int channelDisplayH,
											 const int leftRulerW, const int leftRulerH)
	: m_DrawingArea(gtk_drawing_area_new()), m_ParentDisplayView(displayView), m_Database(displayView->m_Buffer),
	  m_LeftRulerW(leftRulerW), m_LeftRulerH(leftRulerH), m_StopY(leftRulerH)
{
	//creates the drawing area

	gtk_widget_set_size_request(m_DrawingArea, channelDisplayW, channelDisplayH);

	//Set background color (White)
	const GdkColor backgroundColor = InitGDKColor(0, 65535, 65535, 65535);

	gtk_widget_modify_bg(m_DrawingArea, GTK_STATE_NORMAL, &backgroundColor);

	//connects the signals
	gtk_widget_add_events(GTK_WIDGET(m_DrawingArea), GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(GTK_WIDGET(m_DrawingArea), GDK_ENTER_NOTIFY_MASK);
	gtk_widget_add_events(GTK_WIDGET(m_DrawingArea), GDK_LEAVE_NOTIFY_MASK);
	gtk_widget_add_events(GTK_WIDGET(m_DrawingArea), GDK_CONFIGURE);          // Size change

	g_signal_connect_after(G_OBJECT(m_DrawingArea), "expose-event", G_CALLBACK(DrawingAreaExposeEventCB), this);
	g_signal_connect_after(G_OBJECT(m_DrawingArea), "size-allocate", G_CALLBACK(DrawingAreaResizeEventCB), this);
	g_signal_connect_after(G_OBJECT(m_DrawingArea), "button-press-event", G_CALLBACK(DrawingAreaClickedEventCB), this);
	g_signal_connect_after(G_OBJECT(m_DrawingArea), "enter-notify-event", G_CALLBACK(DrawingAreaEnterEventCB), this);
	g_signal_connect_after(G_OBJECT(m_DrawingArea), "leave-notify-event", G_CALLBACK(DrawingAreaLeaveEventCB), this);
	g_signal_connect_after(G_OBJECT(m_DrawingArea), "configure-event", G_CALLBACK(DrawingAreaConfigureCB), this); // Size change, set draw limits

	// These take care of setting the redraw limits in the case of vertical scroll by user
	GtkWidget* widget   = GTK_WIDGET(gtk_builder_get_object(m_ParentDisplayView->m_Builder, "SignalDisplayChannelsScrolledWindow"));
	GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(widget));
	g_signal_connect_after(G_OBJECT(vadj), "value-changed", G_CALLBACK(VisibleRegionChangedCB), this);
}

CSignalChannelDisplay::~CSignalChannelDisplay()
{
	for (auto it = m_LeftRuler.begin(); it != m_LeftRuler.end(); ++it) { delete it->second; }
	m_LeftRuler.clear();

	/*
		m_TranslateY.clear();
		m_InnerTopMargin.clear();
		m_OuterTopMargin.clear();
		m_OuterBottomMargin.clear();
		m_InnerBottomMargin.clear();
		m_ScaleY.clear();*/
}

GtkWidget* CSignalChannelDisplay::GetRulerWidget(const size_t index) const
{
	const auto it = m_LeftRuler.find(index);
	if (it != m_LeftRuler.end() && it->second) { return it->second->GetWidget(); }
	return nullptr;
}

void CSignalChannelDisplay::OnResizeEventCB(const gint width, const gint height)
{
	m_Width  = width;
	m_Height = height;

	m_StartY = 0;
	m_StopY  = m_Height;

	UpdateScale();
}

void CSignalChannelDisplay::UpdateScale()
{
	const size_t samplesPerBuffer = m_Database->m_DimSizes[1];
	size_t nBufferToDisplay       = m_Database->m_NBufferToDisplay;
	if (samplesPerBuffer == 1 && nBufferToDisplay != 1) { nBufferToDisplay--; }

	m_WidthPerBuffer = double(m_Width) / double(nBufferToDisplay);

	m_PointStep = 0;
	if ((samplesPerBuffer * nBufferToDisplay) - 1 != 0) { m_PointStep = double(m_Width) / double((samplesPerBuffer * nBufferToDisplay) - 1); }

#ifdef DEBUG
	std::cout << "Requesting full redraw, C (updateScale)\n";
#endif
	RedrawAllAtNextRefresh(true);
}

void CSignalChannelDisplay::ResetChannelList()
{
	m_ChannelList.clear();
	m_TranslateY.clear();
	m_OuterTopMargin.clear();
	m_InnerTopMargin.clear();
	m_InnerBottomMargin.clear();
	m_OuterBottomMargin.clear();
	m_ScaleY.clear();
}

void CSignalChannelDisplay::AddChannel(const size_t channel)
{
	m_ChannelList.push_back(channel);
	m_LeftRuler[channel] = new CSignalDisplayLeftRuler(int(m_LeftRulerW), int(m_LeftRulerH));
	m_TranslateY.push_back(0);
	m_OuterTopMargin.push_back(0);
	m_InnerTopMargin.push_back(0);
	m_InnerBottomMargin.push_back(0);
	m_OuterBottomMargin.push_back(0);
	m_ScaleY.push_back(1);
}

// Adds a channel, but no ruler
void CSignalChannelDisplay::AddChannelList(const size_t channel)
{
	m_ChannelList.push_back(channel);
	m_TranslateY.push_back(0);
	m_OuterTopMargin.push_back(0);
	m_InnerTopMargin.push_back(0);
	m_InnerBottomMargin.push_back(0);
	m_OuterBottomMargin.push_back(0);
	m_ScaleY.push_back(1);
}

uint64_t CSignalChannelDisplay::CropCurve(const uint64_t pointCount) const
{
	if (pointCount == 0) { return 0; }

	//clears the vector of the points to draw
	m_ParentDisplayView->m_Points.clear();

	const std::vector<std::pair<double, double>>& curvePoints = m_ParentDisplayView->m_RawPoints;

	const GdkRegion* reg = gdk_drawable_get_visible_region(m_DrawingArea->window);
	GdkRectangle box;
	gdk_region_get_clipbox(reg, &box);

	const double yStart = box.y,
				 yStop  = box.y + box.height;

	//for each couple of successive points
	for (size_t i = 0; i < size_t(pointCount - 1); ++i) {
		//get the two points coordinates
		const double x0 = curvePoints[i].first, y0 = curvePoints[i].second, x1 = curvePoints[i + 1].first, y1 = curvePoints[i + 1].second;
		//if(!gdk_region_point_in(reg, x0, y0) || !gdk_region_point_in(reg, x1, y1)) { continue; }

		const bool firstOutTop    = (y0 < yStart), firstOutBottom  = (y0 >= yStop),
				   secondOutTop   = (y1 < yStart), secondOutBottom = (y1 >= yStop),
				   firstPointOut  = firstOutTop || firstOutBottom,
				   secondPointOut = secondOutTop || secondOutBottom;

		GdkPoint point;
		//if one of the points is out of the drawing area
		if (firstPointOut || secondPointOut) {
			if ((firstOutTop && secondOutTop) || (firstOutBottom && secondOutBottom)) { continue; }	// Both out and on the same side, forget about it

			//computes the line's coefficients
			const double a = (y1 - y0) / (x1 - x0),		// slope
						 b = y0 - (a * x0);				// intersect

			//if the first point is out of the window
			if (firstOutTop) {
				//computes its X-coordinate with the minimum Y
				point.x = gint(-b / a);
				//we take -1 and not 0, this way, the line between the two successive intersect points won't be drawn
				point.y = gint(yStart - 1);
				//adds it to the vector
				m_ParentDisplayView->m_Points.push_back(point);
			}
			else if (firstOutBottom) {
				//same with the maximum Y
				point.x = gint((yStop - b) / a);
				point.y = gint(yStop);
				m_ParentDisplayView->m_Points.push_back(point);
			}
			//if it is inside, keep its current coordinates
			else {
				point.x = gint(x0);
				point.y = gint(y0);
				m_ParentDisplayView->m_Points.push_back(point);
			}

			//if the second point is out of the window, computes its intersect point and adds it
			if (secondOutTop) {
				point.x = gint(-b / a);
				point.y = gint(yStart - 1);
				m_ParentDisplayView->m_Points.push_back(point);
			}
			else if (secondOutBottom) {
				point.x = gint((yStop - b) / a);
				point.y = gint(yStop);
				m_ParentDisplayView->m_Points.push_back(point);
			}
		}
		else //both points lie within the drawing area
		{
			//keep the first point
			point.x = gint(x0);
			point.y = gint(y0);
			m_ParentDisplayView->m_Points.push_back(point);

			//add the last point
			if (i == pointCount - 2) {
				point.x = gint(x1);
				point.y = gint(y1);
				m_ParentDisplayView->m_Points.push_back(point);
			}
		}

		// if(point.x<box.x || point.x>=box.x+box.width) { std::cout << "blam\n"; }	// assert
	}

	//return the number of points to draw
	return m_ParentDisplayView->m_Points.size();
}

void CSignalChannelDisplay::GetUpdateRectangle(GdkRectangle& rect) const
{
	rect.y      = m_StartY;
	rect.height = m_StopY - m_StartY;

	//if in scroll mode, or if redrawing everything, update the whole drawing area
	if (m_Database->GetDisplayMode() == Scroll || MustRedrawAll()) {
		rect.x     = 0;
		rect.width = m_Width;
	}
	else //partial redraw only
	{
		//determine index and position of first buffer to display, and index of first sample to display
		size_t bufferIdx = 0;
		size_t sampleIdx = 0;
		size_t bufferPos = 0;
		getFirstBufferToDisplayInformation(bufferIdx, sampleIdx, bufferPos);

		//X position of first sample that will be drawn when channel is refreshed
		const double startX = getSampleXCoordinate(bufferPos, sampleIdx, 0);

		//position on screen of latest buffer
		const auto latestBufferPosition = size_t(bufferPos + m_Database->m_SampleBuffers.size() - 1 - bufferIdx);

		//X position of last sample that will be drawn when channel is refreshed
		const auto samplesPerBuffer = size_t(m_Database->m_DimSizes[1]);
		const double endX           = getSampleXCoordinate(latestBufferPosition, samplesPerBuffer - 1, 0);

		rect.x     = gint(startX);
		rect.width = gint(endX) - gint(startX) + 1 /* this extra pixel accounts for vertical update line*/ + 1;
	}
}

#if SUPPORT_REDRAW
void CSignalChannelDisplay::redrawAllAtNextRefresh(bool redraw) { m_redrawAll = redraw; }
#else
void CSignalChannelDisplay::RedrawAllAtNextRefresh(bool /* redraw */) { m_RedrawAll = false; } // currently NOP, see comment at top
#endif

void CSignalChannelDisplay::Draw(const GdkRectangle& /*area*/)
{
	//ensure there is data to display
	if (!m_Database || m_Database->m_SampleBuffers.empty()) { return; }

#ifdef DEBUG
	uint64_t in = System::Time::zgetTime();
	if (mustRedrawAll()) { std::cout << "Draw(): RedrawAll was requested of " << this << "\n"; }
#endif

	const double sizePerChannel = m_Height / double(m_ChannelList.size());

	//updates the left ruler
	if (m_MultiView) {
		const double scaleY = (m_ScaleY[0] * m_ZoomScaleY * m_Height);
		const double max    = m_TranslateY[0] - ((0 - ((m_Height * m_ZoomScaleY) / 2) + (m_ZoomTranslateY * m_ZoomScaleY)) / scaleY);
		const double min    = m_TranslateY[0] - ((m_Height - ((m_Height * m_ZoomScaleY) / 2) + (m_ZoomTranslateY * m_ZoomScaleY)) / scaleY);

		m_LeftRuler[0]->Update(min, max);
	}
	else {
		// own ruler for each channel
		for (size_t i = m_FirstChannelToDisplay; i <= m_LastChannelToDisplay; ++i) {
			const double max = m_TranslateY[i] - ((0 - ((sizePerChannel * m_ZoomScaleY) / 2) + (m_ZoomTranslateY * m_ZoomScaleY)) / (
													  m_ScaleY[i] * m_ZoomScaleY * sizePerChannel));
			const double min = m_TranslateY[i] - ((sizePerChannel - ((sizePerChannel * m_ZoomScaleY) / 2) + (m_ZoomTranslateY * m_ZoomScaleY)) / (
													  m_ScaleY[i] * m_ZoomScaleY * sizePerChannel));

			m_LeftRuler[m_ChannelList[i]]->Update(min, max);
		}
	}

	//determine index and position of first (in the sense of leftmost) buffer to display, and index of first sample to display
	const auto samplesPerBuffer = size_t(m_Database->m_DimSizes[1]);
	size_t bufferIdx            = 0;
	size_t sampleIdx            = 0;
	size_t bufferPos            = 0;
	getFirstBufferToDisplayInformation(bufferIdx, sampleIdx, bufferPos);

	if (m_Database->GetDisplayMode() == Scan && !MustRedrawAll()) {
		//X position of last drawn sample (0 if restarting from left edge)
		const double startX = getSampleXCoordinate(bufferPos, sampleIdx, 0);

#if SUPPORT_REDRAW
		//position on screen of latest buffer
		const size_t latestBufferPosition = bufferPos + m_Database->m_SampleBuffers.size()-1- bufferIdx;

		//X position of last sample that will be drawn when channel is refreshed
		const double endX = getSampleXCoordinate(latestBufferPosition, samplesPerBuffer-1, 0);
		// is exposed area larger than the currently shown samples indicate?
		if(exposedArea.x < (gint)startX ||
			exposedArea.width-1/*exposed width is 1 pixel larger than asked for*/ > (gint)endX - (gint)startX + 1 + 1)
		{
#ifdef DEBUG
			std::cout << "Requesting full redraw, A (expose larger than sample area)\n";
#endif
			//this means the window was invalidated by an external widget : redraw it all
			redrawAllAtNextRefresh(true);
		
			m_Database->getIndexOfBufferStartingAtTime(m_ParentDisplayView->m_leftmostDisplayedTime, bufferIdx);
			bufferPos = 0;
		}
		else
#endif
		{
			//start drawing from at least one pixel to the left of first sample so that partial redraws connect well together
			const auto oldX = size_t(startX);
			size_t curX;
			do {
				if (sampleIdx == 0) {
					if (bufferPos == 0) { break; }

					bufferIdx--;
					bufferPos--;
					sampleIdx = samplesPerBuffer - 1;
				}
				else { sampleIdx--; }

				curX = size_t(getSampleXCoordinate(bufferPos, sampleIdx, 0));
			} while (curX >= oldX);
		}
	}

	//determine start x coord of first buffer to display
	double bufferStartX;
	if (m_Database->GetDisplayMode() == Scroll) {
		bufferStartX = double(m_Width) - double(m_Database->m_SampleBuffers.size()) * m_WidthPerBuffer;
		if (bufferStartX < 0) { bufferStartX = 0; }
	}
	else { bufferStartX = getSampleXCoordinate(bufferPos, 0, 0); }

	const auto lastBufferToDisplay = size_t(m_Database->m_SampleBuffers.size() - 1);


	// std::cout << "plot " << firstChannelToDisplay << "," << lastChannelToDisplay << "\n";


	//draw latest signals
	drawSignals(bufferIdx, lastBufferToDisplay, sampleIdx, bufferStartX, m_FirstChannelToDisplay, m_LastChannelToDisplay);

	//in scan mode, there is more to be drawn
	if (m_Database->GetDisplayMode() == Scan) {
		//draw progress line
		drawProgressLine(bufferIdx, bufferPos);

#if SUPPORT_REDRAW
		//if redrawing the whole window
		if(m_redrawAll == true && bufferIdx > 0)
		{
			//get start x coord of first buffer after the most recent one
			bufferStartX = getSampleXCoordinate(lastBufferToDisplay - bufferIdx + 1, 0, bufferStartX);

			//draw older signals (to the right of progress line)
			drawSignals(0, bufferIdx -1, 0, bufferStartX, m_FirstChannelToDisplay, m_LastChannelToDisplay);
		}
#else
		// We never redraw the whole window since with big signals (e.g. 256chns, 1000hz), 
		// this could cause rendering on Windows to freeze on resizes in a manner 
		// that the display doesn't recover
#endif
	}

	//draw Y=0 line
	drawZeroLine();

	//update time of latest displayed data
	m_LatestDisplayedTime = m_Database->m_EndTime.back();

#ifdef DEBUG
	uint64_t out = System::Time::zgetTime();
//	std::cout << "Elapsed2 " << CTime(out-in).toSeconds() << ", ld=" << m_latestDisplayedTime << "\n";
#endif

	//reset redraw all flag
	RedrawAllAtNextRefresh(false);
}

void CSignalChannelDisplay::ComputeZoom(const bool zoomIn, const double x, const double y)
{
	if (zoomIn) {
		m_ZoomTranslateX += (x - (m_Width / (m_ZoomFactor * 2))) / m_ZoomScaleX;
		m_ZoomTranslateY += (y - (m_Height / (m_ZoomFactor * 2))) / m_ZoomScaleY;

		m_ZoomScaleX *= m_ZoomFactor;
		m_ZoomScaleY *= m_ZoomFactor;
	}
	else {
		m_ZoomScaleX /= m_ZoomFactor;
		m_ZoomScaleY /= m_ZoomFactor;

		if (fabs(m_ZoomScaleY - 1) < 0.001) { m_ZoomScaleX = m_ZoomScaleY = 1.; }

		m_ZoomTranslateX -= (x - (m_Width / (m_ZoomFactor * 2))) / m_ZoomScaleX;
		m_ZoomTranslateY -= (y - (m_Height / (m_ZoomFactor * 2))) / m_ZoomScaleY;

		if (fabs(m_ZoomTranslateY) < 0.001) { m_ZoomTranslateX = m_ZoomTranslateY = 1.; }
	}

	//check if we are out of the window
	if (m_ZoomTranslateX < 0) { m_ZoomTranslateX = 0; }
	if (m_ZoomTranslateY < 0) { m_ZoomTranslateY = 0; }
	if (m_ZoomTranslateX > m_Width - (m_Width / m_ZoomScaleX)) { m_ZoomTranslateX = m_Width - (m_Width / m_ZoomScaleX); }
	if (m_ZoomTranslateY > m_Height - (m_Height / m_ZoomScaleY)) { m_ZoomTranslateY = m_Height - (m_Height / m_ZoomScaleY); }
	//Put a Y translation breaks the zoom out so let's set it to 0
	m_ZoomTranslateY = 0;
}

void CSignalChannelDisplay::GetDisplayedValueRange(std::vector<double>& min, std::vector<double>& max) const
{
	min.resize(m_ChannelList.size());
	max.resize(m_ChannelList.size());

	for (size_t k = 0; k < m_ChannelList.size(); ++k) {
		//update maximum and minimum values displayed by this channel
		double currentMin, currentMax;
		//get local min/max
		m_Database->GetDisplayedChannelLocalMinMaxValue(m_ChannelList[k], currentMin, currentMax);

		//set parameter to recomputed range
		min[k] = currentMin;
		max[k] = currentMax;
	}
}

void CSignalChannelDisplay::SetGlobalScaleParameters(const double min, const double max, const double margin)
{
	const double maxTop    = max + margin;
	const double minBottom = min - margin;

	for (size_t k = 0; k < m_ChannelList.size(); ++k) {
		m_OuterTopMargin[k] = maxTop;
		m_InnerTopMargin[k] = max;

		m_InnerBottomMargin[k] = min;
		m_OuterBottomMargin[k] = minBottom;
	}

	UpdateDisplayParameters();
}

void CSignalChannelDisplay::SetLocalScaleParameters(const size_t subChannelIdx, const double min, const double max, const double margin)
{
	m_OuterTopMargin[subChannelIdx]    = max + margin;
	m_InnerTopMargin[subChannelIdx]    = max;
	m_InnerBottomMargin[subChannelIdx] = min;
	m_OuterBottomMargin[subChannelIdx] = min - margin;
	//	std::cout << "Scaling to [" <<  min << "," << max << "]S\n";
}

// Assume [a,b] is the range between MinimumBottom and MaxiMumTop of k. If c \in [a,b],
// then m_scales[k]*(m_translates[k]-c) should be in what???
void CSignalChannelDisplay::UpdateDisplayParameters()
{
	//compute the translation needed to center the signal correctly in the window
	//    m_TranslateX = 0;

	for (size_t k = 0; k < m_ChannelList.size(); ++k) {
		if (std::fabs(m_OuterTopMargin[k] - m_OuterBottomMargin[k]) <= DBL_EPSILON) { m_ScaleY[k] = 1; }
		else { m_ScaleY[k] = 1 / (m_OuterTopMargin[k] - m_OuterBottomMargin[k]); }
		m_TranslateY[k] = (m_OuterTopMargin[k] + m_OuterBottomMargin[k]) / 2;
	}

	//reflect changes
#ifdef DEBUG
	std::cout << "Requesting full redraw, F (display params changed)\n";
#endif

#if SUPPORT_REDRAW
	redrawAllAtNextRefresh(true);
#else
	// Side effect: draw a little boxes to denote discontinuity in the signal due
	// to the runtime change of scale.  
	GdkColor lineColor = InitGDKColor(0, 65535, 0, 0);
	gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &lineColor);

	size_t bufferIdx = 0;
	size_t sampleIdx = 0;
	size_t bufferPos = 0;
	getFirstBufferToDisplayInformation(bufferIdx, sampleIdx, bufferPos);

	const GdkRegion* region = gdk_drawable_get_visible_region(m_DrawingArea->window);
	GdkRectangle box;
	gdk_region_get_clipbox(region, &box);

	const double startX = getSampleXCoordinate(bufferPos, sampleIdx, 0);
	gdk_draw_rectangle(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], true, gint(startX) - 2, box.y, gint(2), 4);
	gdk_draw_rectangle(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], true, gint(startX) - 2, box.y + box.height - 4,
					   gint(2), 4);

	lineColor.red   = 0 * 65535 / 255;
	lineColor.green = 0 * 65535 / 255;
	lineColor.blue  = 0 * 65535 / 255;
	gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &lineColor);

#endif
}

void CSignalChannelDisplay::getFirstBufferToDisplayInformation(size_t& bufferIdx, size_t& sampleIdx, size_t& bufferPos) const
{
	bufferIdx = 0;
	sampleIdx = 0;
	bufferPos = 0;

	if (m_Database->GetDisplayMode() == Scan) {
		if (m_RedrawAll) {
			m_Database->GetIndexOfBufferStartingAtTime(m_ParentDisplayView->m_LeftmostDisplayedTime, bufferIdx);
			return;
		}

		const bool haveLatestBufferDisplayed = m_Database->GetIndexOfBufferStartingAtTime(m_LatestDisplayedTime, bufferIdx);
		if (!haveLatestBufferDisplayed) {
#if SUPPORT_REDRAW
			//chances are drawing is up to date and this call was triggered following an "external" expose event
			//(e.g. the window was covered by a widget which was just moved, resulting in an expose event)
#ifdef DEBUG
			std::cout << "Requesting full redraw, B1 (buffer not displayed)\n";
#endif
			m_Database->getIndexOfBufferStartingAtTime(m_ParentDisplayView->m_leftmostDisplayedTime, rFirstBufferToDisplay);
			redrawAllAtNextRefresh(true);
			return;
#else
			//=>let's just start from the last sample 
			bufferIdx = size_t(m_Database->m_SampleBuffers.size() - 1);
#endif
		}

		//partial redraw
		size_t leftmostBufferIdx = 0;
		m_Database->GetIndexOfBufferStartingAtTime(m_ParentDisplayView->m_LeftmostDisplayedTime, leftmostBufferIdx);

		if (leftmostBufferIdx > bufferIdx) {
			// @fixme not sure why this happens...
			bufferPos = 0;
		}
		else {
			//get position of first new buffer
			bufferPos = bufferIdx - leftmostBufferIdx;

			//redraw from last sample of last drawn buffer, if we're not restarting from left edge
			if (bufferPos > 0) {
				bufferIdx--;
				bufferPos--;
				sampleIdx = size_t(m_Database->m_DimSizes[1]) - 1;
			}
		}
	}
}

int CSignalChannelDisplay::getBufferStartX(const size_t pos) const { return int((double(pos) * m_WidthPerBuffer - 0) * 1.0); }

double CSignalChannelDisplay::getSampleXCoordinate(const size_t bufferPos, const size_t sampleIdx, const double offset) const
{
	return double((offset + double(bufferPos) * m_WidthPerBuffer + double(sampleIdx) * m_PointStep - 0) * 1);
}

double CSignalChannelDisplay::getSampleYCoordinate(const double value, const size_t channelIdx) const
{
	//TODO : precompute some factors!
	const double sizePerChannel = m_Height / double(m_ChannelList.size());

	const double translatedData = m_TranslateY[channelIdx] - value;
	return m_ScaleY[channelIdx] * m_ZoomScaleY * sizePerChannel * translatedData
		   + double(channelIdx + 1) * sizePerChannel * m_ZoomScaleY - m_ZoomTranslateY * m_ZoomScaleY - sizePerChannel / 2;
}

double CSignalChannelDisplay::getSampleYMultiViewCoordinate(const double value) const
{
	const double translatedData = m_TranslateY[0] - value;
	return m_ScaleY[0] * m_ZoomScaleY * m_Height * translatedData + (m_Height * m_ZoomScaleY) / 2 - m_ZoomTranslateY * m_ZoomScaleY;
}

bool CSignalChannelDisplay::drawSignals(const size_t firstBuffer, const size_t lastBuffer, const size_t firstSample, const double startX,
										const size_t firstChannel, size_t lastChannel) const
{
	//compute and draw sample points
	const auto samplesPerBuffer = size_t(m_Database->m_DimSizes[1]);
	if (samplesPerBuffer == 0) { return false; }	// @FIXME silent fail, but no logManager here, so can't print

	GdkColor lineColor = InitGDKColor(0, 0, 0, 0);
	lastChannel        = std::min(lastChannel, size_t(m_ChannelList.size() - 1));

#ifdef DEBUG
	//	std::cout << "Channel range [" << firstChannelToDisplay << "," << lastChannelToDisplay << "]\n";
#endif

	for (size_t k = firstChannel; k <= lastChannel; ++k) {
		if (m_MultiView) { m_ParentDisplayView->GetMultiViewColor(m_ChannelList[k], lineColor); }
		else { if (m_CurrentSignalMode != GlobalBestFit) { lineColor.red = 65535; } }
		gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &lineColor);

		size_t index = 0;

		for (size_t j = firstBuffer; j <= lastBuffer; ++j) {
			const double* buffer = (m_Database->m_SampleBuffers)[j] + (m_ChannelList[k] * samplesPerBuffer);

			//for all samples in current buffer
			for (size_t i = (j == firstBuffer) ? firstSample : 0; i < samplesPerBuffer; ++i, ++index) {
				if (m_MultiView) {
					(m_ParentDisplayView->m_RawPoints)[index].first  = getSampleXCoordinate(j - firstBuffer, i, startX);
					(m_ParentDisplayView->m_RawPoints)[index].second = getSampleYMultiViewCoordinate(buffer[i]);
				}
				else {
					(m_ParentDisplayView->m_RawPoints)[index].first  = getSampleXCoordinate(j - firstBuffer, i, startX);
					(m_ParentDisplayView->m_RawPoints)[index].second = getSampleYCoordinate(buffer[i], k);
				}
			}
		}

		//crop points
		const uint64_t nPointsToDisplay = CropCurve(index);


#ifdef DEBUG
		if(numberOfPointsToDisplay>2000) { std::cout << "points " << numberOfPointsToDisplay << " in " << k << "\n"; }
#endif

		if (nPointsToDisplay != 0) {
			//draw all the points and link them
			gdk_draw_lines(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &m_ParentDisplayView->m_Points[0],
						   gint(nPointsToDisplay));
		}
	}

	if (!m_Database->m_Stimulations.empty()) {
		//switch to dashed line
		gdk_gc_set_line_attributes(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

		//compute current time window start and end time
#if 0
		uint64_t startTime = m_Database->m_startTimes[bufferIdx] +	m_Database->m_BufferDuration * sampleIdx / samplesPerBuffer;
		uint64_t endTime = m_Database->m_startTimes[lastBufferToDisplay] + m_Database->m_BufferDuration;
#else
		const uint64_t firstBufferDuration = m_Database->m_EndTime[firstBuffer] - m_Database->m_StartTime[firstBuffer];
		const uint64_t lastBufferDuration  = m_Database->m_EndTime[lastBuffer] - m_Database->m_StartTime[lastBuffer];
		const uint64_t startTime           = m_Database->m_StartTime[firstBuffer] + firstBufferDuration * firstSample / samplesPerBuffer;
		const uint64_t endTime             = m_Database->m_StartTime[lastBuffer] + lastBufferDuration;
#endif

		for (auto it = m_Database->m_Stimulations.begin(); it != m_Database->m_Stimulations.end(); ++it) {
			//look for stimulations lying in current time window
			if (it->first >= startTime && it->first <= endTime) {
				size_t j = firstBuffer;
				while (it->first > m_Database->m_EndTime[j]) { j++; }

				m_ParentDisplayView->GetStimulationColor(it->second, lineColor);
				gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &lineColor);

#if 0
				const auto i = size_t((it->first - m_Database->m_startTimes[j]) * samplesPerBuffer / m_Database->m_BufferDuration);
#else
				const uint64_t duration = m_Database->m_EndTime[j] - m_Database->m_StartTime[j];
				const auto i            = size_t((it->first - m_Database->m_StartTime[j]) * samplesPerBuffer / duration);
#endif
				const auto x = size_t(getSampleXCoordinate(j - firstBuffer, i, startX));
				gdk_draw_line(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], gint(x), m_StartY, gint(x), m_StopY);
			}
		}
	}

	lineColor.red   = 0;
	lineColor.green = 0;
	lineColor.blue  = 0;
	gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &lineColor);
	gdk_gc_set_line_attributes(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

	return true;
}

void CSignalChannelDisplay::drawProgressLine(const size_t firstBufferIdx, const size_t firstBufferPos) const
{
	//draw line only if there's more data to be drawn after it
	if (m_Database->m_SampleBuffers.size() < m_Database->m_NBufferToDisplay || m_ParentDisplayView->m_LeftmostDisplayedTime > m_Database->m_StartTime[0]) {
		const auto samplesPerBuffer = size_t(m_Database->m_DimSizes[1]);

		//position on screen of latest buffer
		const auto latestBufferPosition = size_t(firstBufferPos + m_Database->m_SampleBuffers.size() - 1 - firstBufferIdx);

		//X position of last sample that will be drawn when channel is refreshed
		const double endX = getSampleXCoordinate(latestBufferPosition, samplesPerBuffer - 1, 0);

		const GdkColor lineColor = InitGDKColor(0, 0, 65535, 0);
		gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &lineColor);

		//draw line one pixel after last sample
		gdk_draw_line(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], gint(endX) + 1, 0, gint(endX) + 1, m_Height - 1);

		const GdkColor black = InitGDKColor(0, 0, 0, 0);
		gdk_gc_set_rgb_fg_color(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], &black);
	}
}

void CSignalChannelDisplay::drawZeroLine() const
{
	//switch to dashed line
	gdk_gc_set_line_attributes(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

	const double sizePerChannel = m_Height / double(m_ChannelList.size());

	//draw Y=0 line
	if (m_MultiView) {
		const gint midPoint = gint(getSampleYMultiViewCoordinate(m_TranslateY[0]));
		const gint zeroY    = gint(getSampleYMultiViewCoordinate(0));
		if (zeroY >= 0 && zeroY < gint(m_Height) && std::abs(midPoint - zeroY) < sizePerChannel / 2.0) {
			gdk_draw_line(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 0, zeroY, m_Width, zeroY);
		}
	}
	else {
		for (size_t k = m_FirstChannelToDisplay; k <= m_LastChannelToDisplay; ++k) {
			const gint midPoint = gint(getSampleYCoordinate(m_TranslateY[k], k));
			const gint zeroY    = gint(getSampleYCoordinate(0, k));

			if (zeroY >= 0 && zeroY < gint(m_Height) && std::abs(midPoint - zeroY) < sizePerChannel / 2.0) {
				gdk_draw_line(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 0, zeroY, m_Width, zeroY);
			}
		}
	}

	//switch back to normal line
	gdk_gc_set_line_attributes(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);
}

void CSignalChannelDisplay::UpdateLimits()
{
	if (m_Height == 0) {
		// Bail out, no use setting the limits based on 0 ... this happens sometimes (todo: why?)
		// display->m_FirstChannelToDisplay = 0;
		// display->m_LastChannelToDisplay = display->m_ChannelList.size() - 1;
		return;
	}

	if (m_MultiView) {
		m_FirstChannelToDisplay = 0;
		m_LastChannelToDisplay  = size_t(m_ChannelList.size() - 1);

		// keep as is:  display->m_StartY,  display->m_StopY 
		return;
	}

	GtkWidget* widget = GTK_WIDGET(gtk_builder_get_object(m_ParentDisplayView->m_Builder, "SignalDisplayChannelsScrolledWindow"));

	const GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(widget));

	const double areaStartY = vadj->value;
	const double areaSizeY  = vadj->page_size;

	const double sizePerChannel = m_Height / double(m_ChannelList.size()) * m_ZoomScaleY;
	m_FirstChannelToDisplay     = size_t(std::floor(areaStartY / sizePerChannel));
	m_LastChannelToDisplay      = std::min(size_t(m_ChannelList.size() - 1), m_FirstChannelToDisplay + size_t(std::floor(areaSizeY / sizePerChannel)) + 1);

	m_StartY = gint(sizePerChannel * double(m_FirstChannelToDisplay));
	m_StopY  = std::min(m_Height, gint(sizePerChannel * double(m_LastChannelToDisplay + 1)));

#ifdef DEBUG
	std::cout << "Requesting full redraw, Q (updated limits)\n";
#endif

#if SUPPORT_REDRAW
	redrawAllAtNextRefresh(true);
#else
	//	gdk_window_clear(m_drawingArea->window);
	//	gtk_widget_queue_draw(static_cast<GtkWidget*>(m_drawingArea));
#endif

	// std::cout << "SetChns " << display->m_FirstChannelToDisplay << " to " << display->m_LastChannelToDisplay << "\n";
	// std::cout << "SetLim  " << display->m_StartY << " to " << display->m_StopY << "\n";
}

//
//CALLBACKS
//

// DrawingArea visible region may have changed, estimate the limits again
gboolean VisibleRegionChangedCB(GtkWidget* /*pWidget*/, gpointer data)
{
	auto* display = reinterpret_cast<CSignalChannelDisplay*>(data);
	display->UpdateLimits();
	return false; // propagate
}


gboolean DrawingAreaConfigureCB(GtkWidget* /*pWidget*/, GdkEventExpose* /*pEvent*/, gpointer data)
{
	auto* display = reinterpret_cast<CSignalChannelDisplay*>(data);
	display->UpdateLimits();
	return false; // propagate
}

gboolean DrawingAreaExposeEventCB(GtkWidget* /*widget*/, GdkEventExpose* event, gpointer data)
{
	//	std::cout << "EE for " << pEvent->area.x << " " << pEvent->area.y <<  "  " << pEvent->area.width << " " << pEvent->area.height << "\n";

	auto* display = reinterpret_cast<CSignalChannelDisplay*>(data);

#if SUPPORT_REDRAW
	//check if a full redrawn was asked for
	if(pEvent->area.width == (gint)display->m_Width && pEvent->area.height == (gint)display->m_Height)
	{
#ifdef DEBUG
		std::cout << "Requesting full redraw, G (full window exposed)\n";
#endif
		display->redrawAllAtNextRefresh(true);
	}
#endif

	//redraw signals
	display->Draw(event->area);

	//don't propagate this signal to the children if any
	return TRUE;
}

gboolean DrawingAreaResizeEventCB(GtkWidget* /*widget*/, GtkAllocation* allocation, gpointer data)
{
	auto* display = reinterpret_cast<CSignalChannelDisplay*>(data);
	display->OnResizeEventCB(allocation->width, allocation->height);
	display->UpdateLimits();
	return FALSE;
}

void DrawingAreaClickedEventCB(GtkWidget* /*widget*/, GdkEventButton* event, gpointer data)
{
	if (event->type != GDK_BUTTON_PRESS) { return; }

	auto* display                = reinterpret_cast<CSignalChannelDisplay*>(data);
	bool zoomChanged             = false;
	display->m_CurrentSignalMode = GlobalBestFit;

	if (event->button == 1) {
		display->m_CurrentSignalMode = ZoomIn;
		display->ComputeZoom(true, event->x, event->y);
		zoomChanged = true;
	}
	else if (event->button == 3) {
		if (display->m_ZoomScaleY != 1.0) {
			display->m_CurrentSignalMode = ZoomOut;
			display->ComputeZoom(false, event->x, event->y);
			zoomChanged = true;

			if (display->m_ZoomScaleY == 1.0) {
				display->m_CurrentSignalMode = GlobalBestFit;
				display->UpdateDisplayParameters();
			}
			else { display->m_CurrentSignalMode = ZoomOut; }
		}
	}

	//if the zoom level has changed, redraw the signal and left ruler
	if (zoomChanged) {
		display->RedrawAllAtNextRefresh(true);
		if (GTK_WIDGET(display->m_DrawingArea)->window) { gdk_window_invalidate_rect(GTK_WIDGET(display->m_DrawingArea)->window, nullptr, true); }

		for (auto it = display->m_LeftRuler.begin(); it != display->m_LeftRuler.end(); ++it) {
			if (GTK_WIDGET(it->second->GetWidget())->window) { gdk_window_invalidate_rect(GTK_WIDGET(it->second->GetWidget())->window, nullptr, true); }
		}
	}
}

void DrawingAreaEnterEventCB(GtkWidget* widget, GdkEventCrossing* /*event*/, gpointer data)
{
	const auto* display = reinterpret_cast<CSignalChannelDisplay*>(data);

	//change the cursor to the zooming one
	gdk_window_set_cursor(widget->window, display->m_ParentDisplayView->m_Cursor[1]);
}

void DrawingAreaLeaveEventCB(GtkWidget* widget, GdkEventCrossing* /*event*/, gpointer data)
{
	const auto* display = reinterpret_cast<CSignalChannelDisplay*>(data);

	//change the cursor back to the normal one
	gdk_window_set_cursor(widget->window, display->m_ParentDisplayView->m_Cursor[0]);
}
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
