///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalDisplayLeftRuler.cpp
/// \brief Implementation for the class CSignalDisplayLeftRuler.
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

#include "CSignalDisplayLeftRuler.hpp"

#include <cmath>
#include <glib.h>
#include <glib/gprintf.h>
#include <iostream>
#include <sstream>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

//! Callback to redraw the bottom ruler
static gboolean LeftRulerExposeEventCallback(GtkWidget* /*widget*/, GdkEventExpose* /*event*/, gpointer data)
{
	//redraw the ruler
	const auto* leftRuler = reinterpret_cast<CSignalDisplayLeftRuler*>(data);
	leftRuler->Draw();
	//don't propagate this signal to the children if any
	return TRUE;
}


CSignalDisplayLeftRuler::CSignalDisplayLeftRuler(const int width, const int height) : m_Ruler(gtk_drawing_area_new()), m_Width(width)
{
	gtk_widget_set_size_request(m_Ruler, width, height);

	g_signal_connect_after(G_OBJECT(m_Ruler), "expose-event", G_CALLBACK(LeftRulerExposeEventCallback), this);

	//get left ruler widget's font description
	PangoContext* ctx                = gtk_widget_get_pango_context(m_Ruler);
	const PangoFontDescription* desc = pango_context_get_font_description(ctx);

	//adapt the allocated height per label to the font's height (plus 4 pixel to add some spacing)
	if (pango_font_description_get_size_is_absolute(desc)) { m_PixelsPerLabel = pango_font_description_get_size(desc) + 4; }
	else { m_PixelsPerLabel = pango_font_description_get_size(desc) / PANGO_SCALE + 4; }
}

void CSignalDisplayLeftRuler::Update(const double min, const double max)
{
	m_MaxDisplayedValue = max;
	m_MinDisplayedValue = min;

	//redraw the ruler
	if (m_Ruler->window) { gdk_window_invalidate_rect(m_Ruler->window, nullptr, true); }
}


void CSignalDisplayLeftRuler::Draw() const
{
	if (!GTK_WIDGET_VISIBLE(m_Ruler)) { return; }

	gint width, height;
	gdk_drawable_get_size(m_Ruler->window, &width, &height);

	//draw ruler base (vertical line)
	gdk_draw_line(m_Ruler->window, m_Ruler->style->fg_gc[GTK_WIDGET_STATE(m_Ruler)], width - 1, 0, width - 1, height);

	//computes the step in values for the ruler
	const double intervalWidth = m_MaxDisplayedValue - m_MinDisplayedValue;
	double valueStep           = 0;
	double baseValue           = 0;

	//if the signal is not constant
	if (intervalWidth > 0) {
		//computes the step
		const auto nearestSmallerPowerOf10 = double(pow(10, floor(log10(intervalWidth))));

		//get max number of labels that fit in widget
		const size_t maxNLabels = size_t(height / m_PixelsPerLabel);

		//ensure there is room for at least one label
		if (maxNLabels > 0) {
			//get the current number of labels to display based on the nearest inferior power of ten value
			const size_t tempNLabels = size_t(floor(intervalWidth / nearestSmallerPowerOf10));

			if (tempNLabels > 2 * maxNLabels) { valueStep = 4 * nearestSmallerPowerOf10; }
			else if (tempNLabels > maxNLabels) { valueStep = 2 * nearestSmallerPowerOf10; }
			else if (tempNLabels < (maxNLabels / 4)) { valueStep = nearestSmallerPowerOf10 / 4; }
			else if (tempNLabels < (maxNLabels / 2)) { valueStep = nearestSmallerPowerOf10 / 2; }
			else { valueStep = nearestSmallerPowerOf10; }

			//recompute base value of the step
			baseValue = valueStep * floor(m_MinDisplayedValue / valueStep);
		}
	}
	else {
		valueStep = 1;
		baseValue = floor(m_MinDisplayedValue - 0.5);
	}

	int textW;
	int textH;

	//if the step is too small, it causes problems, so don't display anything and return
	if (valueStep < 0.5e-5) { return; }
	double i = baseValue;
	while (i < double(0.5 + m_MaxDisplayedValue)) {
		//computes the coordinate of the current label
		const gint textY = gint((m_MaxDisplayedValue - i) * (height / intervalWidth));

		if (textY >= 0 && textY <= height) {
			gchar value[40];
			//if the current value is (almost) 0, displays 0
			abs(i) < 0.5e-10 ? g_sprintf(value, "0") : g_sprintf(value, "%g", i);

			PangoLayout* text = gtk_widget_create_pango_layout(m_Ruler, value);
			pango_layout_set_width(text, 28);
			pango_layout_set_justify(text, PANGO_ALIGN_RIGHT);

			pango_layout_get_pixel_size(text, &textW, &textH);

			gdk_draw_layout(m_Ruler->window, m_Ruler->style->fg_gc[GTK_WIDGET_STATE(m_Ruler)], 0, textY - (textH / 2), text);

			if (i < 0.5e-10 && i > -0.5e-10) {
				gdk_draw_line(m_Ruler->window, m_Ruler->style->fg_gc[GTK_WIDGET_STATE(m_Ruler)], width - 6, textY, width, textY);
			}
			else { gdk_draw_line(m_Ruler->window, m_Ruler->style->fg_gc[GTK_WIDGET_STATE(m_Ruler)], width - 4, textY, width, textY); }
		}
		i += valueStep;
	}
}
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
