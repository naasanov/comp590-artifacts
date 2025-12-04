#include "CAlgorithmSnapshotGen.hpp"

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

namespace OpenViBE {
namespace PluginInspector {

// ------------------------------------------------------------------------------------------------------------------------------------
CAlgorithmSnapshotGen::CAlgorithmSnapshotGen(const Kernel::IKernelContext& ctx, const std::string& snapshotDir, const std::string& docTemplateDir)
	: CPluginObjectDescEnum(ctx), m_snapshotDir(snapshotDir), m_docTemplateDir(docTemplateDir),
	  m_window(gtk_window_new(GTK_WINDOW_TOPLEVEL)), m_widget(gtk_drawing_area_new())
{
#define GDK_COLOR_SET(c, r, g, b) { c.pixel=0; c.red=r; c.green=g; c.blue=b; }
	GDK_COLOR_SET(m_colors[EColors::BackgroundPlayerStarted], 32767, 32767, 32767);
	GDK_COLOR_SET(m_colors[EColors::BoxBackgroundSelected], 65535, 65535, 49151);
	GDK_COLOR_SET(m_colors[EColors::BoxBackgroundMissing], 49151, 32767, 32767);
	GDK_COLOR_SET(m_colors[EColors::BoxBackgroundObsolete], 32767, 49151, 49151);
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

CAlgorithmSnapshotGen::~CAlgorithmSnapshotGen()
{
	std::ofstream file;
	file.open((m_docTemplateDir + "/Doc_Algorithms.dox").c_str());
	file << "/**\n * \\page Doc_Algorithms Algorithms list\n\n * Available algorithms :\n";

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

			auto it1 = lastSplittedCat.size() < splittedCat.size()
						   ? std::mismatch(lastSplittedCat.begin(), lastSplittedCat.end(), splittedCat.begin()).second
						   : std::mismatch(splittedCat.begin(), splittedCat.end(), lastSplittedCat.begin()).first;

			for (; it1 != splittedCat.end(); ++it1) { file << " * " << std::string((it1 - splittedCat.begin()) * 3, ' ') << " - " << *it1 << " : \n"; }

			lastCat         = cat;
			lastSplittedCat = splittedCat;
		}

		file << " * " << std::string(3 * (level + 1), ' ') << " - \\subpage Doc_algorithm_" << Transform(cat + "/" + name) << " \"" << name << "\"\n";
	}

	file << " */\n";
	file.close();
}

bool CAlgorithmSnapshotGen::Callback(const Plugins::IPluginObjectDesc& pod)
{
	const std::string fullName = std::string(pod.getCategory().toASCIIString()) + "/" + pod.getName().toASCIIString();
	const std::string filename = m_snapshotDir + "/Algorithm_" + Transform(fullName);
	CIdentifier id;
	if ((id = m_kernelCtx.getAlgorithmManager().createAlgorithm(pod.getCreatedClassIdentifier())) == CIdentifier::undefined()) {
		m_kernelCtx.getLogManager() << Kernel::LogLevel_Warning << "Skipped [" << filename << "] (could not create corresponding algorithm)\n";
		return true;
	}
	Kernel::IAlgorithmProxy& algorithm = m_kernelCtx.getAlgorithmManager().getAlgorithm(id);

	m_kernelCtx.getLogManager() << Kernel::LogLevel_Trace << "Working on [" << filename << "]\n";

	PangoContext* ctx   = gtk_widget_get_pango_context(m_widget);
	PangoLayout* layout = pango_layout_new(ctx);
	PangoRectangle rectangle;

	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_text(layout, pod.getName(), -1);
	pango_layout_get_pixel_extents(layout, nullptr, &rectangle);

	const int marginX      = 5;
	const int marginY      = 5;
	const int circleMargin = 5;
	const int circleSize   = 11;
	const int circleSpace  = 4;

	const int w = rectangle.width + marginX * 2;
	const int h = rectangle.height + marginY * 2;
	const int x = 16;
	const int y = 16;

	gdk_window_invalidate_rect(m_widget->window, nullptr, true);

	while (gtk_events_pending()) { gtk_main_iteration(); }

	GdkGC* drawGC = gdk_gc_new(m_widget->window);

	gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxBackground]);
	gdk_draw_rectangle(m_widget->window, drawGC, TRUE, x, y, w, h);
	gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxBorder]);
	gdk_draw_rectangle(m_widget->window, drawGC, FALSE, x, y, w, h);

	size_t i = 0;
	while ((id = algorithm.getNextInputParameterIdentifier(id)) != CIdentifier::undefined()) {
		GdkColor color = m_colors[EColors(algorithm.getInputParameter(id)->getType())];

		std::array<GdkPoint, 4> points;
		points[0].x = circleSize >> 1;
		points[0].y = circleSize;
		points[1].x = 0;
		points[1].y = 0;
		points[2].x = circleSize - 1;
		points[2].y = 0;
		for (int j = 0; j < 3; ++j) {
			points[j].x += gint(x + circleMargin + i * (circleSpace + circleSize));
			points[j].y += gint(y - (circleSize >> 1));
		}

		gdk_gc_set_rgb_fg_color(drawGC, &color);
		gdk_draw_polygon(m_widget->window, drawGC, TRUE, points.data(), 3);
		gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxInputBorder]);
		gdk_draw_polygon(m_widget->window, drawGC, FALSE, points.data(), 3);

		i++;
	}

	i = 0;
	while ((id = algorithm.getNextOutputParameterIdentifier(id)) != CIdentifier::undefined()) {
		GdkColor color = m_colors[EColors(algorithm.getOutputParameter(id)->getType())];

		std::array<GdkPoint, 4> points;
		points[0].x = circleSize >> 1;
		points[0].y = circleSize;
		points[1].x = 0;
		points[1].y = 0;
		points[2].x = circleSize - 1;
		points[2].y = 0;
		for (int j = 0; j < 3; ++j) {
			points[j].x += gint(x + circleMargin + i * (circleSpace + circleSize));
			points[j].y += gint(y - (circleSize >> 1) + h);
		}

		gdk_gc_set_rgb_fg_color(drawGC, &color);
		gdk_draw_polygon(m_widget->window, drawGC, TRUE, points.data(), 3);
		gdk_gc_set_rgb_fg_color(drawGC, &m_colors[EColors::BoxOutputBorder]);
		gdk_draw_polygon(m_widget->window, drawGC, FALSE, points.data(), 3);

		i++;
	}

	gdk_draw_layout(m_widget->window, m_widget->style->text_gc[GTK_WIDGET_STATE(m_widget)], x + marginX, y + marginY, layout);
	g_object_unref(layout);
	g_object_unref(drawGC);

	GdkPixbuf* buf = gdk_pixbuf_get_from_drawable(nullptr, m_widget->window, nullptr, 0, 0, 0, 0, w + 32, h + 32);
	OV_WARNING_UNLESS_K(gdk_pixbuf_save(buf, (filename+".png").c_str(), "png", nullptr, nullptr), "Failed saving " << (filename+".png"));

	g_object_unref(buf);

	// --------------------------------------------------------------------------------------------------------------------

#if 0

	m_categories.push_back(pair < string, string >(pod.getCategory().toASCIIString(), pod.getName().toASCIIString()));

	std::ofstream file;
	file.open((filename+".dox").toASCIIString());

	file << "/**\n"
		<< " * \\page Doc_" << filename << " " << fullName.toASCIIString() << "\n"
		<< " * \\section Doc_" << filename << "_summary Summary\n"
		<< " * \\image html " << filename << ".png\n *\n"
		<< " * - Plugin name : " << pod.getName() << "\n"
		<< " * - Version : " << pod.getVersion() << "\n"
		<< " * - Author : " << pod.getAuthorName() << "\n"
		<< " * - Company : " << pod.getAuthorCompanyName() << "\n"
		<< " * - Short description : " << pod.getShortDescription() << "\n"
		<< " * - Documentation template generation date : " << __DATE__ << "\n *\n"
		<< " * \\section Doc_" << filename << "_description Description\n"
		<< " * " << pod.getDetailedDescription() << " TODO\n *\n";

	file << " * \\section Doc_" << filename << "_inputs Inputs\n"
		<< " * Number of inputs : " << box.getInputCount() << "\n *\n";

	for (i = 0; i < box.getInputCount(); ++i)
	{
		CString name;
		CIdentifier typeID;
		box.getInputName(i, name);
		box.getInputType(i, typeID);
		file << " * - Input " << i+1 << "\n"
			<< " *  - Name : " << name.toASCIIString() << "\n"
			<< " *  - Type identifier : " << m_kernelCtx.getTypeManager().getTypeName(typeID).toASCIIString() << " " << typeID.str() << "\n"
			<< " *  - Description : TODO\n *\n";
	}

	file << " * \\section Doc_" << filename << "_outputs Outputs\n"
		<< " * Number of outputs : " << box.getOutputCount() << "\n *\n";

	for (i = 0; i < box.getOutputCount(); ++i)
	{
		CString name;
		CIdentifier typeID;
		box.getOutputName(i, name);
		box.getOutputType(i, typeID);
		file
			<< " * - Output " << i+1 << "\n"
			<< " *  - Name : " << name.toASCIIString() << "\n"
			<< " *  - Type identifier : " << m_kernelCtx.getTypeManager().getTypeName(typeID).toASCIIString() << " " << typeID.str() << "\n"
			<< " *  - Description : TODO\n"
			<< " *\n";
	}

	file << " * \\section Doc_" << filename << "_settings Settings\n"
		<< " * Number of settings : " << box.getSettingCount() << "\n *\n";
	
	for (i = 0; i < box.getSettingCount(); ++i)
	{
		CString name;
		CIdentifier typeID;
		CString defaultValue;
		box.getSettingName(i, name);
		box.getSettingType(i, typeID);
		box.getSettingDefaultValue(i, defaultValue);
		file
			<< " * - Setting " << i+1 << "\n"
			<< " *  - Name : " << name.toASCIIString() << "\n"
			<< " *  - Type identifier : " << m_kernelCtx.getTypeManager().getTypeName(typeID).toASCIIString() << " " << typeID.str() << "\n"
			<< " *  - Default value : " << defaultValue.toASCIIString() << "\n"
			<< " *  - Description : TODO\n *\n";
	}

	file << " * \\section Doc_" << filename << "_examples Examples\n * TODO\n *\n"
		<< " *  \\section Doc_" << filename << "_misc Miscellaneous\n * TODO\n */";

	file.close();

#endif

	m_kernelCtx.getAlgorithmManager().releaseAlgorithm(algorithm);

	return true;
}

}  // namespace PluginInspector
}  // namespace OpenViBE
