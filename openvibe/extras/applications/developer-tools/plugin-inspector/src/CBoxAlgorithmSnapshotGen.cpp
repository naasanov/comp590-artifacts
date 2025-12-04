#include "CBoxAlgorithmSnapshotGen.hpp"

#include <system/ovCTime.h>

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <pango/pango.h>
#include <array>

// ------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------

namespace OpenViBE {
namespace PluginInspector {

CBoxAlgorithmSnapshotGen::CBoxAlgorithmSnapshotGen(const Kernel::IKernelContext& ctx, const std::string& snapshotDir, const std::string& docTemplateDir)
	: CPluginObjectDescEnum(ctx), m_snapshotDir(snapshotDir), m_docTemplateDir(docTemplateDir),
	  m_window(gtk_window_new(GTK_WINDOW_TOPLEVEL)), m_widget(gtk_drawing_area_new())
{
	m_kernelCtx.getScenarioManager().createScenario(m_scenarioID);
	m_scenario = &m_kernelCtx.getScenarioManager().getScenario(m_scenarioID);

#define GDK_COLOR_SET(c, r, g, b) { c.pixel = 0; c.red = r; c.green = g; c.blue = b; }
	GDK_COLOR_SET(m_colors[EColors::BackgroundPlayerStarted], 32767, 32767, 32767);
	GDK_COLOR_SET(m_colors[EColors::BoxBackgroundSelected], 65535, 65535, 49151);
	GDK_COLOR_SET(m_colors[EColors::BoxBackgroundMissing], 49151, 32767, 32767);
	GDK_COLOR_SET(m_colors[EColors::BoxBackgroundDeprecated], 16383, 24575, 24575);
	GDK_COLOR_SET(m_colors[EColors::BoxBackground], 65535, 65535, 65535);
	GDK_COLOR_SET(m_colors[EColors::BoxBorderSelected], 0, 0, 0);
	GDK_COLOR_SET(m_colors[EColors::BoxBorder], 0, 0, 0);
	GDK_COLOR_SET(m_colors[EColors::BoxInputBackground], 65535, 49151, 32767);
	GDK_COLOR_SET(m_colors[EColors::BoxInputBorder], 16383, 16383, 16383);
	GDK_COLOR_SET(m_colors[EColors::BoxOutputBackground], 32767, 65535, 49151);
	GDK_COLOR_SET(m_colors[EColors::BoxOutputBorder], 16383, 16383, 16383);
	GDK_COLOR_SET(m_colors[EColors::BoxSettingBackground], 49151, 32767, 65535);
	GDK_COLOR_SET(m_colors[EColors::BoxSettingBorder], 16383, 16383, 16383);
	GDK_COLOR_SET(m_colors[EColors::Link], 0, 0, 0);
	GDK_COLOR_SET(m_colors[EColors::LinkSelected], 49151, 16383, 16383);
	GDK_COLOR_SET(m_colors[EColors::SelectionArea], 0x3f00, 0x3f00, 0x3f00);
	GDK_COLOR_SET(m_colors[EColors::SelectionAreaBorder], 0, 0, 0);
#undef GDK_COLOR_SET

	gtk_container_add(GTK_CONTAINER(m_window), m_widget);
	gtk_widget_set_size_request(m_widget, 512, 128);
	gtk_widget_show_all(m_window);
	gdk_flush();
	System::Time::sleep(1000);
}

CBoxAlgorithmSnapshotGen::~CBoxAlgorithmSnapshotGen()
{
	m_kernelCtx.getScenarioManager().releaseScenario(m_scenarioID);

	std::ofstream file;
	file.open((m_docTemplateDir + "/Doc_BoxAlgorithms.dox").c_str());
	file << "/**\n * \\page Doc_BoxAlgorithms Box algorithms list\n *\n * Available box algorithms :\n";

	size_t level = 0;
	std::string lastCat;
	std::vector<std::string> lastSplittedCat;
	std::sort(m_categories.begin(), m_categories.end());
	for (const auto& category : m_categories) {
		std::string cat  = category.first;
		std::string name = category.second;

		if (lastCat != cat) {
			std::vector<std::string> splittedCat;
			size_t i      = size_t(-1);
			bool finished = false;
			while (!finished) {
				size_t j = cat.find('/', i + 1);
				if (j == std::string::npos) {
					j        = cat.length();
					finished = true;
				}
				if (j != i + 1) {
					splittedCat.push_back(cat.substr(i + 1, j - i - 1));
					i = j;
				}
			}
			level = splittedCat.size();
			// @FIXME C++14 : auto it1 = std::mismatch(lastSplittedCat.begin(), lastSplittedCat.end(), splittedCat.begin(), splittedCat.end()).second;
			auto it1 = lastSplittedCat.size() < splittedCat.size()
						   ? std::mismatch(lastSplittedCat.begin(), lastSplittedCat.end(), splittedCat.begin()).second
						   : std::mismatch(splittedCat.begin(), splittedCat.end(), lastSplittedCat.begin()).first;

			for (; it1 != splittedCat.end(); ++it1) { file << " * " << std::string((it1 - splittedCat.begin()) * 3, ' ') << " - " << *it1 << " : \n"; }

			lastCat         = cat;
			lastSplittedCat = splittedCat;
		}

		file << " * " << std::string(3 * level, ' ') << " - \\subpage Doc_BoxAlgorithm_" << Transform(name) << " \"" << name << "\"\n";
	}

	file << " */\n";
	file.close();
}

bool CBoxAlgorithmSnapshotGen::Callback(const Plugins::IPluginObjectDesc& pod)
{
	//string fullName = string(rPluginObjectDesc.getCategory().toASCIIString()) + "/" + string(rPluginObjectDesc.getName().toASCIIString());
	std::string filename = "BoxAlgorithm_" + Transform(pod.getName().toASCIIString());
	CIdentifier boxID;
	if (!m_scenario->addBox(boxID, pod.getCreatedClassIdentifier(), CIdentifier::undefined())) {
		OV_WARNING_K("Skipped [" << filename << "] (could not create corresponding box)");
		return true;
	}
	Kernel::IBox& box = *m_scenario->getBoxDetails(boxID);
	getLogManager() << Kernel::LogLevel_Trace << "Working on [" << filename << "]\n";

	PangoContext* ctx   = gtk_widget_get_pango_context(m_widget);
	PangoLayout* layout = pango_layout_new(ctx);
	PangoRectangle rectangle;

	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_text(layout, pod.getName(), -1);
	pango_layout_get_pixel_extents(layout, nullptr, &rectangle);

	const int marginX     = 5;
	const int marginY     = 5;
	const int circleSize  = 11;
	const int circleSpace = 4;

	int w = rectangle.width + marginX * 2;
	int h = rectangle.height + marginY * 2;
	int x = 16;
	int y = 16;

	gdk_window_invalidate_rect(m_widget->window, nullptr, true);

	while (gtk_events_pending()) { gtk_main_iteration(); }

	GdkGC* drawGC = gdk_gc_new(m_widget->window);

	bool deprecated = m_kernelCtx.getPluginManager().isPluginObjectFlaggedAsDeprecated(box.getAlgorithmClassIdentifier());
	if (deprecated) { gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxBackgroundDeprecated]); }
	else { gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxBackground]); }

	gdkDrawRoundedRectangle(m_widget->window, drawGC, TRUE, x, y, w, h);
	gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxBorder]);
	gdkDrawRoundedRectangle(m_widget->window, drawGC, FALSE, x, y, w, h);

	int offset = int(w - box.getInputCount() * (circleSpace + circleSize) + circleSize / 2) / 2;
	for (size_t i = 0; i < box.getInputCount(); ++i) {
		CIdentifier id;
		box.getInputType(i, id);
		GdkColor color = colorFromIdentifier(id);

		std::array<GdkPoint, 4> points;
		points[0].x = circleSize >> 1;
		points[0].y = circleSize;
		points[1].x = 0;
		points[1].y = 0;
		points[2].x = circleSize - 1;
		points[2].y = 0;
		for (size_t j = 0; j < 3; ++j) {
			points[j].x += gint(x + i * (circleSpace + circleSize) + offset);
			points[j].y += gint(y - (circleSize >> 1));
		}

		gdk_gc_set_rgb_fg_color(drawGC, &color);
		gdk_draw_polygon(m_widget->window, drawGC, TRUE, points.data(), 3);
		gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxInputBorder]);
		gdk_draw_polygon(m_widget->window, drawGC, FALSE, points.data(), 3);
	}

	offset = int(w - box.getOutputCount() * (circleSpace + circleSize) + circleSize / 2) / 2;
	for (size_t i = 0; i < box.getOutputCount(); ++i) {
		CIdentifier id;
		box.getOutputType(i, id);
		GdkColor color = colorFromIdentifier(id);

		std::array<GdkPoint, 4> points;
		points[0].x = circleSize >> 1;
		points[0].y = circleSize;
		points[1].x = 0;
		points[1].y = 0;
		points[2].x = circleSize - 1;
		points[2].y = 0;
		for (int j = 0; j < 3; ++j) {
			points[j].x += gint(x + i * (circleSpace + circleSize) + offset);
			points[j].y += gint(y - (circleSize >> 1) + h);
		}

		gdk_gc_set_rgb_fg_color(drawGC, &color);
		gdk_draw_polygon(m_widget->window, drawGC, TRUE, points.data(), 3);
		gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxOutputBorder]);
		gdk_draw_polygon(m_widget->window, drawGC, FALSE, points.data(), 3);
	}

	gdk_draw_layout(m_widget->window, m_widget->style->text_gc[GTK_WIDGET_STATE(m_widget)], x + marginX, y + marginY, layout);
	g_object_unref(layout);
	g_object_unref(drawGC);

	GdkPixbuf* buf = gdk_pixbuf_get_from_drawable(nullptr, m_widget->window, nullptr, 0, 0, 0, 0, w + 32, h + 32);
	gdk_pixbuf_save(buf, (m_snapshotDir + "/Doc_" + filename + ".png").c_str(), "png", nullptr, nullptr);

	g_object_unref(buf);

	// --------------------------------------------------------------------------------------------------------------------

	m_categories.push_back(std::pair<std::string, std::string>(pod.getCategory().toASCIIString(), pod.getName().toASCIIString()));

	std::ofstream fileSkeleton;
	std::ofstream filePart;
	fileSkeleton.open((m_docTemplateDir + "/Doc_" + filename + ".dox-skeleton").c_str());
	filePart.open((m_docTemplateDir + "/Doc_" + filename + ".dox-part-skeleton").c_str());

	fileSkeleton << "/**\n"
			<< " * \\page Doc_" << filename << " " << pod.getName() << "\n"
			<< " * \\section Doc_" << filename << "_Summary Summary\n"
			<< " * \\image html Doc_" << filename << ".png\n"
			<< " *\n"
			<< " * - Plugin name : " << pod.getName() << "\n"
			<< " * - Version : " << pod.getVersion() << "\n"
			<< " * - Author : " << pod.getAuthorName() << "\n"
			<< " * - Company : " << pod.getAuthorCompanyName() << "\n"
			<< " * - Short description : " << pod.getShortDescription() << "\n"
			<< " * - Documentation template generation date : " << __DATE__ << "\n";

	bool deprectated = m_kernelCtx.getPluginManager().isPluginObjectFlaggedAsDeprecated(pod.getCreatedClassIdentifier());
	if (deprectated) {
		fileSkeleton << " * - <b>WARNING : this box has been marked as DEPRECATED by the developer.</b>\n"
				<< " * It will be removed soon or later, so you should consider not using this\n"
				<< " * box and turn to another \"equivalent\" one.\n";
	}

	// @FIXME CERT
#pragma message("WARNING: 'Unstable' block commented out due to Certivibe")

	/*
	bool isUnstable=m_kernelCtx.getPluginManager().isPluginObjectFlaggedAsUnstable(rPluginObjectDesc.getCreatedClassIdentifier());
	if(isUnstable)
	{
		fileSkeleton
			<< " * - <b>WARNING : this box has been marked as UNSTABLE by the developer.\n"
			<< " * It means that its implementation may be incomplete or that the box can only work\n"
			<< " * under well known conditions. It may possibly crash or cause data loss.\n"
			<< " * Use this box at your own risk, you've been warned.</b>\n";
	}
	*/

	fileSkeleton << " *\n"
			<< " * \\section Doc_" << filename << "_Description Description\n"
			<< " * " << pod.getDetailedDescription() << "\n *\n"
			<< " * @Doc_" << filename << "_Description_Content@\n *\n";

	filePart << "/**\n"
			<< " * \\page " << filename << " " << pod.getName() << "\n"
			<< "__________________________________________________________________\n\n"
			<< "Detailed description\n"
			<< "__________________________________________________________________\n\n"
			<< " * |OVP_DocBegin_" << filename << "_Description|\n"
			<< " * |OVP_DocEnd_" << filename << "_Description|\n";

	if (box.getInputCount()) {
		fileSkeleton << " * \\section Doc_" << filename << "_Inputs Inputs\n"
				// << " * Number of inputs : " << box.getInputCount() << "\n"
				<< " *\n"
				<< " * @Doc_" << filename << "_Inputs_Content@\n";

		filePart << "__________________________________________________________________\n\n"
				<< "Inputs description\n"
				<< "__________________________________________________________________\n\n"
				<< " * |OVP_DocBegin_" << filename << "_Inputs|\n"
				<< " * |OVP_DocEnd_" << filename << "_Inputs|\n";

		for (size_t i = 0; i < box.getInputCount(); ++i) {
			CString name;
			CIdentifier typeID;
			CString typeName;
			box.getInputName(i, name);
			box.getInputType(i, typeID);
			typeName = m_kernelCtx.getTypeManager().getTypeName(typeID);

			fileSkeleton << " * \\subsection Doc_" << filename << "_Input_" << i + 1 << " " << i + 1 << ". " << name
					<< "\n * @Doc_" << filename << "_Input" << i + 1 << "_Content@\n *\n * - Type identifier : \\ref Doc_Streams_"
					<< Transform(typeName.toASCIIString()) << " \"" << typeName << "\" <em>" << typeID.str() << "</em>\n *\n";

			filePart << "\n * |OVP_DocBegin_" << filename << "_Input" << i + 1 << "|\n * |OVP_DocEnd_" << filename << "_Input" << i + 1 <<
					"|\n";
		}
	}

	if (box.getOutputCount()) {
		fileSkeleton << " * \\section Doc_" << filename << "_Outputs Outputs\n"
				// << " * Number of outputs : " << box.getOutputCount() << "\n"
				<< " *\n * @Doc_" << filename << "_Outputs_Content@\n";

		filePart << "__________________________________________________________________\n\n"
				<< "Outputs description\n"
				<< "__________________________________________________________________\n\n"
				<< " * |OVP_DocBegin_" << filename << "_Outputs|\n * |OVP_DocEnd_" << filename << "_Outputs|\n";

		for (size_t i = 0; i < box.getOutputCount(); ++i) {
			CString name;
			CIdentifier typeID;
			CString typeName;
			box.getOutputName(i, name);
			box.getOutputType(i, typeID);
			typeName = m_kernelCtx.getTypeManager().getTypeName(typeID);

			fileSkeleton
					<< " * \\subsection Doc_" << filename << "_Output_" << i + 1 << " " << i + 1 << ". " << name << "\n"
					<< " * @Doc_" << filename << "_Output" << i + 1 << "_Content@\n *\n * - Type identifier : \\ref Doc_Streams_"
					<< Transform(typeName.toASCIIString()) << " \"" << typeName << "\" <em>" << typeID.str() << "</em>\n *\n";

			filePart << "\n * |OVP_DocBegin_" << filename << "_Output" << i + 1 << "|\n * |OVP_DocEnd_" << filename << "_Output" << i + 1 <<
					"|\n";
		}
	}

	if (box.getSettingCount()) {
		fileSkeleton
				<< " * \\section Doc_" << filename << "_Settings Settings\n"
				// << " * Number of settings : " << box.getSettingCount() << "\n"
				<< " *\n * @Doc_" << filename << "_Settings_Content@\n";

		filePart
				<< "__________________________________________________________________\n\n"
				<< "Settings description\n"
				<< "__________________________________________________________________\n\n"
				<< " * |OVP_DocBegin_" << filename << "_Settings|\n * |OVP_DocEnd_" << filename << "_Settings|\n";

		for (size_t i = 0; i < box.getSettingCount(); ++i) {
			CString name;
			CIdentifier typeID;
			CString defaultValue;
			CString typeName;
			box.getSettingName(i, name);
			box.getSettingType(i, typeID);
			box.getSettingDefaultValue(i, defaultValue);
			typeName = m_kernelCtx.getTypeManager().getTypeName(typeID);

			fileSkeleton
					<< " * \\subsection Doc_" << filename << "_Setting_" << i + 1 << " " << i + 1 << ". " << name << "\n"
					<< " * @Doc_" << filename << "_Setting" << i + 1 << "_Content@\n"
					<< " *\n"
					// "\\ref Doc_Types_" << transform(typeName.toASCIIString())
					<< " * - Type identifier : <em> " << typeName << " " << typeID.str() << "</em>\n"
					<< " *\n"
					<< " * - Default value : [ <em>" << defaultValue << "</em> ]\n"
					<< " *\n";

			filePart << "\n * |OVP_DocBegin_" << filename << "_Setting" << i + 1 << "|\n * |OVP_DocEnd_" << filename << "_Setting" << i + 1 << "|\n";
		}
	}

	// @FIXME CERT
#pragma message("WARNING: PluginFunctionality_Visualization block commented out due to Certivibe")

	/*
	if(rPluginObjectDesc.hasFunctionality(PluginFunctionality_Visualization))
	{
		fileSkeleton
			<< " * \\section Doc_" << filename << "_OnlineVisualizationSettings Online visualization settings\n"
			<< " *\n"
			<< " * @Doc_" << filename << "_OnlineVisualizationSettings_Content@\n";

		filePart
			<< "__________________________________________________________________\n"
			<< "\n"
			<< "Online visualization settings\n"
			<< "__________________________________________________________________\n"
			<< "\n"
			<< " * |OVP_DocBegin_" << filename << "_OnlineVisualizationSettings|\n"
			<< " * |OVP_DocEnd_" << filename << "_OnlineVisualizationSettings|\n";
	}
	*/

	fileSkeleton
			<< " *\n"
			<< " * \\section Doc_" << filename << "_Examples Examples\n"
			<< " * @Doc_" << filename << "_Examples_Content@\n"
			<< " *\n"
			<< " *  \\section Doc_" << filename << "_Miscellaneous Miscellaneous\n"
			<< " * @Doc_" << filename << "_Miscellaneous_Content@\n"
			<< " */\n";

	filePart
			<< "__________________________________________________________________\n\n"
			<< "Examples description\n"
			<< "__________________________________________________________________\n\n"
			<< " * |OVP_DocBegin_" << filename << "_Examples|\n"
			<< " * |OVP_DocEnd_" << filename << "_Examples|\n"
			<< "__________________________________________________________________\n\n"
			<< "Miscellaneous description\n"
			<< "__________________________________________________________________\n\n"
			<< " * |OVP_DocBegin_" << filename << "_Miscellaneous|\n"
			<< " * |OVP_DocEnd_" << filename << "_Miscellaneous|\n"
			<< " */\n";

	fileSkeleton.close();
	filePart.close();

	return true;
}

}  // namespace PluginInspector
}  // namespace OpenViBE
