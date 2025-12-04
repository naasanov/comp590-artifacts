///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalDisplayLeftRuler.hpp
/// \brief Definition for the class CSignalDisplayLeftRuler.
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
#include <gtk/gtk.h>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
/// <summary> Used to display a vertical "amplitude" ruler. </summary>
class CSignalDisplayLeftRuler
{
public:
	/// <summary> Constructor. </summary>
	/// <param name="width"> Width to be requested by widget. </param>
	/// <param name="height"> Height to be requested by widget. </param>
	CSignalDisplayLeftRuler(int width, int height);
	//	CSignalChannelDisplay* pParentChannelDisplay);

	/// <summary> Destructor. </summary>
	~CSignalDisplayLeftRuler() = default;

	/// <summary> Update ruler with latest min/max values and request a redraw. </summary>
	/// <param name="min"> Minimum value to be displayed. </param>
	/// <param name="max"> Maximum value to be displayed. </param>
	void Update(double min, double max);

	//! returns the widget, so it can be added to the main interface
	GtkWidget* GetWidget() const { return m_Ruler; }

	/// <summary> Draws the ruler by using the information from the database. </summary>
	void Draw() const;

	GtkWidget* m_Ruler         = nullptr;
	int m_Width                = 0;
	double m_MaxDisplayedValue = -DBL_MAX;
	double m_MinDisplayedValue = DBL_MAX;
	uint64_t m_PixelsPerLabel  = 10;
	// CSignalChannelDisplay* m_pParentChannelDisplay = nullptr;
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
