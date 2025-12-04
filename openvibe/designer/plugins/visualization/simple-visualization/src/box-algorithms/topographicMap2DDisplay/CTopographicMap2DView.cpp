#include "CTopographicMap2DView.hpp"

#include <iostream>

#include <cmath>
#include <cstring>
#include <array>

#ifdef TARGET_OS_Windows
#ifndef NDEBUG
#include <cassert>
#endif
#endif

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

//#define INTERPOLATE_AT_CHANNEL_LOCATION

static const size_t NB_COLORS = 13;
static std::array<GdkColor, 13> palette;
static std::array<uint8_t, 39> palette8;	// 13 * 3 (RGB)

static double Deg2Rad(const double x) { return x * 0.01745329251994329576923690768489; }
static gboolean redrawCallback(GtkWidget* widget, GdkEventExpose* event, gpointer data);
static gboolean resizeCallback(GtkWidget* widget, GtkAllocation* allocation, gpointer data);
static void toggleElectrodesCallback(GtkWidget* widget, gpointer data);
static void setProjectionCallback(GtkWidget* widget, gpointer data);
static void setViewCallback(GtkWidget* widget, gpointer data);
static void setInterpolationCallback(GtkWidget* widget, gpointer data);
static void setDelayCallback(GtkRange* range, gpointer data);

CTopographicMap2DView::CTopographicMap2DView(CTopographicMapDatabase& mapDatabase, const EInterpolationType interpolation, double delay)
	: m_mapDatabase(mapDatabase), m_currentInterpolation(interpolation)
{
	m_sampleCoordinatesMatrix.setDimensionCount(2);

	//load the gtk builder interface
	m_builderInterface = gtk_builder_new();
	gtk_builder_add_from_file(m_builderInterface, Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-TopographicMap2D.ui",
							  nullptr);

	if (m_builderInterface == nullptr) {
		g_warning("Couldn't load the interface!");
		return;
	}

	gtk_builder_connect_signals(m_builderInterface, nullptr);

	m_bgColor.pixel = 0;
	m_bgColor.red   = 0xFFFF;
	m_bgColor.green = 0;//0xFFFF;
	m_bgColor.blue  = 0;//0xFFFF;

	//toolbar
	//-------

	//get pointers to projection mode buttons
	m_axialProjectionButton  = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "AxialProjection"));
	m_radialProjectionButton = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "RadialProjection"));

	g_signal_connect(G_OBJECT(m_axialProjectionButton), "toggled", G_CALLBACK(setProjectionCallback), this);
	g_signal_connect(G_OBJECT(m_radialProjectionButton), "toggled", G_CALLBACK(setProjectionCallback), this);

	//get pointers to view buttons
	m_topViewButton   = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "TopView"));
	m_leftViewButton  = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "LeftView"));
	m_rightViewButton = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "RightView"));
	m_backViewButton  = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "BackView"));

	g_signal_connect(G_OBJECT(m_topViewButton), "toggled", G_CALLBACK(setViewCallback), this);
	g_signal_connect(G_OBJECT(m_leftViewButton), "toggled", G_CALLBACK(setViewCallback), this);
	g_signal_connect(G_OBJECT(m_rightViewButton), "toggled", G_CALLBACK(setViewCallback), this);
	g_signal_connect(G_OBJECT(m_backViewButton), "toggled", G_CALLBACK(setViewCallback), this);

	//get pointers to interpolation type buttons
	m_mapPotentials = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "MapPotentials"));
	m_mapCurrents   = GTK_RADIO_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "MapCurrents"));

	g_signal_connect(G_OBJECT(m_mapPotentials), "toggled", G_CALLBACK(setInterpolationCallback), this);
	g_signal_connect(G_OBJECT(m_mapCurrents), "toggled", G_CALLBACK(setInterpolationCallback), this);

	//get pointer to electrodes toggle button
	m_electrodesToggleButton = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(m_builderInterface, "ToggleElectrodes"));

	g_signal_connect(G_OBJECT(m_electrodesToggleButton), "toggled", G_CALLBACK(toggleElectrodesCallback), this);

	//tell database about maximum delay
	m_mapDatabase.AdjustNumberOfDisplayedBuffers(m_maxDelay);
	//ensure default delay lies in [0, m_maxDelay]
	if (delay > m_maxDelay) { delay = m_maxDelay; }
	//set default delay
	SetDelayCB(delay);
	//configure delay slider
	GtkWidget* delayScale = gtk_hscale_new_with_range(0.0, m_maxDelay, 0.1);
	gtk_range_set_value(GTK_RANGE(delayScale), delay);
	gtk_scale_set_value_pos(GTK_SCALE(delayScale), GTK_POS_TOP);
	gtk_range_set_update_policy(GTK_RANGE(delayScale), GTK_UPDATE_CONTINUOUS);
	gtk_widget_set_size_request(delayScale, 100, -1);
	gtk_widget_show_all(delayScale);
	g_signal_connect(G_OBJECT(delayScale), "value_changed", G_CALLBACK(setDelayCallback), this);

	//replace existing scale (which somehow can't be used) with the newly created one
	GtkWidget* oldScale    = GTK_WIDGET(gtk_builder_get_object(m_builderInterface, "DelayScale"));
	GtkWidget* scaleParent = gtk_widget_get_parent(oldScale);
	if (scaleParent != nullptr && GTK_IS_CONTAINER(scaleParent)) {
		gtk_container_remove(GTK_CONTAINER(scaleParent), oldScale);
		if (GTK_IS_BOX(scaleParent)) {
			gtk_box_pack_start(GTK_BOX(scaleParent), delayScale, TRUE, TRUE, 0);
			gtk_box_reorder_child(GTK_BOX(scaleParent), delayScale, 0);
		}
	}

	//color palettes
	palette8 = {
		255, 0, 0,
		234, 1, 0,
		205, 0, 101,
		153, 0, 178,
		115, 1, 177,
		77, 0, 178,
		0, 0, 152,
		0, 97, 121,
		0, 164, 100,
		0, 225, 25,
		150, 255, 0,
		200, 255, 0,
		255, 255, 0
	};

	const auto factor = 65535 / 255;	// 257
	for (size_t i = 0, j = 0; i < NB_COLORS; ++i) {
		palette[i].red   = palette8[j++] * factor;
		palette[i].green = palette8[j++] * factor;
		palette[i].blue  = palette8[j++] * factor;
	}
}

CTopographicMap2DView::~CTopographicMap2DView()
{
	//destroy clip mask
	if (m_clipmask != nullptr) {
		g_object_unref(m_clipmask);
		m_clipmask = nullptr;
	}
	if (m_clipmaskGC != nullptr) {
		g_object_unref(m_clipmaskGC);
		m_clipmaskGC = nullptr;
	}
	//destroy visible region
	if (m_visibleRegion != nullptr) {
		gdk_region_destroy(m_visibleRegion);
		m_visibleRegion = nullptr;
	}
	//destroy pixmap
	m_skullRGBBuffer.clear();

	//unref the xml file as it's not needed anymore
	g_object_unref(G_OBJECT(m_builderInterface));
	m_builderInterface = nullptr;
}

void CTopographicMap2DView::Init()
{
	m_drawingArea = GTK_WIDGET(gtk_builder_get_object(m_builderInterface, "TopographicMap2DDrawingArea"));

	gtk_widget_set_double_buffered(m_drawingArea, TRUE);

	g_signal_connect(G_OBJECT(m_drawingArea), "expose-event", G_CALLBACK(redrawCallback), this);
	g_signal_connect(G_OBJECT(m_drawingArea), "size-allocate", G_CALLBACK(resizeCallback), this);

	gtk_widget_show(m_drawingArea);

	//set radial projection by default
	m_currentProjection = EProjection::Radial;
	enableProjectionButtonSignals(false);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(m_radialProjectionButton), TRUE);
	enableProjectionButtonSignals(true);

	//set top view by default
	m_currentView = EView::Top;
	enableViewButtonSignals(false);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(m_topViewButton), TRUE);
	enableViewButtonSignals(true);

	//reflect default interpolation type
	m_mapDatabase.SetInterpolationType(m_currentInterpolation);
	enableInterpolationButtonSignals(false);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(m_mapPotentials), gboolean(m_currentInterpolation == EInterpolationType::Spline));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(m_mapCurrents), gboolean(m_currentInterpolation == EInterpolationType::Laplacian));
	enableInterpolationButtonSignals(true);

	//hide electrodes by default
	m_electrodesToggledOn = false;
	enableElectrodeButtonSignals(false);
	gtk_toggle_tool_button_set_active(m_electrodesToggleButton, gboolean(m_electrodesToggledOn));
	enableElectrodeButtonSignals(true);

	//recompute sample points coordinates
	m_needResize = true;
}

void CTopographicMap2DView::Redraw()
{
	if (m_drawingArea != nullptr && GTK_WIDGET_VISIBLE(m_drawingArea)) {
		if (m_needResize) { resizeData(); }

		//draw face
		drawFace(0, 0, m_headWindowWidth, m_headWindowHeight);

		//draw head
		drawHead();

		//draw palette
		drawPalette(0, m_headWindowHeight, m_paletteWindowWidth, m_paletteWindowHeight);

		//don't clear screen at every redraw, it introduces major flickering
		//gdk_window_invalidate_rect(m_drawingArea->window, nullptr, true);
	}
}

void CTopographicMap2DView::GetWidgets(GtkWidget* & widget, GtkWidget* & toolbar) const
{
	widget  = GTK_WIDGET(gtk_builder_get_object(m_builderInterface, "TopographicMap2DDrawingArea"));
	toolbar = GTK_WIDGET(gtk_builder_get_object(m_builderInterface, "Toolbar"));
}

CMatrix* CTopographicMap2DView::GetSampleCoordinatesMatrix()
{
	if (m_needResize) { resizeData(); }
	return &m_sampleCoordinatesMatrix;
}

bool CTopographicMap2DView::SetSampleValuesMatrix(CMatrix* matrix)
{
	//ensure matrix has the right size
	if (matrix == nullptr || matrix->getBufferElementCount() < m_sampleValues.size()) { return false; }

	//retrieve min/max potentials
	double minPotential, maxPotential;
	m_mapDatabase.GetLastBufferInterpolatedMinMaxValue(minPotential, maxPotential);

	const size_t colorStartIdx = 0;
	const size_t colorEndIdx   = NB_COLORS - 1;

	double invPotentialStep = 0;

	if (minPotential < maxPotential) { invPotentialStep = (colorEndIdx - colorStartIdx + 1) / (maxPotential - minPotential); }

	//determine color index of each sample
	for (size_t i = 0; i < m_sampleValues.size(); ++i) {
		const double value = *(matrix->getBuffer() + i);
		size_t colorIdx;

		if (value < minPotential) { colorIdx = 0; }
		else if (value > maxPotential) { colorIdx = NB_COLORS - 1; }
		else //linear itp
		{
			colorIdx = colorStartIdx + int((value - minPotential) * invPotentialStep);
			if (colorIdx > NB_COLORS - 1) { colorIdx = NB_COLORS - 1; }
		}
		m_sampleValues[i] = colorIdx;
	}

	refreshPotentials();

	//force redraw

	return true;
}

void CTopographicMap2DView::ToggleElectrodesCB()
{
	m_electrodesToggledOn = !m_electrodesToggledOn;

	if (!m_electrodesToggledOn) {
		//clear screen so that electrode labels are hidden
		if (m_drawingArea->window != nullptr) { gdk_window_invalidate_rect(m_drawingArea->window, nullptr, 1); }
	}
}

void CTopographicMap2DView::SetProjectionCB(GtkWidget* widget)
{
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) == FALSE) { return; }

	if (widget == GTK_WIDGET(m_axialProjectionButton)) { m_currentProjection = EProjection::Axial; }
	else if (widget == GTK_WIDGET(m_radialProjectionButton)) { m_currentProjection = EProjection::Radial; }

	//recompute sample points coordinates
	m_needResize = true;

	//clear screen
	if (m_drawingArea->window != nullptr) { gdk_window_invalidate_rect(m_drawingArea->window, nullptr, 1); }
}

void CTopographicMap2DView::SetViewCB(GtkWidget* widget)
{
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) == FALSE) { return; }

	if (widget == GTK_WIDGET(m_topViewButton)) { m_currentView = EView::Top; }
	else if (widget == GTK_WIDGET(m_leftViewButton)) { m_currentView = EView::Left; }
	else if (widget == GTK_WIDGET(m_rightViewButton)) { m_currentView = EView::Right; }
	else if (widget == GTK_WIDGET(m_backViewButton)) { m_currentView = EView::Back; }

	//recompute sample points coordinates, update clipmask
	m_needResize = true;

	//clear screen
	if (m_drawingArea->window != nullptr) { gdk_window_invalidate_rect(m_drawingArea->window, nullptr, 1); }
}

void CTopographicMap2DView::SetInterpolationCB(GtkWidget* widget)
{
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)) == FALSE) { return; }

	if (widget == GTK_WIDGET(m_mapPotentials)) {
		m_currentInterpolation = EInterpolationType::Spline;
		m_mapDatabase.SetInterpolationType(EInterpolationType::Spline);
	}
	else if (widget == GTK_WIDGET(m_mapCurrents)) {
		m_currentInterpolation = EInterpolationType::Laplacian;
		m_mapDatabase.SetInterpolationType(EInterpolationType::Laplacian);
	}

	//recompute sample points coordinates
	m_needResize = true;
}

void CTopographicMap2DView::drawPalette(const size_t x, const size_t y, const size_t width, const size_t height) const
{
	if (width == 0 || height == 0) { return; }

	// FIXME is it necessary to keep next line uncomment ?
	//bool drawText = true;

	//retrieve text size
	PangoLayout* text = gtk_widget_create_pango_layout(GTK_WIDGET(m_drawingArea), "0");
	gint textHeight;
	pango_layout_get_pixel_size(text, nullptr, &textHeight);

	//don't draw text if not enough room
	if (textHeight >= gint(height - m_minPaletteBarHeight)) {
		// FIXME is it necessary to keep next line uncomment ?
		//drawText = false;
	}
	//determine palette bar dims
	const gint barWidth = gint(0.9 * double(width));
	gint barHeight      = gint(height - textHeight);
	if (barHeight < gint(m_minPaletteBarHeight)) { barHeight = gint(m_minPaletteBarHeight); }
	else if (barHeight > gint(m_maxPaletteBarHeight)) { barHeight = gint(m_maxPaletteBarHeight); }
	const gint barStartX = gint(x + (width - barWidth) / 2);
	const gint barStartY = gint(y);

	gint textWidth;
	const gint labelY = gint(barStartY + barHeight + (height - barHeight - textHeight) / 2);

	//draw 0 label
	pango_layout_get_pixel_size(text, &textWidth, nullptr);
	gint labelX = gint(x + (width - textWidth) / 2);

	gdk_draw_layout(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], labelX, labelY, text);

	//draw + label
	pango_layout_set_text(text, "+", 1);
	pango_layout_get_pixel_size(text, &textWidth, nullptr);
	labelX = barStartX - textWidth / 2;
	if (labelX < 0) { labelX = 0; }
	gdk_draw_layout(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], labelX, labelY, text);

	//draw - label
	pango_layout_set_text(text, "-", 1);
	pango_layout_get_pixel_size(text, &textWidth, nullptr);
	labelX = barStartX + barWidth - textWidth / 2;
	if (labelX + textWidth >= gint(width)) { labelX = gint(width) - textWidth; }
	gdk_draw_layout(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], labelX, labelY, text);

	//draw palette bar (typically reversed : high potentials to the left; low potentials to the right)
	gint currentX = barStartX;

	for (int i = NB_COLORS - 1; i >= 0; i--) {
		gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &palette[i]);

		gdk_draw_rectangle(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)],
						   TRUE, currentX, barStartY, barWidth / gint(NB_COLORS), barHeight);

		currentX += barWidth / 13;
	}

	//restore default black color
	GdkColor black;
	black.red = black.green = black.blue = 0;
	gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &black);

	//delete pango layout
	g_object_unref(text);
}

void CTopographicMap2DView::drawFace(size_t /*X*/, size_t /*Y*/, size_t /*width*/, size_t /*height*/) const
{
	gdk_gc_set_line_attributes(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

	//head center
	const double skullRadius  = double(m_skullDiameter) / 2.0;
	const double skullCenterX = double(m_skullX) + skullRadius;
	const double skullCenterY = double(m_skullY) + skullRadius;

	if (m_currentView == EView::Top) {
		const double noseHalfAngle = 6;
		//nose lower left/right anchor
		const gint lx = gint(size_t(skullCenterX + 1.0 * skullRadius * cos(Deg2Rad(90 + noseHalfAngle))));
		const gint ly = gint(size_t(skullCenterY - 1.0 * skullRadius * sin(Deg2Rad(90 + noseHalfAngle))));
		const gint rx = gint(size_t(skullCenterX + 1.0 * skullRadius * cos(Deg2Rad(90 - noseHalfAngle))));
		const gint ry = gint(size_t(skullCenterY - 1.0 * skullRadius * sin(Deg2Rad(90 - noseHalfAngle))));

		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], lx, ly, gint(skullCenterX), gint(m_noseY));
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], rx, ry, gint(skullCenterX), gint(m_noseY));
	}
	else if (m_currentView == EView::Back) {
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_skullOutlineLeftPointX),
					  gint(m_skullOutlineLeftPointY), gint(m_leftNeckX), gint(m_leftNeckY));
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_skullOutlineRightPointX),
					  gint(m_skullOutlineRightPointY), gint(m_rightNeckX), gint(m_rightNeckY));
	}
	else if (m_currentView == EView::Left || m_currentView == EView::Right) {
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_noseTopX), gint(m_noseTopY),
					  gint(m_noseBumpX), gint(m_noseBumpY));
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_noseBumpX), gint(m_noseBumpY),
					  gint(m_noseTipX), gint(m_noseTipY));
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_noseTipX), gint(m_noseTipY),
					  gint(m_noseBaseX), gint(m_noseBaseY));
		gdk_draw_line(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_noseBaseX), gint(m_noseBaseY),
					  gint(m_noseBottomX), gint(m_noseBottomY));
	}
}

void CTopographicMap2DView::drawHead() const
{
	//draw head outline
	gdk_gc_set_line_attributes(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);
	gdk_draw_arc(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], FALSE, gint(m_skullX), gint(m_skullY),
				 gint(m_skullDiameter), gint(m_skullDiameter), gint(64 * m_skullOutlineStartAngle),
				 gint(64 * (m_skullOutlineEndAngle - m_skullOutlineStartAngle)));
	gdk_gc_set_line_attributes(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);
	gdk_gc_set_clip_origin(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_skullX), gint(m_skullY));
	gdk_gc_set_clip_mask(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], m_clipmask);

	drawPotentials();
	drawElectrodes();

	gdk_gc_set_clip_mask(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], nullptr);
}

void CTopographicMap2DView::resizeData()
{
	//window size
	const size_t width  = m_drawingArea->allocation.width;
	const size_t height = m_drawingArea->allocation.height;

	//retrieve text size
	PangoLayout* text = gtk_widget_create_pango_layout(GTK_WIDGET(m_drawingArea), "0");
	int textHeight;
	pango_layout_get_pixel_size(text, nullptr, &textHeight);
	g_object_unref(text);
	text = nullptr;

	//palette sub-window dims
	m_paletteWindowWidth  = width;
	m_paletteWindowHeight = size_t(0.1 * double(height));
	if (m_paletteWindowHeight > size_t(m_maxPaletteBarHeight + textHeight + 4)) { m_paletteWindowHeight = m_maxPaletteBarHeight + textHeight + 4; }
	else if (m_paletteWindowHeight < size_t(m_minPaletteBarHeight + textHeight)) { m_paletteWindowHeight = size_t(m_minPaletteBarHeight + textHeight); }

	//return if not enough room available
	if (m_paletteWindowHeight > height) { return; }

	//head sub-window dims
	m_headWindowWidth  = width;
	m_headWindowHeight = height - m_paletteWindowHeight;

	size_t headMaxSize;
	if (m_headWindowWidth < m_headWindowHeight) { headMaxSize = size_t(0.9 * double(m_headWindowWidth)); }
	else { headMaxSize = size_t(0.9 * double(m_headWindowHeight)); }

	if (m_currentView == EView::Top) {
		//height used up by nose
		const auto noseProtrudingHeight = size_t(0.1 * double(headMaxSize));
		//Y coordinate where nose starts
		m_noseY = size_t((m_headWindowHeight - headMaxSize) / 2);
		//skull diameter
		m_skullDiameter = headMaxSize - noseProtrudingHeight;
		//skull upper left corner
		m_skullX = (m_headWindowWidth - m_skullDiameter) / 2;
		m_skullY = m_noseY + noseProtrudingHeight;

		//skull outline and filled area start/end angles
		m_skullOutlineStartAngle = 0;
		m_skullOutlineEndAngle   = 360;
		m_skullFillStartAngle    = 0;
		m_skullFillEndAngle      = 360;

		//clip mask
		m_clipmaskWidth  = m_skullDiameter;
		m_clipmaskHeight = m_skullDiameter;
	}
	else if (m_currentView == EView::Back) {
		//FIXME take into account width used up by ears

		//height used up by neck
		const size_t neckProtrudingHeight = size_t(0.072 * double(headMaxSize));

		//skull diameter
		m_skullDiameter = headMaxSize - neckProtrudingHeight;
		//skull upper left corner
		m_skullX = (m_headWindowWidth - m_skullDiameter) / 2;
		m_skullY = (m_headWindowHeight - headMaxSize) / 2;

		//skull outline and filled area start/end angles
		m_skullOutlineStartAngle = -38;
		m_skullOutlineEndAngle   = 180 - m_skullOutlineStartAngle;
		m_skullFillStartAngle    = -30;
		m_skullFillEndAngle      = 180 - m_skullFillStartAngle;

		const size_t skullRadius  = m_skullDiameter / 2;
		const size_t skullCenterX = m_skullX + skullRadius;
		const size_t skullCenterY = m_skullY + skullRadius;

		m_skullOutlineLeftPointX  = skullCenterX + size_t(1.0 * double(skullRadius) * cos(Deg2Rad(m_skullOutlineEndAngle)));
		m_skullOutlineLeftPointY  = skullCenterY - size_t(1.0 * double(skullRadius) * sin(Deg2Rad(m_skullOutlineEndAngle)));
		m_skullOutlineRightPointX = skullCenterX + size_t(1.0 * double(skullRadius) * cos(Deg2Rad(m_skullOutlineStartAngle)));
		m_skullOutlineRightPointY = skullCenterY - size_t(1.0 * double(skullRadius) * sin(Deg2Rad(m_skullOutlineStartAngle)));

		m_skullFillLeftPointX   = skullCenterX + size_t(1.0 * double(skullRadius) * cos(Deg2Rad(m_skullFillEndAngle)));
		m_skullFillLeftPointY   = skullCenterY - size_t(1.0 * double(skullRadius) * sin(Deg2Rad(m_skullFillEndAngle)));
		m_skullFillRightPointX  = skullCenterX + size_t(1.0 * double(skullRadius) * cos(Deg2Rad(m_skullFillStartAngle)));
		m_skullFillRightPointY  = skullCenterY - size_t(1.0 * double(skullRadius) * sin(Deg2Rad(m_skullFillStartAngle)));
		m_skullFillBottomPointX = m_skullX + skullRadius;
		m_skullFillBottomPointY = m_skullFillRightPointY;

		//neck extremities
		m_leftNeckX  = m_skullOutlineLeftPointX + size_t(0.025 * double(m_skullDiameter));
		m_leftNeckY  = m_skullOutlineLeftPointY + neckProtrudingHeight;
		m_rightNeckX = m_skullOutlineRightPointX - size_t(0.025 * double(m_skullDiameter));
		m_rightNeckY = m_leftNeckY;

		//clip mask
		m_clipmaskWidth  = m_skullDiameter;
		m_clipmaskHeight = m_skullFillBottomPointY - m_skullY + 1;
	}
	else if (m_currentView == EView::Left || m_currentView == EView::Right) {
		//width used up by nose
		const auto noseProtrudingWidth = size_t(0.06 * double(m_skullDiameter));	//size_t(0.047 * m_skullDiameter);

		//skull diameter
		m_skullDiameter = headMaxSize - noseProtrudingWidth;

		//topmost skull coordinate
		m_skullY = (m_headWindowHeight - m_skullDiameter) / 2;

		if (m_currentView == EView::Left) {
			//X coordinate of nose tip
			m_noseTipX = (m_headWindowWidth - headMaxSize) / 2;
			//leftmost skull coordinate
			m_skullX = m_noseTipX + noseProtrudingWidth;
			//skull outline and filled area start/end angles
			m_skullOutlineStartAngle = -41;
			m_skullOutlineEndAngle   = 193;//194;
			m_skullFillStartAngle    = -22;
			m_skullFillEndAngle      = 188;

			const size_t skullRadius  = m_skullDiameter / 2;
			const size_t skullCenterX = m_skullX + skullRadius;
			const size_t skullCenterY = m_skullY + skullRadius;

			//nose top = head outline left boundary
			m_noseTopX = skullCenterX + size_t(double(skullRadius) * cos(Deg2Rad(m_skullOutlineEndAngle)));
			m_noseTopY = skullCenterY - size_t(double(skullRadius) * sin(Deg2Rad(m_skullOutlineEndAngle)));
			//nose bump
			m_noseBumpX = m_noseTipX;
			m_noseBumpY = m_noseTopY + size_t(0.15 * double(m_skullDiameter));//size_t(0.179f * m_skullDiameter);
			//nose tip
			//m_noseTipX = m_noseBumpX;
			m_noseTipY = m_noseBumpY + size_t(0.03 * double(m_skullDiameter));//size_t(0.021f * m_skullDiameter);
			//nose base
			m_noseBaseX = m_noseTipX + size_t(0.1 * double(m_skullDiameter));
			m_noseBaseY = m_noseTipY;
			//nose bottom
			m_noseBottomX = m_noseBaseX;
			m_noseBottomY = m_noseBaseY + size_t(0.02 * double(m_skullDiameter));//size_t(0.016f * m_skullDiameter);
		}
		else {
			//X coordinate of nose tip
			m_noseTipX = (m_headWindowWidth + headMaxSize) / 2;
			//leftmost skull coordinate
			m_skullX = (m_headWindowWidth - headMaxSize) / 2;
			//skull outline and filled area start/end angles
			m_skullOutlineStartAngle = -13; //-14;
			m_skullOutlineEndAngle   = 221;
			m_skullFillStartAngle    = -8;
			m_skullFillEndAngle      = 202;

			const size_t skullRadius  = m_skullDiameter / 2;
			const size_t skullCenterX = m_skullX + skullRadius;
			const size_t skullCenterY = m_skullY + skullRadius;

			//nose top = head outline right boundary
			m_noseTopX = skullCenterX + size_t(double(skullRadius) * cos(Deg2Rad(m_skullOutlineStartAngle)));
			m_noseTopY = skullCenterY - size_t(double(skullRadius) * sin(Deg2Rad(m_skullOutlineStartAngle)));
			//nose bump
			m_noseBumpX = m_noseTipX;
			m_noseBumpY = m_noseTopY + size_t(0.15 * double(m_skullDiameter));	//size_t(0.179f * m_skullDiameter);
			//nose tip
			//m_noseTipX = m_noseBumpX;
			m_noseTipY = m_noseBumpY + size_t(0.03 * double(m_skullDiameter));	//size_t(0.021f * m_skullDiameter);
			//nose base
			m_noseBaseX = m_noseTipX - size_t(0.1 * double(m_skullDiameter));
			m_noseBaseY = m_noseTipY;
			//nose bottom
			m_noseBottomX = m_noseBaseX;
			m_noseBottomY = m_noseBaseY + size_t(0.02 * double(m_skullDiameter));	//size_t(0.016f * m_skullDiameter);
		}

		const size_t skullRadius  = m_skullDiameter / 2;
		const size_t skullCenterX = m_skullX + skullRadius;
		const size_t skullCenterY = m_skullY + skullRadius;
		m_skullFillLeftPointX     = skullCenterX + size_t(double(skullRadius) * cos(Deg2Rad(m_skullFillEndAngle)));
		m_skullFillLeftPointY     = skullCenterY - size_t(double(skullRadius) * sin(Deg2Rad(m_skullFillEndAngle)));
		m_skullFillRightPointX    = skullCenterX + size_t(double(skullRadius) * cos(Deg2Rad(m_skullFillStartAngle)));
		m_skullFillRightPointY    = skullCenterY - size_t(double(skullRadius) * sin(Deg2Rad(m_skullFillStartAngle)));

		m_skullFillBottomPointX = m_skullX + skullRadius;
		m_skullFillBottomPointY = m_skullY + size_t(0.684 * double(m_skullDiameter));

		//clip mask
		m_clipmaskWidth  = m_skullDiameter;
		m_clipmaskHeight = m_skullFillBottomPointY - m_skullY + 1;
	}

	//free existing clipmask, if any
	if (m_clipmaskGC != nullptr) { g_object_unref(m_clipmaskGC); }
	if (m_clipmask != nullptr) { g_object_unref(m_clipmask); }

	//allocate clipmask
	m_clipmask   = gdk_pixmap_new(m_drawingArea->window, gint(m_clipmaskWidth), gint(m_clipmaskHeight), 1);
	m_clipmaskGC = gdk_gc_new(GDK_DRAWABLE(m_clipmask));
	gdk_gc_set_colormap(m_clipmaskGC, gdk_gc_get_colormap(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)]));

	//redraw it
	redrawClipmask();

	//allocate main pixmap
	//TODO!

	//allocate RGB pixmap

	m_skullRGBBuffer.clear();

	//align lines on 32bit boundaries
	m_rowStride = ((m_skullDiameter * 3) % 4 == 0) ? (m_skullDiameter * 3) : ((((m_skullDiameter * 3) >> 2) + 1) << 2);
	m_skullRGBBuffer.resize(m_rowStride * m_skullDiameter);

	//determine size of colored cells
#if 1
	const size_t cellMinSize            = 6;
	const size_t cellMaxSize            = 6;
	const double cellOverSkullSizeRatio = 0.02;
	m_cellSize                          = size_t(double(m_skullDiameter) * cellOverSkullSizeRatio);

	if (m_cellSize < cellMinSize) { m_cellSize = cellMinSize; }
	else if (m_cellSize > cellMaxSize) { m_cellSize = cellMaxSize; }
#else
			m_cellSize = m_skullDiameter / 2;
#endif
	if (m_cellSize == 0) { return; }

	//number of samples in a row or column
	m_gridSize = size_t(ceil(double(m_skullDiameter) / double(m_cellSize)));

	//determine number of samples lying within skull
	const size_t nSamples = computeSamplesNormalizedCoordinates(false);

	//resize sample grids accordingly
	m_sample2DCoordinates.resize(nSamples);
	m_sampleCoordinatesMatrix.resize(nSamples, 3);
	m_sampleValues.resize(nSamples);

	//compute samples normalized coordinates
	computeSamplesNormalizedCoordinates(true);

	//resizing completed
	m_needResize = false;
}

void CTopographicMap2DView::redrawClipmask()
{
	//clear clipmask by drawing a black rectangle
	GdkColor black;
	black.red = black.green = black.blue = 0;
	gdk_gc_set_rgb_fg_color(m_clipmaskGC, &black);
	gdk_draw_rectangle(m_clipmask, m_clipmaskGC, TRUE, 0, 0, gint(m_clipmaskWidth), gint(m_clipmaskHeight));

	//draw visible circular region with a white filled arc
	GdkColor white;
	white.red = white.green = white.blue = 65535;
	gdk_gc_set_rgb_fg_color(m_clipmaskGC, &white);
	gdk_draw_arc(m_clipmask, m_clipmaskGC, TRUE, 0, 0, gint(m_skullDiameter), gint(m_skullDiameter), gint(64 * m_skullFillStartAngle),
				 gint(64 * (m_skullFillEndAngle - m_skullFillStartAngle)));

	//views other than top have an extra non-clipped area
	if (m_currentView == EView::Left || m_currentView == EView::Right || m_currentView == EView::Back) {
		//draw polygon : { skullCenter, skullFillStartPoint, skullFillBottomPoint, skullFillEndPoint, skullCenter }
		const std::array<GdkPoint, 4> polygon = {
			{
				{ gint(m_skullX + m_skullDiameter / 2 - m_skullX), gint(m_skullY + m_skullDiameter / 2 - m_skullY - 2) },
				{ gint(m_skullFillRightPointX - m_skullX), gint(m_skullFillRightPointY - m_skullY - 2) },
				{ gint(m_skullFillBottomPointX - m_skullX), gint(m_skullFillBottomPointY - m_skullY - 2) },
				{ gint(m_skullFillLeftPointX - m_skullX), gint(m_skullFillLeftPointY - m_skullY - 2) }
			}
		};
		gdk_draw_polygon(m_clipmask, m_clipmaskGC, TRUE, polygon.data(), 4);
	}

	//restore default black color
	gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &black);

	//update visible region
	if (m_visibleRegion != nullptr) { gdk_region_destroy(m_visibleRegion); }
	m_visibleRegion = gdk_drawable_get_visible_region(GDK_DRAWABLE(m_clipmask));
}

void CTopographicMap2DView::refreshPotentials() const
{
	size_t w, h;

#ifdef INTERPOLATE_AT_CHANNEL_LOCATION
	for (size_t i = size_t(m_mapDatabase.getChannelCount()); i < m_sampleValues.size(); ++i)
#else
	for (size_t i = 0; i < m_sampleValues.size(); ++i)
#endif
	{
		//cells of last row and last column may be smaller than other ones
		if (m_sample2DCoordinates[i].first + m_cellSize >= m_skullDiameter) { w = m_skullDiameter - m_sample2DCoordinates[i].first; }
		else { w = m_cellSize; }

		if (m_sample2DCoordinates[i].second + m_cellSize >= m_skullDiameter) { h = m_skullDiameter - m_sample2DCoordinates[i].second; }
		else { h = m_cellSize; }

		size_t index = m_sampleValues[i];
		if (index > 12) { index = 12; }

		drawBoxToBuffer(m_sample2DCoordinates[i].first, m_sample2DCoordinates[i].second, w, h, palette8[3 * index], palette8[3 * index + 1],
						palette8[3 * index + 2]);
	}
}

void CTopographicMap2DView::drawPotentials() const
{
	gdk_draw_rgb_image(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], gint(m_skullX), gint(m_skullY),
					   gint(m_skullDiameter), gint(m_skullDiameter), GDK_RGB_DITHER_NONE, m_skullRGBBuffer.data(), gint(m_rowStride));
}

void CTopographicMap2DView::drawElectrodes() const
{
	if (!m_electrodesToggledOn) { return; }


#if 0
			//determine size of electrode rings
			const double electrodeRingOverSkullSizeRatio = 0.05;
			gint electrodeRingSize = gint(m_skullDiameter * electrodeRingOverSkullSizeRatio);
			if (electrodeRingSize < (gint)electrodeRingMinSize) { electrodeRingSize = (gint)electrodeRingMinSize; }
			else if (electrodeRingSize > (gint)electrodeRingMaxSize) { electrodeRingSize = (gint)electrodeRingMaxSize; }
			if (electrodeRingSize == 0) { return; }
#else
	const gint electrodeRingSize = 5;
#endif

	GdkColor white;
	white.red   = 65535;
	white.green = 65535;
	white.blue  = 65535;

	GdkColor black;
	black.red   = 0;
	black.green = 0;
	black.blue  = 0;

	//set electrode ring thickness
	const gint electrodeRingThickness = 1;
	gdk_gc_set_line_attributes(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], electrodeRingThickness, GDK_LINE_SOLID, GDK_CAP_BUTT,
							   GDK_JOIN_BEVEL);

	//electrode label
	CString electrodeLabel;
	PangoLayout* electrodeLabelLayout = gtk_widget_create_pango_layout(GTK_WIDGET(m_drawingArea), " ");
	gint textHeight, textWidth;
	pango_layout_get_pixel_size(electrodeLabelLayout, nullptr, &textHeight);

	//draw rings
	const size_t nChannel = size_t(m_mapDatabase.GetChannelCount());
	gint channelX, channelY;

	for (size_t i = 0; i < nChannel; ++i) {
		if (!getChannel2DPosition(i, channelX, channelY)) { continue; }

#ifdef INTERPOLATE_AT_CHANNEL_LOCATION
				//disk colored according to value interpolated at this channel location
				gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &PALETTE[m_sampleValues[i]]);
#else
		//fill ring with white
		gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &white);
#endif
		gdk_draw_arc(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], TRUE,
					 channelX - electrodeRingSize / 2, channelY - electrodeRingSize / 2, electrodeRingSize, electrodeRingSize, 0, 64 * 360);

		//ring centered on channel location
		gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &black);

		gdk_draw_arc(m_drawingArea->window,
					 m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], FALSE,
					 channelX - electrodeRingSize / 2, channelY - electrodeRingSize / 2,
					 electrodeRingSize, electrodeRingSize, 0, 64 * 360);

		//channel label
		gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &black/*&white*/);

		m_mapDatabase.GetChannelLabel(i, electrodeLabel);
		pango_layout_set_text(electrodeLabelLayout, electrodeLabel, int(strlen(electrodeLabel)));
		pango_layout_get_pixel_size(electrodeLabelLayout, &textWidth, nullptr);
		gdk_draw_layout(m_drawingArea->window, m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)],
						channelX - textWidth / 2, channelY - electrodeRingSize / 2 - textHeight - 5, electrodeLabelLayout);
	}

	//restore default line thickness
	gdk_gc_set_line_attributes(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

	//restore default black color
	gdk_gc_set_rgb_fg_color(m_drawingArea->style->fg_gc[GTK_WIDGET_STATE(m_drawingArea)], &black);

	//delete pango layout
	g_object_unref(electrodeLabelLayout);
}

bool CTopographicMap2DView::getChannel2DPosition(const size_t index, gint& x, gint& y) const
{
	const double skullRadius  = double(m_skullDiameter) / 2.0;
	const size_t skullCenterX = m_skullX + size_t(skullRadius);
	const size_t skullCenterY = m_skullY + size_t(skullRadius);
	//get normalized coordinates
	double* originaPosition;
	m_mapDatabase.GetChannelPosition(index, originaPosition);

	/* flip the eletrode positions in order to use the mensia coordinate system */

	const std::array<double, 3> position = { originaPosition[0], originaPosition[1], originaPosition[2] };

	if (m_currentView == EView::Top) {
		if (m_currentProjection == EProjection::Axial) {
			x = gint(double(skullCenterX) + position[0] * skullRadius);
			y = gint(double(skullCenterY) - position[1] * skullRadius);
		}
		else //radial
		{
			//compute back frame 2D coordinates
			const double theta = getThetaFromCartesianCoordinates(position);
			const double phi   = getPhiFromCartesianCoordinates(position);
			compute2DCoordinates(theta, phi, skullCenterX, skullCenterY, x, y);
		}
	}
	else if (m_currentView == EView::Back) {
		//if(electrodePosition[1] > 0) //electrode not visible
		if (position[1] > sin(1.0 / 90.0 * M_PI / 2.0)) { return false; }

		if (m_currentProjection == EProjection::Axial) {
			x = gint(double(skullCenterX) + position[0] * skullRadius);
			y = gint(double(skullCenterY) - position[2] * skullRadius);
		}
		else //radial
		{
			//transform coordinates from top frame to back frame
			const std::array<double, 3> backPosition = { position[0], position[2], -position[1] };
			//compute back frame 2D coordinates
			const double theta = getThetaFromCartesianCoordinates(backPosition);
			const double phi   = getPhiFromCartesianCoordinates(backPosition);
			compute2DCoordinates(theta, phi, skullCenterX, skullCenterY, x, y);
		}
	}
	else if (m_currentView == EView::Left) {
		//if(electrodePosition[0] > 0) //electrode not visible
		if (position[0] > cos(89.0 / 90.0 * M_PI / 2.0)) { return false; }

		if (m_currentProjection == EProjection::Axial) {
			x = gint(double(skullCenterX) - position[1] * skullRadius);
			y = gint(double(skullCenterY) - position[2] * skullRadius);
		}
		else //radial
		{
			//transform coordinates from top frame to left frame
			const std::array<double, 3> backPosition = { -position[1], position[2], -position[0] };
			//compute back frame 2D coordinates
			const double theta = getThetaFromCartesianCoordinates(backPosition);
			const double phi   = getPhiFromCartesianCoordinates(backPosition);
			compute2DCoordinates(theta, phi, skullCenterX, skullCenterY, x, y);
		}
	}
	else if (m_currentView == EView::Right) {
		//if(electrodePosition[0] < 0) //electrode not visible
		if (position[0] < -cos(89.0 / 90.0 * M_PI / 2.0)) { return false; }

		if (m_currentProjection == EProjection::Axial) {
			x = gint(double(skullCenterX) + position[1] * skullRadius);
			y = gint(double(skullCenterY) - position[2] * skullRadius);
		}
		else //radial
		{
			//transform coordinates from top frame to left frame
			const std::array<double, 3> backPosition = { position[1], position[2], position[0] };
			//compute back frame 2D coordinates
			const double theta = getThetaFromCartesianCoordinates(backPosition);
			const double phi   = getPhiFromCartesianCoordinates(backPosition);
			compute2DCoordinates(theta, phi, skullCenterX, skullCenterY, x, y);
		}
	}

	//make sure electrode is in the non clipped area of the display
	//TODO : perform this test once per view only!
	return gdk_region_point_in(m_visibleRegion, x - int(m_skullX), y - int(m_skullY)) != 0;
}

void CTopographicMap2DView::drawBoxToBuffer(const size_t x, const size_t y, const size_t width, const size_t height,
											const uint8_t red, const uint8_t green, const uint8_t blue) const
{
#ifdef TARGET_OS_Windows
#ifndef NDEBUG
	//m_skullRGBBuffer == m_rowStride*m_skullDiameter
	assert(x < m_skullDiameter);
	assert(y < m_skullDiameter);
	assert((m_rowStride * y) + (x * 3) + 2 < m_rowStride * m_skullDiameter);
#endif
#endif
	guchar* lineBase = const_cast<guchar*>(m_skullRGBBuffer.data()) + (m_rowStride * y) + (x * 3);

	for (size_t j = 0; j < height; ++j) {
		for (size_t i = 0; i < (width * 3); i += 3) {
			*(lineBase + i)     = red;
			*(lineBase + i + 1) = green;
			*(lineBase + i + 2) = blue;
		}

		lineBase += (m_rowStride);
	}
}

size_t CTopographicMap2DView::computeSamplesNormalizedCoordinates(const bool all)
{
	size_t curSample = 0;

#ifdef INTERPOLATE_AT_CHANNEL_LOCATION
			size_t nChannel = (size_t)m_topographicMapDatabase.getChannelCount();
			double* electrodePosition = nullptr;

			//sampling at electrode locations
			for (curSample = 0; curSample < nChannel; ++curSample)
			{
				m_topographicMapDatabase.getChannelPosition(curSample, electrodePosition);

				//dummy 2D coords - actual coords are computed when drawing electrode rings
				m_sample2DCoordinates[curSample].first = 0;
				m_sample2DCoordinates[curSample].second = 0;

				*(m_sampleCoordinatesMatrix.getBuffer() + 3 * curSample) = *electrodePosition;
				*(m_sampleCoordinatesMatrix.getBuffer() + 3 * curSample + 1) = *(electrodePosition + 1);
				*(m_sampleCoordinatesMatrix.getBuffer() + 3 * curSample + 2) = *(electrodePosition + 2);
			}
#endif

	//sampling over skull area
	const double skullRadius  = double(m_skullDiameter) / 2.0;
	const double skullCenterX = double(m_skullX) + skullRadius;
	const double skullCenterY = double(m_skullY) + skullRadius;
	double* buffer            = m_sampleCoordinatesMatrix.getBuffer();

	//for each row
	double curY = double(m_skullY);
	for (size_t i = 0; i < m_gridSize; ++i) {
		//for each column
		double curX = double(m_skullX);
		for (size_t j = 0; j < m_gridSize; ++j) {
			//find corner closest to skull center
			const double closestX = std::fabs(curX - skullCenterX) < std::fabs(curX + double(m_cellSize) - skullCenterX) ? curX : (curX + double(m_cellSize));
			const double closestY = std::fabs(curY - skullCenterY) < std::fabs(curY + double(m_cellSize) - skullCenterY) ? curY : (curY + double(m_cellSize));

			//make sure electrode is in the non clipped area of the display
			//TODO : perform this test once per view only!
			//ensure closest corner lies within "skull sphere"
			if ((closestX - skullCenterX) * (closestX - skullCenterX) + (closestY - skullCenterY) * (closestY - skullCenterY) <= (skullRadius * skullRadius)) {
				//ensure this point is in the non clipped skull area
				//FIXME : the previous test remains necessary to get rid of all points lying outside "skull sphere"... Bug in gdk_region_point_in()?
				if (gdk_region_point_in(m_visibleRegion, int(closestX - double(m_skullX)), int(closestY - double(m_skullY)))) {
					if (all) {
						m_sample2DCoordinates[curSample].first  = j * m_cellSize;
						m_sample2DCoordinates[curSample].second = i * m_cellSize;

						//compute normalized coordinates to be fed to spherical spline algorithm
						//----------------------------------------------------------------------
						const size_t baseIndex = 3 * curSample;

						//normalized X, Y coords in (X, Y) projection plane
						const double x = (closestX - skullCenterX) / skullRadius;
						const double y = -(closestY - skullCenterY) / skullRadius; // y axis down in 2D but up in 3D convention

						if (m_currentProjection == EProjection::Axial) {
							if (m_currentView == EView::Top) {
								*(buffer + baseIndex)     = x;
								*(buffer + baseIndex + 1) = y;
								//z = sqrt(1-x*x-y*y)
								const double squareXYSum  = x * x + y * y;
								*(buffer + baseIndex + 2) = (squareXYSum >= 1) ? 0 : sqrt(1 - squareXYSum);
							}
							else if (m_currentView == EView::Back) {
								*(buffer + baseIndex)     = x;
								*(buffer + baseIndex + 2) = y;
								//y = sqrt(1-x*x-z*z)
								const double squareXYSum  = x * x + y * y;
								*(buffer + baseIndex + 1) = (squareXYSum >= 1) ? 0 : sqrt(1 - squareXYSum);
							}
							else if (m_currentView == EView::Left) {
								*(buffer + baseIndex + 1) = -x;
								*(buffer + baseIndex + 2) = y;
								//x = sqrt(1-y*y-z*z)
								const double squareXYSum = x * x + y * y;
								*(buffer + baseIndex)    = (squareXYSum >= 1) ? 0 : sqrt(1 - squareXYSum);
							}
							else if (m_currentView == EView::Right) {
								*(buffer + baseIndex + 1) = x;
								*(buffer + baseIndex + 2) = y;
								//x = sqrt(1-y*y-z*z)
								const double squareXYSum = x * x + y * y;
								*(buffer + baseIndex)    = (squareXYSum >= 1) ? 0 : sqrt(1 - squareXYSum);
							}
						}
						else //radial
						{
							//theta = (X,Y) arc length
							const double theta         = double(M_PI / 2 * sqrt(x * x + y * y));
							const double scalingFactor = (theta <= 1e-3) ? 0 : (sin(theta) / theta);
							//x = sin(theta) / theta * X, y = sin(theta) / theta * Y, z = cos(theta)
							const std::array<double, 3> sampleLocalCoordinates = {
								scalingFactor * x * (M_PI / 2.0), scalingFactor * y * (M_PI / 2.0), cos(theta)
							};

							if (m_currentView == EView::Top) {
								*(buffer + baseIndex)     = sampleLocalCoordinates[0];
								*(buffer + baseIndex + 1) = sampleLocalCoordinates[1];
								*(buffer + baseIndex + 2) = sampleLocalCoordinates[2];
							}
							else if (m_currentView == EView::Back) {
								*(buffer + baseIndex)     = sampleLocalCoordinates[0];
								*(buffer + baseIndex + 1) = -sampleLocalCoordinates[2];
								*(buffer + baseIndex + 2) = sampleLocalCoordinates[1];
							}
							else if (m_currentView == EView::Left) {
								*(buffer + baseIndex)     = -sampleLocalCoordinates[2];
								*(buffer + baseIndex + 1) = -sampleLocalCoordinates[0];
								*(buffer + baseIndex + 2) = sampleLocalCoordinates[1];
							}
							else if (m_currentView == EView::Right) {
								*(buffer + baseIndex)     = sampleLocalCoordinates[2];
								*(buffer + baseIndex + 1) = sampleLocalCoordinates[0];
								*(buffer + baseIndex + 2) = sampleLocalCoordinates[1];
							}
						}
					}

					curSample++;
				} //point in non clipped area
			} //point in "skull sphere"
			curX += double(m_cellSize);
		}
		curY += double(m_cellSize);
	}

	return curSample;
}

void CTopographicMap2DView::enableElectrodeButtonSignals(const bool enable)
{
	if (enable) { g_signal_connect(G_OBJECT(m_electrodesToggleButton), "toggled", G_CALLBACK(toggleElectrodesCallback), this); }
	else { g_signal_handlers_disconnect_by_func(G_OBJECT(m_electrodesToggleButton), reinterpret_cast<void*>(G_CALLBACK(toggleElectrodesCallback)), this); }
}

void CTopographicMap2DView::enableProjectionButtonSignals(const bool enable)
{
	if (enable) {
		g_signal_connect(G_OBJECT(m_axialProjectionButton), "toggled", G_CALLBACK(setProjectionCallback), this);
		g_signal_connect(G_OBJECT(m_radialProjectionButton), "toggled", G_CALLBACK(setProjectionCallback), this);
	}
	else {
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_axialProjectionButton), reinterpret_cast<void*>(G_CALLBACK(setProjectionCallback)), this);
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_radialProjectionButton), reinterpret_cast<void*>(G_CALLBACK(setProjectionCallback)), this);
	}
}

void CTopographicMap2DView::enableViewButtonSignals(const bool enable)
{
	if (enable) {
		g_signal_connect(G_OBJECT(m_topViewButton), "toggled", G_CALLBACK(setViewCallback), this);
		g_signal_connect(G_OBJECT(m_leftViewButton), "toggled", G_CALLBACK(setViewCallback), this);
		g_signal_connect(G_OBJECT(m_rightViewButton), "toggled", G_CALLBACK(setViewCallback), this);
		g_signal_connect(G_OBJECT(m_backViewButton), "toggled", G_CALLBACK(setViewCallback), this);
	}
	else {
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_topViewButton), reinterpret_cast<void*>(G_CALLBACK(setViewCallback)), this);
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_leftViewButton), reinterpret_cast<void*>(G_CALLBACK(setViewCallback)), this);
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_rightViewButton), reinterpret_cast<void*>(G_CALLBACK(setViewCallback)), this);
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_backViewButton), reinterpret_cast<void*>(G_CALLBACK(setViewCallback)), this);
	}
}

void CTopographicMap2DView::enableInterpolationButtonSignals(const bool enable)
{
	if (enable) {
		g_signal_connect(G_OBJECT(m_mapPotentials), "toggled", G_CALLBACK(setInterpolationCallback), this);
		g_signal_connect(G_OBJECT(m_mapCurrents), "toggled", G_CALLBACK(setInterpolationCallback), this);
	}
	else {
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_mapPotentials), reinterpret_cast<void*>(G_CALLBACK(setInterpolationCallback)), this);
		g_signal_handlers_disconnect_by_func(G_OBJECT(m_mapCurrents), reinterpret_cast<void*>(G_CALLBACK(setInterpolationCallback)), this);
	}
}

double CTopographicMap2DView::getThetaFromCartesianCoordinates(const std::array<double, 3>& cartesian) const { return acos(cartesian[2]); }

double CTopographicMap2DView::getPhiFromCartesianCoordinates(const std::array<double, 3>& cartesian) const
{
	double phi;
	if (cartesian[0] > 0.001) {
		phi = atan(cartesian[1] / cartesian[0]);
		if (phi < 0) { phi += 2 * M_PI; }
	}
	else if (cartesian[0] < -0.001) { phi = atan(cartesian[1] / cartesian[0]) + M_PI; }
	else { phi = cartesian[1] > 0 ? (M_PI / 2) : (3 * M_PI / 2); }

	return phi;
}

bool CTopographicMap2DView::compute2DCoordinates(const double theta, const double phi, const size_t skullCenterX, const size_t skullCenterY, gint& x,
												 gint& y) const
{
	//linear plotting along radius
	const double length = theta / (M_PI / 2.0) * double(m_skullDiameter) / 2.0;
	//determine coordinates on unit circle
	const double x1 = cos(phi);
	const double y1 = sin(phi);
	//scale vector so that it is length long
	x = gint(double(skullCenterX) + length * x1);
	y = gint(double(skullCenterY) - length * y1);
	return true;
}

//CALLBACKS

gboolean redrawCallback(GtkWidget* /*widget*/, GdkEventExpose* /*event*/, gpointer data)
{
	reinterpret_cast<CTopographicMap2DView*>(data)->Redraw();
	return TRUE;
}

gboolean resizeCallback(GtkWidget* /*pWidget*/, GtkAllocation* allocation, gpointer data)
{
	reinterpret_cast<CTopographicMap2DView*>(data)->ResizeCB(allocation->width, allocation->height);
	return FALSE;
}

void toggleElectrodesCallback(GtkWidget* /*pWidget*/, gpointer data)
{
	auto* view = reinterpret_cast<CTopographicMap2DView*>(data);
	view->ToggleElectrodesCB();
}

void setProjectionCallback(GtkWidget* widget, gpointer data)
{
	auto* view = reinterpret_cast<CTopographicMap2DView*>(data);
	view->SetProjectionCB(widget);
}

void setViewCallback(GtkWidget* widget, gpointer data)
{
	auto* view = reinterpret_cast<CTopographicMap2DView*>(data);
	view->SetViewCB(widget);
}

void setInterpolationCallback(GtkWidget* widget, gpointer data)
{
	auto* view = reinterpret_cast<CTopographicMap2DView*>(data);
	view->SetInterpolationCB(widget);
}

void setDelayCallback(GtkRange* range, gpointer data)
{
	auto* view = reinterpret_cast<CTopographicMap2DView*>(data);
	view->SetDelayCB(gtk_range_get_value(range));
}

}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
